#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace omg;
using namespace omg::dsp;

namespace
{
    inline float raw (juce::AudioProcessorValueTreeState& s, const char* id)
    {
        if (auto* v = s.getRawParameterValue (id))
            return v->load();
        jassertfalse;    // a UI control exists for a parameter that was never declared
        return 0.0f;
    }

    inline float rawEq (juce::AudioProcessorValueTreeState& s, int band, const char* field)
    {
        const auto id = eqId (band, field);
        if (auto* v = s.getRawParameterValue (id))
            return v->load();
        jassertfalse;
        return 0.0f;
    }
}

OmgnedProcessor::OmgnedProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",     juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output",    juce::AudioChannelSet::stereo(), true)
                          .withInput  ("Sidechain", juce::AudioChannelSet::stereo(), false)),
      apvts (*this, nullptr, "OMGNEDD", createLayout())
{
    presetManager.rescan();
}

OmgnedProcessor::~OmgnedProcessor() = default;

bool OmgnedProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();
    const auto& in  = layouts.getMainInputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    if (in != out)
        return false;

    // the sidechain is optional: disabled, mono or stereo are all fine
    if (layouts.inputBuses.size() > 1)
    {
        const auto& sc = layouts.getChannelSet (true, 1);
        if (! sc.isDisabled() && sc != juce::AudioChannelSet::mono() && sc != juce::AudioChannelSet::stereo())
            return false;
    }

    return true;
}

void OmgnedProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    currentBlockSize  = samplesPerBlock;

    // linear-phase FIR half-band filters: their latency is a constant delay,
    // so the dry path can be aligned with the engine path exactly
    for (int i = 0; i < 3; ++i)
    {
        oversamplers[(size_t) i] = std::make_unique<juce::dsp::Oversampling<float>> (
            2, i + 1, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, true);
        oversamplers[(size_t) i]->initProcessing ((size_t) samplesPerBlock);
    }

    juce::dsp::ProcessSpec base { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    eq.prepare (base);
    compressor.prepare (base);
    deEsser.prepare (base);
    outputStage.prepare (base);
    transient.prepare (base);
    modulation.prepare (base);
    filterFx.prepare (sampleRate, samplesPerBlock);
    space.prepare (sampleRate, samplesPerBlock);

    // the engines allocate for the fastest rate they could ever run at, then
    // applyEngineRate() tells them the rate they will actually run at
    const double maxRate = sampleRate * 8.0;
    underwater.prepare (maxRate, samplesPerBlock * 8);
    distortion.prepare (maxRate, samplesPerBlock * 8);
    saturation.prepare (maxRate, samplesPerBlock * 8);
    multiband.prepare  (maxRate, samplesPerBlock * 8);

    dryBuffer.setSize     (2, samplesPerBlock, false, false, true);
    sidechainCopy.setSize (2, samplesPerBlock, false, false, true);
    sideStash.setSize     (1, samplesPerBlock, false, false, true);
    osScratch.setSize     (2, samplesPerBlock * 8, false, false, true);
    xfadeScratch.setSize  (2, samplesPerBlock * 8, false, false, true);

    for (auto& d : dryAlign) d.allocate (1024);
    for (auto& d : bypassAlign) d.allocate (4096);
    sideAlign.allocate (1024);

    inGainSmooth.reset (sampleRate, 0.02, 1.0f);
    outGainSmooth.reset (sampleRate, 0.02, 1.0f);
    mixSmooth.reset (sampleRate, 0.02, 1.0f);
    dryLevelSmooth.reset (sampleRate, 0.02, 1.0f);
    wetLevelSmooth.reset (sampleRate, 0.02, 1.0f);

    inputMeter.prepare (sampleRate, 2);
    outputMeter.prepare (sampleRate, 2);
    spectrum.prepare (sampleRate);

    lastOsIndex = -1;
    currentEngine = previousEngine = -1;
    xfadeLeft = 0;
    underwaterWeight = (int) raw (apvts, pid::engine) == Underwater ? 1.0f : 0.0f;
}

