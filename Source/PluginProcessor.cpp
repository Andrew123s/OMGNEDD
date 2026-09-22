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

    for (int i = 0; i < 3; ++i)
    {
        oversamplers[(size_t) i] = std::make_unique<juce::dsp::Oversampling<float>> (
            2, i + 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
        oversamplers[(size_t) i]->initProcessing ((size_t) samplesPerBlock);
    }

    juce::dsp::ProcessSpec base { sampleRate, (juce::uint32) samplesPerBlock, 2 };

    eq.prepare (base);
    compressor.prepare (base);
    deEsser.prepare (base);
    outputStage.prepare (base);
    transient.prepare (base);
    modulation.prepare (base);

    // the nonlinear engines and the crossover run inside the oversampler, so
    // they are prepared at the highest rate they may ever be asked to run at
    juce::dsp::ProcessSpec os { sampleRate * 8.0, (juce::uint32) (samplesPerBlock * 8), 2 };
    underwater.prepare (os);
    distortion.prepare (os);
    saturation.prepare (os);
    multiband.prepare (os);

    dryBuffer.setSize (2, samplesPerBlock, false, false, true);
    sidechainCopy.setSize (2, samplesPerBlock, false, false, true);

    // the widest the oversampler can ever hand back, allocated once here so
    // processBlock never asks the allocator for anything
    osScratch.setSize (2, samplesPerBlock * 8, false, false, true);

    inGainSmooth.reset (sampleRate, 0.02, 1.0f);
    outGainSmooth.reset (sampleRate, 0.02, 1.0f);
    mixSmooth.reset (sampleRate, 0.02, 1.0f);
    dryLevelSmooth.reset (sampleRate, 0.02, 1.0f);
    wetLevelSmooth.reset (sampleRate, 0.02, 1.0f);

    inputMeter.prepare (sampleRate, 2);
    outputMeter.prepare (sampleRate, 2);
    spectrum.prepare (sampleRate);

    lastOversamplingIndex = -1;
}

void OmgnedProcessor::releaseResources()
{
    underwater.reset(); distortion.reset(); saturation.reset();
    eq.reset(); compressor.reset(); deEsser.reset(); outputStage.reset();
    modulation.reset(); multiband.reset(); transient.reset();
    for (auto& o : oversamplers) if (o != nullptr) o->reset();

    driveMeterDb.store (-100.0f);
}

void OmgnedProcessor::pullParameters()
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

    ModulationEngine::Params m;
    m.rate = raw (apvts, pid::modRate);      m.depth  = raw (apvts, pid::modDepth);
    m.detune = raw (apvts, pid::modDetune);  m.width  = raw (apvts, pid::modWidth);
    m.motion = raw (apvts, pid::modMotion);  m.mix    = raw (apvts, pid::modMix);
    m.mode = (int) raw (apvts, pid::modMode);
    m.enabled = raw (apvts, pid::modOn) > 0.5f;
    m.chaos = macros.chaos;
    macros.applyModulation (m);
    modulation.setParams (m);

    MultibandDrive::Params mb;
    mb.low = raw (apvts, pid::mbLow);   mb.mid = raw (apvts, pid::mbMid);
    mb.high = raw (apvts, pid::mbHigh);
    mb.crossLow = raw (apvts, pid::mbCrossLow);
    mb.crossHigh = raw (apvts, pid::mbCrossHigh);
    mb.enabled = raw (apvts, pid::mbOn) > 0.5f;
    multiband.setParams (mb);

    DeEsser::Params de;
    de.freq = raw (apvts, pid::deFreq);        de.threshold = raw (apvts, pid::deThresh);
    de.amount = raw (apvts, pid::deAmount);    de.range = raw (apvts, pid::deRange);
    de.attack = raw (apvts, pid::deAttack);    de.release = raw (apvts, pid::deRelease);
    de.listen = raw (apvts, pid::deListen) > 0.5f;
    de.enabled = raw (apvts, pid::deOn) > 0.5f;
    deEsser.setParams (de);

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
}