void OmgnedProcessor::releaseResources()
{
    underwater.reset(); distortion.reset(); saturation.reset(); multiband.reset();
    eq.reset(); compressor.reset(); deEsser.reset(); outputStage.reset();
    modulation.reset(); transient.reset(); filterFx.reset(); space.reset();
    for (auto& o : oversamplers) if (o != nullptr) o->reset();
    for (auto& d : dryAlign) d.clear();
    for (auto& d : bypassAlign) d.clear();
    sideAlign.clear();
    driveMeterDb.store (-100.0f);
}

omg::dsp::TransportInfo OmgnedProcessor::readTransport()
{
    TransportInfo t;

    if (hasTransportOverride)
        return transportOverride;

    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
        {
            if (auto bpm = pos->getBpm())         t.bpm = juce::jlimit (20.0, 400.0, *bpm);
            if (auto ppq = pos->getPpqPosition()) t.ppqAtBlockStart = *ppq;
            t.playing = pos->getIsPlaying();
        }

    return t;
}

void OmgnedProcessor::pullParameters (const TransportInfo& transport)
{
    MacroEngine macros;
    macros.character = raw (apvts, pid::character);
    macros.body      = raw (apvts, pid::macBody);
    macros.color     = raw (apvts, pid::macColor);
    macros.damage    = raw (apvts, pid::macDamage);
    macros.depth     = raw (apvts, pid::macDepth);
    macros.motion    = raw (apvts, pid::macMotion);
    macros.space     = raw (apvts, pid::macSpace);
    macros.punch     = raw (apvts, pid::sigPunch) > 0.5f;
    macros.chaos     = raw (apvts, pid::sigChaos) > 0.5f;
    macros.air       = raw (apvts, pid::sigAir)   > 0.5f;

    // ---- engines ---------------------------------------------------------
    UnderwaterEngine::Params u;
    u.depth = raw (apvts, pid::uwDepth);      u.water = raw (apvts, pid::uwWater);
    u.murk  = raw (apvts, pid::uwMurk);       u.pressure = raw (apvts, pid::uwPressure);
    u.ripple = raw (apvts, pid::uwRipple);    u.wave = raw (apvts, pid::uwWave);
    u.bubble = raw (apvts, pid::uwBubble);    u.mix = raw (apvts, pid::uwMix);
    u.resonance = raw (apvts, pid::uwResonance);
    u.modPhaseDeg = raw (apvts, pid::uwModPhase);
    u.pressResponse = raw (apvts, pid::uwPressResp);
    u.slope = (int) raw (apvts, pid::uwSlope);
    u.modShape = (int) raw (apvts, pid::uwModShape);
    macros.applyUnderwater (u);
    underwater.setParams (u);
    washAmount = underwater.getWashAmount() * pct (u.mix);

    DistortionEngine::Params d;
    d.drive = raw (apvts, pid::dsDrive);   d.bite = raw (apvts, pid::dsBite);
    d.body  = raw (apvts, pid::dsBody);    d.crush = raw (apvts, pid::dsCrush);
    d.edge  = raw (apvts, pid::dsEdge);    d.smooth = raw (apvts, pid::dsSmooth);
    d.mix   = raw (apvts, pid::dsMix);     d.bias = raw (apvts, pid::dsBias);
    d.asym  = raw (apvts, pid::dsAsym);    d.preEmph = raw (apvts, pid::dsPreEmph);
    d.postFilter = raw (apvts, pid::dsPostFilter);
    d.algo = (int) raw (apvts, pid::dsType);
    macros.applyDistortion (d);
    distortion.setParams (d);

    SaturationEngine::Params s;
    s.drive = raw (apvts, pid::satDrive);       s.warmth = raw (apvts, pid::satWarmth);
    s.harmonics = raw (apvts, pid::satHarmonics); s.thickness = raw (apvts, pid::satThickness);
    s.tone = raw (apvts, pid::satTone);         s.softClip = raw (apvts, pid::satSoftClip);
    s.density = raw (apvts, pid::satDensity);   s.mix = raw (apvts, pid::satMix);
    s.model = (int) raw (apvts, pid::satModel);
    macros.applySaturation (s);
    saturation.setParams (s);

    MultibandDrive::Params mb;
    mb.low = raw (apvts, pid::mbLow);   mb.mid = raw (apvts, pid::mbMid);
    mb.high = raw (apvts, pid::mbHigh);
    mb.crossLow = raw (apvts, pid::mbCrossLow);
    mb.crossHigh = raw (apvts, pid::mbCrossHigh);
    mb.enabled = raw (apvts, pid::mbOn) > 0.5f;
    multiband.setParams (mb);

    // ---- EQ ---------------------------------------------------------------
    std::array<EqSection::Band, kNumEqBands> bands;
    for (int b = 0; b < kNumEqBands; ++b)
    {
        bands[(size_t) b].freq    = rawEq (apvts, b, "Freq");
        bands[(size_t) b].gain    = rawEq (apvts, b, "Gain");
        bands[(size_t) b].q       = rawEq (apvts, b, "Q");
        bands[(size_t) b].type    = (int) rawEq (apvts, b, "Type");
        bands[(size_t) b].on      = rawEq (apvts, b, "On") > 0.5f;
        bands[(size_t) b].dynamic = rawEq (apvts, b, "Dyn") > 0.5f;
        bands[(size_t) b].slope   = (int) rawEq (apvts, b, "Slope");
    }
    macros.applyEq (bands);

    for (int b = 0; b < kNumEqBands; ++b)
        eq.setBand (b, bands[(size_t) b]);

    eq.setEnabled (raw (apvts, pid::eqOn) > 0.5f);
    eq.updateCoefficients();

    // ---- dynamics ----------------------------------------------------------
    CompressorSection::Params c;
    c.threshold = raw (apvts, pid::compThresh); c.ratio = raw (apvts, pid::compRatio);
    c.attack = raw (apvts, pid::compAttack);    c.release = raw (apvts, pid::compRelease);
    c.knee = raw (apvts, pid::compKnee);        c.makeup = raw (apvts, pid::compMakeup);
    c.mix = raw (apvts, pid::compMix);          c.scHpf = raw (apvts, pid::compScHpf);
    c.mode = (int) raw (apvts, pid::compMode);
    c.detector = (int) raw (apvts, pid::compDetect);
    c.scAmount = raw (apvts, pid::compScAmount);
    c.autoMode = raw (apvts, pid::compAuto) > 0.5f;
    c.autoRelease = raw (apvts, pid::compAutoRel) > 0.5f;
    c.externalSidechain = raw (apvts, pid::compScExt) > 0.5f;
    c.enabled  = raw (apvts, pid::compOn) > 0.5f;
    macros.applyCompressor (c);
    compressor.setParams (c);

    TransientShaper::Params t;
    t.attack  = raw (apvts, pid::trAttack);
    t.body    = raw (apvts, pid::trBody);
    t.enabled = raw (apvts, pid::trOn) > 0.5f;
    macros.applyTransient (t);
    transient.setParams (t);

    DeEsser::Params de;
    de.freq = raw (apvts, pid::deFreq);        de.threshold = raw (apvts, pid::deThresh);
    de.amount = raw (apvts, pid::deAmount);    de.range = raw (apvts, pid::deRange);
    de.attack = raw (apvts, pid::deAttack);    de.release = raw (apvts, pid::deRelease);
    de.listen = raw (apvts, pid::deListen) > 0.5f;
    de.enabled = raw (apvts, pid::deOn) > 0.5f;
    deEsser.setParams (de);

    // ---- movement ------------------------------------------------------------
    ModulationEngine::Params m;
    m.rate = raw (apvts, pid::modRate);      m.depth  = raw (apvts, pid::modDepth);
    m.detune = raw (apvts, pid::modDetune);  m.width  = raw (apvts, pid::modWidth);
    m.motion = raw (apvts, pid::modMotion);  m.mix    = raw (apvts, pid::modMix);
    m.mode = (int) raw (apvts, pid::modMode);
    m.enabled = raw (apvts, pid::modOn) > 0.5f;
    m.shiftSemis = raw (apvts, pid::pitShift);
    m.shiftMix = raw (apvts, pid::pitMix);
    macros.applyModulation (m);
    modulation.setParams (m);

    FilterFx::Params fx;
    fx.on = raw (apvts, pid::fxOn) > 0.5f;
    fx.mode = (int) raw (apvts, pid::fxMode);
    fx.sync = raw (apvts, pid::fxSync) > 0.5f;
    fx.div = (int) raw (apvts, pid::fxDiv);
    fx.rateHz = raw (apvts, pid::fxRate);
    fx.freq = raw (apvts, pid::fxFreq);
    fx.depth = raw (apvts, pid::fxDepth);
    fx.reso = raw (apvts, pid::fxReso);
    fx.sens = raw (apvts, pid::fxSens);
    fx.shape = (int) raw (apvts, pid::fxShape);
    fx.drive = raw (apvts, pid::fxDrive);
    fx.stereo = raw (apvts, pid::fxStereo);
    fx.mix = raw (apvts, pid::fxMix);
    filterFx.setParams (fx, transport);

    // ---- space: the panel, the macros, and the underwater wash --------------
    SpaceEngine::Params sp;
    sp.revOn = raw (apvts, pid::revOn) > 0.5f;
    sp.revMix = raw (apvts, pid::revMix);     sp.revSize = raw (apvts, pid::revSize);
    sp.revDecay = raw (apvts, pid::revDecay); sp.revDamp = raw (apvts, pid::revDamp);
    sp.revPre = raw (apvts, pid::revPre);     sp.revDuck = raw (apvts, pid::revDuck);
    sp.dlyOn = raw (apvts, pid::dlyOn) > 0.5f;
    sp.dlyMix = raw (apvts, pid::dlyMix);
    sp.dlySync = raw (apvts, pid::dlySync) > 0.5f;
    sp.dlyDiv = (int) raw (apvts, pid::dlyDiv);
    sp.dlyTimeMs = raw (apvts, pid::dlyTime);
    sp.dlyFeedback = raw (apvts, pid::dlyFeedback);
    sp.dlyTone = raw (apvts, pid::dlyTone);
    sp.dlyPing = raw (apvts, pid::dlyPing) > 0.5f;
    sp.dlyDuck = raw (apvts, pid::dlyDuck);
    sp.dlyWarp = raw (apvts, pid::dlyWarp);
    macros.applySpace (sp);

    // WATER's wash: a dark, ducked reverb and a dark ping-pong 1/8 dotted
    // delay. If the SPACE page's own reverb or delay is on, the wash adds to
    // its level and leaves its settings alone.
    const float wash = washAmount * underwaterWeight;
    if (wash > 0.002f)
    {
        if (! sp.revOn)
        {
            sp.revOn = true;  sp.revMix = 0.0f;
            sp.revSize = 72.0f; sp.revDecay = 1.8f + wash * 4.4f; sp.revDamp = 64.0f + wash * 30.0f;
            sp.revPre = 16.0f; sp.revDuck = 58.0f;
        }
        sp.revMix = MacroEngine::push (sp.revMix, wash * 62.0f, 0.0f, 100.0f);

        if (! sp.dlyOn)
        {
            sp.dlyOn = true; sp.dlyMix = 0.0f; sp.dlySync = true; sp.dlyDiv = 8;
            sp.dlyFeedback = 28.0f + wash * 28.0f; sp.dlyTone = 28.0f; sp.dlyPing = true;
            sp.dlyDuck = 62.0f; sp.dlyWarp = 30.0f;
        }
        sp.dlyMix = MacroEngine::push (sp.dlyMix, wash * 24.0f, 0.0f, 100.0f);
    }
    space.setParams (sp, transport);

    // ---- output ------------------------------------------------------------
    OutputStage::Params o;
    o.width = raw (apvts, pid::stWidth);       o.sideLevel = raw (apvts, pid::stSide);
    o.monoComp = raw (apvts, pid::stMonoComp); o.ceiling = raw (apvts, pid::ceiling);
    o.ms = raw (apvts, pid::stMS) > 0.5f;      o.limiter = raw (apvts, pid::limiter) > 0.5f;
    o.truePeak = raw (apvts, pid::truePeak) > 0.5f;
    o.safety = raw (apvts, pid::safety) > 0.5f;
    o.phase = raw (apvts, pid::phase) > 0.5f;
    o.mono = raw (apvts, pid::mono) > 0.5f;
    macros.applyOutput (o);
    outputStage.setParams (o);

    midSideEngine = o.ms;
}

void OmgnedProcessor::processEngine (int engine, juce::AudioBuffer<float>& b)
{
    switch (engine)
    {
        case Distortion: distortion.process (b); break;
        case Saturation: saturation.process (b); break;
        case Underwater:
        default:         underwater.process (b); break;
    }
}

void OmgnedProcessor::resetEngine (int engine)
{
    switch (engine)
    {
        case Distortion: distortion.reset(); break;
        case Saturation: saturation.reset(); break;
        case Underwater:
        default:         underwater.reset(); break;
    }
}

/** Tells every module inside the oversampler the rate it now runs at. This is
    the fix for the fault that made the first version sound flat: the engines
    were prepared for eight times the session rate and then run at two, so
    every filter, LFO and envelope in them was out by a factor of four. */
void OmgnedProcessor::applyEngineRate (int osIndex)
{
    const double rate = currentSampleRate * (double) (1 << osIndex);
    underwater.setSampleRate (rate);
    distortion.setSampleRate (rate);
    saturation.setSampleRate (rate);
    multiband.setSampleRate (rate);

    for (auto& o : oversamplers) if (o != nullptr) o->reset();

    engineLatency = osIndex == 0 ? 0.0f : oversamplers[(size_t) (osIndex - 1)]->getLatencyInSamples();
    setLatencySamples (juce::roundToInt (engineLatency) + outputStage.getLatencySamples());
    lastOsIndex = osIndex;
}