/** The character engine, with the multiband pre and post gains around it and
    the oversampler around the pair. Kept out of processBlock so the routing
    reads in one piece.
*/
void OmgnedProcessor::runCharacterChain (juce::AudioBuffer<float>& buffer, int engineMode, int osIndex)
{
    auto runEngine = [this, engineMode] (juce::AudioBuffer<float>& b)
    {
        multiband.applyPre (b);

        switch (engineMode)
        {
            case Distortion: distortion.process (b); break;
            case Saturation: saturation.process (b); break;
            case Underwater:
            default:         underwater.process (b); break;
        }

        multiband.applyPost (b);
    };

    if (osIndex == 0)
    {
        runEngine (buffer);
        setLatencySamples (0);
        return;
    }

    auto& os = *oversamplers[(size_t) (osIndex - 1)];
    juce::dsp::AudioBlock<float> block (buffer);
    auto upBlock = os.processSamplesUp (block);

    const int upCh = (int) upBlock.getNumChannels();
    const int upN  = (int) upBlock.getNumSamples();

    // osScratch was sized at prepare for the widest case, so this is a view
    // onto memory that already exists rather than an allocation
    jassert (upCh <= osScratch.getNumChannels() && upN <= osScratch.getNumSamples());

    juce::AudioBuffer<float> upBuffer (osScratch.getArrayOfWritePointers(),
                                       juce::jmin (upCh, osScratch.getNumChannels()),
                                       juce::jmin (upN, osScratch.getNumSamples()));

    for (int c = 0; c < upBuffer.getNumChannels(); ++c)
        juce::FloatVectorOperations::copy (upBuffer.getWritePointer (c),
                                           upBlock.getChannelPointer ((size_t) c),
                                           upBuffer.getNumSamples());

    runEngine (upBuffer);

    for (int c = 0; c < upBuffer.getNumChannels(); ++c)
        juce::FloatVectorOperations::copy (upBlock.getChannelPointer ((size_t) c),
                                           upBuffer.getReadPointer (c),
                                           upBuffer.getNumSamples());

    os.processSamplesDown (block);
    setLatencySamples ((int) os.getLatencyInSamples());
}

void OmgnedProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    auto mainIn  = getBusBuffer (buffer, true, 0);
    const int numSamples = mainIn.getNumSamples();
    const int numCh = juce::jmin (2, mainIn.getNumChannels());

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, buffer.getNumSamples());

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

    // the plugin works on the main bus only; a wider buffer keeps its tail
    juce::AudioBuffer<float> work (mainIn.getArrayOfWritePointers(), numCh, numSamples);

    inputMeter.push (work);

    const bool powered = raw (apvts, pid::power) > 0.5f;
    if (! powered)
    {
        outputMeter.push (work);
        driveMeterDb.store (-100.0f);
        if (numCh > 0) spectrum.push (work.getReadPointer (0), numSamples);
        return;
    }

    pullParameters();

    // ---- input gain --------------------------------------------------------
    const float inTarget = dsp::dbToGain (raw (apvts, pid::inGain));
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = inGainSmooth.next (inTarget);
        for (int c = 0; c < numCh; ++c)
            work.setSample (c, i, work.getSample (c, i) * g);
    }

    // ---- dry copy for the final mix ----------------------------------------
    if (dryBuffer.getNumSamples() < numSamples || dryBuffer.getNumChannels() < numCh)
        dryBuffer.setSize (juce::jmax (2, numCh), juce::jmax (numSamples, currentBlockSize), false, false, true);

    for (int c = 0; c < numCh; ++c)
        dryBuffer.copyFrom (c, 0, work, c, 0, numSamples);

    // ---- tone, then the vocal shaping in front of the character -------------
    eq.process (work);
    deEsser.process (work);
    transient.process (work);

    const int compPlacement = juce::jlimit (0, 1, (int) raw (apvts, pid::compPlace));

    if (compPlacement == CompPre)
        compressor.process (work, sidechain);

    // ---- what the character engine is about to be handed, for the meter -----
    driveMeterDb.store (dsp::gainToDb (work.getMagnitude (0, numSamples)));

    // ---- character engine, with the multiband gains and the oversampler -----
    runCharacterChain (work,
                       juce::jlimit (0, 2, (int) raw (apvts, pid::engine)),
                       juce::jlimit (0, 3, (int) raw (apvts, pid::oversampling)));

    // ---- movement, after the character so it modulates the result -----------
    modulation.process (work);

    if (compPlacement == CompPost)
        compressor.process (work, sidechain);

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
            const float dry = dryBuffer.getSample (juce::jmin (c, dryBuffer.getNumChannels() - 1), i) * dg;
            work.setSample (c, i, dry + (wet - dry) * m);
        }
    }

    // ---- stereo, limiter, safety ------------------------------------------------
    outputStage.process (work);

    // ---- output gain -------------------------------------------------------------
    const float outTarget = dsp::dbToGain (raw (apvts, pid::outGain));
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = outGainSmooth.next (outTarget);
        for (int c = 0; c < numCh; ++c)
            work.setSample (c, i, work.getSample (c, i) * g);
    }

    // ---- last line of defence: nothing infinite ever leaves the plugin -----------
    for (int c = 0; c < numCh; ++c)
    {
        auto* d = work.getWritePointer (c);
        for (int i = 0; i < numSamples; ++i)
            if (! std::isfinite (d[i]))
                d[i] = 0.0f;
    }

    outputMeter.push (work);
    if (numCh > 0)
        spectrum.push (work.getReadPointer (0), numSamples);
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