void OmgnedProcessor::runCharacterChain (juce::AudioBuffer<float>& buffer, int osIndex)
{
    const int wanted = juce::jlimit (0, 2, (int) raw (apvts, pid::engine));
    const double runRate = currentSampleRate * (double) (1 << osIndex);

    if (currentEngine < 0)
    {
        currentEngine = wanted;
    }
    else if (wanted != currentEngine)
    {
        previousEngine = currentEngine;
        currentEngine = wanted;
        resetEngine (currentEngine);
        xfadeTotal = juce::jmax (1, (int) (0.030 * runRate));
        xfadeLeft = xfadeTotal;
    }

    auto run = [this] (juce::AudioBuffer<float>& b)
    {
        multiband.applyPre (b);

        if (xfadeLeft > 0 && previousEngine >= 0)
        {
            const int n = b.getNumSamples();
            const int ch = juce::jmin (b.getNumChannels(), xfadeScratch.getNumChannels());
            juce::AudioBuffer<float> old (xfadeScratch.getArrayOfWritePointers(), ch, n);
            for (int c = 0; c < ch; ++c) old.copyFrom (c, 0, b, c, 0, n);

            processEngine (currentEngine, b);
            processEngine (previousEngine, old);

            int left = xfadeLeft;
            for (int i = 0; i < n; ++i)
            {
                const float t = left > 0 ? (float) left / (float) xfadeTotal : 0.0f;   // 1 -> 0
                const float gOld = std::sin (t * juce::MathConstants<float>::halfPi);
                const float gNew = std::cos (t * juce::MathConstants<float>::halfPi);
                for (int c = 0; c < ch; ++c)
                    b.setSample (c, i, b.getSample (c, i) * gNew + old.getSample (c, i) * gOld);
                if (left > 0) --left;
            }
            xfadeLeft = left;
            if (xfadeLeft == 0) previousEngine = -1;
        }
        else
        {
            processEngine (currentEngine, b);
        }

        multiband.applyPost (b);
    };

    if (osIndex == 0)
    {
        run (buffer);
        return;
    }

    auto& os = *oversamplers[(size_t) (osIndex - 1)];
    juce::dsp::AudioBlock<float> block (buffer);
    auto upBlock = os.processSamplesUp (block);

    const int upCh = juce::jmin ((int) upBlock.getNumChannels(), osScratch.getNumChannels());
    const int upN  = juce::jmin ((int) upBlock.getNumSamples(), osScratch.getNumSamples());

    juce::AudioBuffer<float> upBuffer (osScratch.getArrayOfWritePointers(), upCh, upN);
    for (int c = 0; c < upCh; ++c)
        juce::FloatVectorOperations::copy (upBuffer.getWritePointer (c), upBlock.getChannelPointer ((size_t) c), upN);

    run (upBuffer);

    for (int c = 0; c < upCh; ++c)
        juce::FloatVectorOperations::copy (upBlock.getChannelPointer ((size_t) c), upBuffer.getReadPointer (c), upN);

    os.processSamplesDown (block);
}

void OmgnedProcessor::passThroughAligned (juce::AudioBuffer<float>& work)
{
    // latency is measured from when POWER was last on; if the plugin has never
    // processed, apply the latency it would report for the current settings
    if (lastOsIndex < 0)
        applyEngineRate (juce::jlimit (0, 3, (int) raw (apvts, pid::oversampling)));

    const float latency = (float) getLatencySamples();
    for (int c = 0; c < juce::jmin (2, work.getNumChannels()); ++c)
    {
        auto& line = bypassAlign[(size_t) c];
        auto* d = work.getWritePointer (c);
        for (int i = 0; i < work.getNumSamples(); ++i) { line.push (d[i]); d[i] = line.read (latency); }
    }
}

void OmgnedProcessor::processBlockBypassed (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    auto mainIn = getBusBuffer (buffer, true, 0);
    juce::AudioBuffer<float> work (mainIn.getArrayOfWritePointers(), juce::jmin (2, mainIn.getNumChannels()), mainIn.getNumSamples());
    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());
    passThroughAligned (work);
}

void OmgnedProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainIn = getBusBuffer (buffer, true, 0);
    const int numSamples = mainIn.getNumSamples();
    const int numCh = juce::jmin (2, mainIn.getNumChannels());

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

    if (numSamples == 0 || numCh == 0)
        return;

    // ---- the external sidechain, taken before anything touches the main bus --
    const bool haveSidechain = getBusCount (true) > 1
                            && getBus (true, 1) != nullptr
                            && getBus (true, 1)->isEnabled();
    sidechainConnected.store (haveSidechain);

    const juce::AudioBuffer<float>* sidechain = nullptr;
    if (haveSidechain)
    {
        auto scIn = getBusBuffer (buffer, true, 1);
        const int scCh = juce::jmin (2, scIn.getNumChannels());
        if (scCh > 0 && numSamples <= sidechainCopy.getNumSamples())
        {
            for (int c = 0; c < 2; ++c)
                sidechainCopy.copyFrom (c, 0, scIn, juce::jmin (c, scCh - 1), 0, numSamples);
            sidechain = &sidechainCopy;
        }
    }

    juce::AudioBuffer<float> work (mainIn.getArrayOfWritePointers(), numCh, numSamples);
    inputMeter.push (work);

    // the bypass line always carries the input, so switching POWER off
    // continues the audio from exactly where it was rather than from silence
    const bool poweredNow = raw (apvts, pid::power) > 0.5f;
    if (poweredNow)
        for (int c = 0; c < numCh; ++c)
        {
            auto& line = bypassAlign[(size_t) c];
            const auto* d = work.getReadPointer (c);
            for (int i = 0; i < numSamples; ++i) line.push (d[i]);
        }

    const auto transport = readTransport();
    currentBpm.store (transport.bpm);

    const bool powered = raw (apvts, pid::power) > 0.5f;
    if (! powered)
    {
        passThroughAligned (work);
        outputMeter.push (work);
        driveMeterDb.store (-100.0f);
        spectrum.push (work.getReadPointer (0), numSamples);
        return;
    }

    // ---- follow the engine for the underwater wash, over about 80 ms ---------
    {
        const float target = (int) raw (apvts, pid::engine) == Underwater ? 1.0f : 0.0f;
        const float k = juce::jmin (1.0f, (float) numSamples / (float) (0.08 * currentSampleRate));
        underwaterWeight += (target - underwaterWeight) * k;
    }

    pullParameters (transport);

    const int osIndex = juce::jlimit (0, 3, (int) raw (apvts, pid::oversampling));
    if (osIndex != lastOsIndex)
        applyEngineRate (osIndex);

    // ---- input gain --------------------------------------------------------
    const float inTarget = dsp::dbToGain (raw (apvts, pid::inGain));
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = inGainSmooth.next (inTarget);
        for (int c = 0; c < numCh; ++c)
            work.setSample (c, i, work.getSample (c, i) * g);
    }

    // ---- dry copy for the final mix, delayed to line up with the engine ------
    if (dryBuffer.getNumSamples() < numSamples)
        dryBuffer.setSize (2, numSamples, false, false, true);

    const float latency = engineLatency;
    for (int c = 0; c < numCh; ++c)
    {
        auto& line = dryAlign[(size_t) c];
        const auto* src = work.getReadPointer (c);
        auto* dst = dryBuffer.getWritePointer (c);
        for (int i = 0; i < numSamples; ++i) { line.push (src[i]); dst[i] = line.read (latency); }
    }

    // ---- tone and vocal shaping in front of the character --------------------
    eq.process (work);
    deEsser.process (work);
    transient.process (work);

    const int compPlacement = juce::jlimit (0, 1, (int) raw (apvts, pid::compPlace));
    if (compPlacement == CompPre)
        compressor.process (work, sidechain);

    driveMeterDb.store (dsp::gainToDb (work.getMagnitude (0, numSamples)));

    // ---- M/S: the engine hears the mid only, the side waits aligned ---------
    const bool ms = midSideEngine && numCh == 2;
    if (ms)
    {
        auto* L = work.getWritePointer (0);
        auto* R = work.getWritePointer (1);
        auto* S = sideStash.getWritePointer (0);
        for (int i = 0; i < numSamples; ++i)
        {
            const float mid = 0.5f * (L[i] + R[i]);
            sideAlign.push (0.5f * (L[i] - R[i]));
            S[i] = sideAlign.read (latency);
            L[i] = R[i] = mid;
        }
    }

    runCharacterChain (work, osIndex);

    if (ms)
    {
        auto* L = work.getWritePointer (0);
        auto* R = work.getWritePointer (1);
        const auto* S = sideStash.getReadPointer (0);
        for (int i = 0; i < numSamples; ++i) { L[i] += S[i]; R[i] -= S[i]; }
    }

    // ---- movement and space ----------------------------------------------------
    modulation.process (work);
    filterFx.process (work);

    if (compPlacement == CompPost)
        compressor.process (work, sidechain);

    space.process (work);

    // ---- global dry / wet ----------------------------------------------------
    const float mixTarget = dsp::pct (raw (apvts, pid::mix));
    const float dryTarget = dsp::dbToGain (raw (apvts, pid::dryLevel));
    const float wetTarget = dsp::dbToGain (raw (apvts, pid::wetLevel));

    for (int i = 0; i < numSamples; ++i)
    {
        const float m  = mixSmooth.next (mixTarget);
        const float dg = dryLevelSmooth.next (dryTarget);
        const float wg = wetLevelSmooth.next (wetTarget);

        for (int c = 0; c < numCh; ++c)
        {
            const float wet = work.getSample (c, i) * wg;
            const float dry = dryBuffer.getSample (c, i) * dg;
            work.setSample (c, i, dry * (1.0f - m) + wet * m);
        }
    }

    // ---- output gain, then the limiter and the safety clip, which are last:
    // nothing, OUTPUT GAIN included, can push the plugin past its ceiling -----
    const float outTarget = dsp::dbToGain (raw (apvts, pid::outGain));
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = outGainSmooth.next (outTarget);
        for (int c = 0; c < numCh; ++c)
            work.setSample (c, i, work.getSample (c, i) * g);
    }

    outputStage.process (work);

    // ---- nothing non-finite ever leaves the plugin ------------------------------
    for (int c = 0; c < numCh; ++c)
    {
        auto* d = work.getWritePointer (c);
        for (int i = 0; i < numSamples; ++i)
            if (! std::isfinite (d[i]))
                d[i] = 0.0f;
    }

    outputMeter.push (work);
    spectrum.push (work.getReadPointer (0), numSamples);

    if (hasTransportOverride)
        transportOverride.ppqAtBlockStart += numSamples * transportOverride.bpm / 60.0 / currentSampleRate;
}

void OmgnedProcessor::setEditorSize (int w, int h)
{
    auto state = apvts.state;
    state.setProperty ("editorWidth", w, nullptr);
    state.setProperty ("editorHeight", h, nullptr);
}

juce::Rectangle<int> OmgnedProcessor::getSavedEditorSize() const
{
    const int w = (int) apvts.state.getProperty ("editorWidth", 1200);
    const int h = (int) apvts.state.getProperty ("editorHeight", 720);
    return { juce::jmax (900, w), juce::jmax (600, h) };
}

juce::AudioProcessorEditor* OmgnedProcessor::createEditor()
{
    return new OmgnedEditor (*this);
}

void OmgnedProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void OmgnedProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new OmgnedProcessor();
}
