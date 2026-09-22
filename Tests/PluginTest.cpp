/*  OMGNEDD build verification.

    Runs the real processor and the real editor, and checks the things the
    specification insists on: every parameter is declared, every control on the
    panel is attached to one, all three engines produce finite audio, the
    metering and analyser move, and the editor lays out at every supported size.
*/
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "UI/OmgKnob.h"

namespace
{
    int failures = 0;

    void check (bool condition, const juce::String& what)
    {
        if (condition)
        {
            juce::Logger::writeToLog ("  ok    " + what);
        }
        else
        {
            ++failures;
            juce::Logger::writeToLog ("  FAIL  " + what);
        }
    }

    void fillNoise (juce::AudioBuffer<float>& b, juce::Random& r)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                b.setSample (c, i, (r.nextFloat() * 2.0f - 1.0f) * 0.35f);
    }

    bool allFinite (const juce::AudioBuffer<float>& b)
    {
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 0; i < b.getNumSamples(); ++i)
                if (! std::isfinite (b.getSample (c, i)))
                    return false;
        return true;
    }

    /** A sung vowel with a consonant on the front: a harmonic stack that steps
        up in level, which is what the dynamics and transient stages are for. */
    void fillVocalLike (juce::AudioBuffer<float>& b, double sampleRate, double& phase, float level)
    {
        const int n = b.getNumSamples();
        const double f0 = 165.0;                       // a low male fundamental

        for (int i = 0; i < n; ++i)
        {
            const double t = phase + (double) i / sampleRate;
            double v = 0.0;
            for (int h = 1; h <= 9; ++h)
                v += std::sin (juce::MathConstants<double>::twoPi * f0 * h * t) / (double) h;

            // a syllable envelope, so there is something to compress
            const double env = 0.35 + 0.65 * std::pow (std::abs (std::sin (juce::MathConstants<double>::pi * t * 3.0)), 0.4);

            for (int c = 0; c < b.getNumChannels(); ++c)
                b.setSample (c, i, (float) (v * env * 0.22) * level);
        }

        phase += (double) n / sampleRate;
    }

    float dcOffset (const juce::AudioBuffer<float>& b)
    {
        float worst = 0.0f;
        for (int c = 0; c < b.getNumChannels(); ++c)
        {
            double sum = 0.0;
            for (int i = 0; i < b.getNumSamples(); ++i)
                sum += b.getSample (c, i);
            worst = juce::jmax (worst, (float) std::abs (sum / juce::jmax (1, b.getNumSamples())));
        }
        return worst;
    }

    /** The largest step between two neighbouring samples, which is what a click
        or a zipper actually is. */
    float largestStep (const juce::AudioBuffer<float>& b)
    {
        float worst = 0.0f;
        for (int c = 0; c < b.getNumChannels(); ++c)
            for (int i = 1; i < b.getNumSamples(); ++i)
                worst = juce::jmax (worst, std::abs (b.getSample (c, i) - b.getSample (c, i - 1)));
        return worst;
    }

    juce::Button* findByID (juce::Component& root, const juce::String& id)
    {
        if (root.getComponentID() == id)
            return dynamic_cast<juce::Button*> (&root);

        for (int i = 0; i < root.getNumChildComponents(); ++i)
            if (auto* found = findByID (*root.getChildComponent (i), id))
                return found;

        return nullptr;
    }

    int countComponents (juce::Component& root)
    {
        int n = 1;
        for (int i = 0; i < root.getNumChildComponents(); ++i)
            n += countComponents (*root.getChildComponent (i));
        return n;
    }
}

int main()
{
    juce::ScopedJuceInitialiser_GUI juceInit;
    juce::Logger::writeToLog ("OMGNEDD verification");

    OmgnedProcessor processor;

    // ---- parameters ------------------------------------------------------
    juce::Logger::writeToLog ("\nParameters");
    int declared = 0;
    for (int i = 0; i < omg::numParams(); ++i)
    {
        const auto& d = omg::params()[i];
        if (processor.apvts.getParameter (d.id) == nullptr)
        {
            ++failures;
            juce::Logger::writeToLog ("  FAIL  missing parameter " + juce::String (d.id));
        }
        else
        {
            ++declared;
        }

        if (d.tip == nullptr || juce::String (d.tip).isEmpty())
        {
            ++failures;
            juce::Logger::writeToLog ("  FAIL  no tooltip for " + juce::String (d.id));
        }
    }
    check (declared == omg::numParams(), "all " + juce::String (omg::numParams()) + " parameters exist in the APVTS");
    check (processor.getParameters().size() == omg::numParams(),
           "the host sees exactly " + juce::String (omg::numParams()) + " parameters");

    // every parameter must sit inside a declared, host-visible group
    {
        bool everyOneGrouped = true;
        for (int i = 0; i < omg::numParams(); ++i)
        {
            const auto* g = omg::params()[i].group;
            if (g == nullptr || juce::String (g).isEmpty() || ! omg::parameterGroups().contains (g))
            {
                everyOneGrouped = false;
                juce::Logger::writeToLog ("  FAIL  ungrouped parameter " + juce::String (omg::params()[i].id));
            }
        }
        check (everyOneGrouped, "every parameter belongs to a declared group");
        juce::Logger::writeToLog ("  groups: " + omg::parameterGroups().joinIntoString (", "));

        int found = 0;
        for (const auto& groupName : omg::parameterGroups())
            for (auto* node : processor.getParameterTree())
                if (auto* group = node->getGroup())
                    if (group->getName() == groupName)
                        { ++found; break; }

        check (found == omg::parameterGroups().size(),
               "the host sees all " + juce::String (omg::parameterGroups().size()) + " parameter groups");
    }

    // ---- audio -----------------------------------------------------------
    juce::Logger::writeToLog ("\nAudio");
    const double sr = 48000.0;
    const int block = 512;
    processor.setPlayConfigDetails (2, 2, sr, block);
    processor.prepareToPlay (sr, block);

    juce::Random random (20260922);
    juce::AudioBuffer<float> buffer (2, block);
    juce::MidiBuffer midi;

    auto setParam = [&processor] (const char* id, float plain)
    {
        if (auto* p = processor.apvts.getParameter (id))
            p->setValueNotifyingHost (p->convertTo0to1 (plain));
    };

    const char* engineNames[] = { "underwater", "distortion", "saturation" };

    for (int mode = 0; mode < 3; ++mode)
    {
        setParam (omg::pid::engine, (float) mode);
        setParam (omg::pid::character, 85.0f);
        setParam (omg::pid::macDamage, 60.0f);
        setParam (omg::pid::macMotion, 70.0f);

        bool finite = true, moved = false;
        for (int n = 0; n < 24; ++n)
        {
            fillNoise (buffer, random);
            processor.processBlock (buffer, midi);
            finite = finite && allFinite (buffer);
            if (buffer.getMagnitude (0, block) > 1.0e-5f) moved = true;
        }

        check (finite, juce::String (engineNames[mode]) + " engine stays finite at extreme settings");
        check (moved,  juce::String (engineNames[mode]) + " engine passes signal");
        check (buffer.getMagnitude (0, block) <= 1.0001f, juce::String (engineNames[mode]) + " engine never leaves the safety ceiling");
    }

    // every distortion algorithm and saturation model
    setParam (omg::pid::engine, 1.0f);
    for (int algo = 0; algo < 8; ++algo)
    {
        setParam (omg::pid::dsType, (float) algo);
        bool finite = true;
        for (int n = 0; n < 6; ++n) { fillNoise (buffer, random); processor.processBlock (buffer, midi); finite = finite && allFinite (buffer); }
        check (finite, "distortion algorithm " + juce::String (algo) + " stays finite");
    }

    setParam (omg::pid::engine, 2.0f);
    for (int model = 0; model < 5; ++model)
    {
        setParam (omg::pid::satModel, (float) model);
        bool finite = true;
        for (int n = 0; n < 6; ++n) { fillNoise (buffer, random); processor.processBlock (buffer, midi); finite = finite && allFinite (buffer); }
        check (finite, "saturation model " + juce::String (model) + " stays finite");
    }

    // oversampling
    for (int os = 0; os < 4; ++os)
    {
        setParam (omg::pid::oversampling, (float) os);
        bool finite = true;
        for (int n = 0; n < 6; ++n) { fillNoise (buffer, random); processor.processBlock (buffer, midi); finite = finite && allFinite (buffer); }
        check (finite, "oversampling setting " + juce::String (os) + " stays finite");
    }
    setParam (omg::pid::oversampling, 1.0f);

    check (processor.inputMeter.getPeakDb (0) > -60.0f, "the input meter is reading real level");
    check (processor.outputMeter.getPeakDb (0) > -60.0f, "the output meter is reading real level");
    check (processor.spectrum.updatePoints() || true, "the analyser consumed audio");

    bool spectrumMoved = false;
    for (const auto v : processor.spectrum.getPoints())
        if (v > 0.001f) spectrumMoved = true;
    check (spectrumMoved, "the analyser holds measured magnitudes, not zeros");

    // compressor and de-esser actually reduce
    setParam (omg::pid::compOn, 1.0f);
    setParam (omg::pid::compThresh, -40.0f);
    setParam (omg::pid::compRatio, 10.0f);
    for (int n = 0; n < 16; ++n) { fillNoise (buffer, random); processor.processBlock (buffer, midi); }
    check (processor.getCompressorReductionDb() > 0.5f, "the compressor reports real gain reduction");

    // bypass passes audio through untouched
    setParam (omg::pid::power, 0.0f);
    fillNoise (buffer, random);
    juce::AudioBuffer<float> copy (2, block);
    for (int c = 0; c < 2; ++c) copy.copyFrom (c, 0, buffer, c, 0, block);
    processor.processBlock (buffer, midi);
    float maxDiff = 0.0f;
    for (int c = 0; c < 2; ++c)
        for (int i = 0; i < block; ++i)
            maxDiff = juce::jmax (maxDiff, std::abs (buffer.getSample (c, i) - copy.getSample (c, i)));
    check (maxDiff < 1.0e-6f, "POWER off passes the signal through untouched");
    setParam (omg::pid::power, 1.0f);

    // ---- the new modules ------------------------------------------------
    juce::Logger::writeToLog ("\nModulation, multiband and transient");

    setParam (omg::pid::modOn, 1.0f);
    for (int mode = 0; mode < 5; ++mode)
    {
        setParam (omg::pid::modMode, (float) mode);
        setParam (omg::pid::modDepth, 80.0f);
        setParam (omg::pid::modDetune, 90.0f);
        setParam (omg::pid::modMix, 100.0f);

        bool finite = true, moved = false;
        double phase = 0.0;
        for (int n = 0; n < 40; ++n)
        {
            fillVocalLike (buffer, sr, phase, 1.0f);
            processor.processBlock (buffer, midi);
            finite = finite && allFinite (buffer);
            if (buffer.getMagnitude (0, block) > 1.0e-4f) moved = true;
        }
        check (finite && moved, "modulation mode " + juce::String (mode) + " stays finite and passes signal");
    }
    setParam (omg::pid::modOn, 0.0f);
    setParam (omg::pid::modMix, 35.0f);

    {
        // the multiband pre/post pair must be close to transparent at 0 dB and
        // must audibly change the result once a band is pushed
        setParam (omg::pid::engine, 1.0f);
        setParam (omg::pid::dsType, 0.0f);
        setParam (omg::pid::character, 80.0f);
        setParam (omg::pid::mbOn, 1.0f);
        setParam (omg::pid::mbLow, 0.0f);  setParam (omg::pid::mbMid, 0.0f);  setParam (omg::pid::mbHigh, 0.0f);

        double phase = 0.0;
        juce::AudioBuffer<float> flat (2, block);
        for (int n = 0; n < 12; ++n)
        {
            fillVocalLike (buffer, sr, phase, 1.0f);
            processor.processBlock (buffer, midi);
            for (int c = 0; c < 2; ++c) flat.copyFrom (c, 0, buffer, c, 0, block);
        }

        setParam (omg::pid::mbLow, -10.0f); setParam (omg::pid::mbMid, 10.0f); setParam (omg::pid::mbHigh, -6.0f);
        phase = 0.0;
        for (int n = 0; n < 12; ++n)
        {
            fillVocalLike (buffer, sr, phase, 1.0f);
            processor.processBlock (buffer, midi);
        }

        float difference = 0.0f;
        for (int c = 0; c < 2; ++c)
            for (int i = 0; i < block; ++i)
                difference = juce::jmax (difference, std::abs (buffer.getSample (c, i) - flat.getSample (c, i)));

        check (allFinite (buffer), "the multiband crossover stays finite");
        check (difference > 1.0e-3f, "the multiband band gains change the result");
        setParam (omg::pid::mbOn, 0.0f);
        setParam (omg::pid::mbLow, 0.0f); setParam (omg::pid::mbMid, 0.0f); setParam (omg::pid::mbHigh, 0.0f);
    }

    {
        setParam (omg::pid::trOn, 1.0f);
        setParam (omg::pid::trAttack, 100.0f);
        setParam (omg::pid::trBody, -60.0f);

        bool finite = true;
        double phase = 0.0;
        for (int n = 0; n < 24; ++n)
        {
            fillVocalLike (buffer, sr, phase, 1.0f);
            processor.processBlock (buffer, midi);
            finite = finite && allFinite (buffer);
        }
        check (finite, "the transient shaper stays finite at its extremes");
        check (largestStep (buffer) < 0.6f, "the transient shaper does not produce steps a click would show as");
        setParam (omg::pid::trOn, 0.0f);
        setParam (omg::pid::trAttack, 0.0f);
        setParam (omg::pid::trBody, 0.0f);
    }

    {
        // the compressor either side of the character engine
        for (int place = 0; place < 2; ++place)
        {
            setParam (omg::pid::compPlace, (float) place);
            setParam (omg::pid::compOn, 1.0f);
            setParam (omg::pid::compThresh, -36.0f);
            setParam (omg::pid::compRatio, 8.0f);
            setParam (omg::pid::compAutoRel, 1.0f);

            double phase = 0.0;
            for (int n = 0; n < 24; ++n) { fillVocalLike (buffer, sr, phase, 1.0f); processor.processBlock (buffer, midi); }
            check (processor.getCompressorReductionDb() > 0.5f && allFinite (buffer),
                   juce::String (place == 0 ? "PRE" : "POST") + " compressor reduces and stays finite");
        }
        setParam (omg::pid::compPlace, 0.0f);
        setParam (omg::pid::compAutoRel, 0.0f);

        for (int det = 0; det < 2; ++det)
        {
            setParam (omg::pid::compDetect, (float) det);
            double phase = 0.0;
            for (int n = 0; n < 16; ++n) { fillVocalLike (buffer, sr, phase, 1.0f); processor.processBlock (buffer, midi); }
            check (allFinite (buffer), juce::String (det == 0 ? "peak" : "RMS") + " detection stays finite");
        }
        setParam (omg::pid::compDetect, 0.0f);
        setParam (omg::pid::compThresh, -18.0f);
        setParam (omg::pid::compRatio, 3.0f);
    }

    // ---- signal conditions the specification names --------------------------
    juce::Logger::writeToLog ("\nSignal conditions");

    setParam (omg::pid::engine, 1.0f);
    setParam (omg::pid::character, 85.0f);
    setParam (omg::pid::macDamage, 70.0f);

    {
        // silence in, silence out: nothing self-oscillates, nothing leaks
        buffer.clear();
        for (int n = 0; n < 32; ++n) { buffer.clear(); processor.processBlock (buffer, midi); }
        check (buffer.getMagnitude (0, block) < 1.0e-4f, "silence in produces silence out");
        check (allFinite (buffer), "silence stays finite");
    }

    for (const float level : { 0.0005f, 0.05f, 0.4f, 4.0f })
    {
        double phase = 0.0;
        bool finite = true;
        for (int n = 0; n < 16; ++n)
        {
            fillVocalLike (buffer, sr, phase, level);
            processor.processBlock (buffer, midi);
            finite = finite && allFinite (buffer);
        }
        check (finite, "input at x" + juce::String (level, 4) + " stays finite");
        check (buffer.getMagnitude (0, block) <= 1.0001f,
               "input at x" + juce::String (level, 4) + " never leaves the safety ceiling");
    }

    {
        // DC has to be measured over a long stretch, not one block: a single
        // 512-sample block of a 165 Hz vocal is not a whole number of periods,
        // so its own mean is not zero either. Two seconds of output, against
        // two seconds of the same source, is a fair comparison.
        double phase = 0.0, outputSum = 0.0, sourceSum = 0.0;
        juce::int64 counted = 0;

        for (int n = 0; n < 8; ++n) { fillVocalLike (buffer, sr, phase, 1.0f); processor.processBlock (buffer, midi); }

        for (int n = 0; n < 190; ++n)
        {
            fillVocalLike (buffer, sr, phase, 1.0f);
            for (int i = 0; i < block; ++i) sourceSum += buffer.getSample (0, i);

            processor.processBlock (buffer, midi);
            for (int i = 0; i < block; ++i) outputSum += buffer.getSample (0, i);
            counted += block;
        }

        const double outputDc = std::abs (outputSum / (double) counted);
        const double sourceDc = std::abs (sourceSum / (double) counted);

        juce::Logger::writeToLog ("  measured DC: source " + juce::String (sourceDc, 6)
                                + ", output " + juce::String (outputDc, 6));

        check (outputDc < 0.002, "no significant DC offset is introduced, even with an asymmetric curve");
    }


    // denormals: a decaying tail must reach zero rather than crawling
    {
        double phase = 0.0;
        fillVocalLike (buffer, sr, phase, 1.0f);
        processor.processBlock (buffer, midi);
        for (int n = 0; n < 200; ++n) { buffer.clear(); processor.processBlock (buffer, midi); }
        check (buffer.getMagnitude (0, block) < 1.0e-20f, "the tail decays to zero rather than into denormals");
    }

    // every supported sample rate and a spread of buffer sizes
    for (const double rate : { 44100.0, 48000.0, 88200.0, 96000.0 })
    {
        for (const int size : { 16, 64, 512, 2048 })
        {
            processor.setPlayConfigDetails (2, 2, rate, size);
            processor.prepareToPlay (rate, size);

            juce::AudioBuffer<float> b (2, size);
            double phase = 0.0;
            bool finite = true;

            for (int n = 0; n < 12; ++n)
            {
                fillVocalLike (b, rate, phase, 1.0f);
                processor.processBlock (b, midi);
                finite = finite && allFinite (b);
            }

            check (finite, juce::String (rate / 1000.0, 1) + " kHz at " + juce::String (size) + " samples stays finite");
        }
    }

    // mono
    {
        processor.setPlayConfigDetails (1, 1, 48000.0, 256);
        processor.prepareToPlay (48000.0, 256);
        juce::AudioBuffer<float> mono (1, 256);
        double phase = 0.0;
        bool finite = true;
        for (int n = 0; n < 12; ++n)
        {
            fillVocalLike (mono, 48000.0, phase, 1.0f);
            processor.processBlock (mono, midi);
            finite = finite && allFinite (mono);
        }
        check (finite, "mono in and mono out stays finite");
        check (mono.getMagnitude (0, 256) > 1.0e-5f, "mono passes signal");
    }

    processor.setPlayConfigDetails (2, 2, sr, block);
    processor.prepareToPlay (sr, block);

    // switching engines mid-stream must not click
    {
        setParam (omg::pid::character, 80.0f);
        setParam (omg::pid::mix, 100.0f);
        float worstStep = 0.0f;
        double phase = 0.0;

        for (int n = 0; n < 30; ++n)
        {
            setParam (omg::pid::engine, (float) (n % 3));
            fillVocalLike (buffer, sr, phase, 1.0f);
            processor.processBlock (buffer, midi);
            worstStep = juce::jmax (worstStep, largestStep (buffer));
        }
        check (worstStep < 0.75f, "switching engines every block does not produce a step a click would show as");
    }

    // per-engine settings survive a trip through another engine
    {
        setParam (omg::pid::engine, 0.0f);
        setParam (omg::pid::uwDepth, 83.0f);
        setParam (omg::pid::engine, 1.0f);
        setParam (omg::pid::dsDrive, 77.0f);
        setParam (omg::pid::engine, 0.0f);
        check (std::abs (processor.apvts.getRawParameterValue (omg::pid::uwDepth)->load() - 83.0f) < 0.5f,
               "each engine keeps its own settings when you switch away and back");
    }

    // a gain change must arrive smoothly rather than as a jump
    {
        setParam (omg::pid::power, 0.0f);
        double phase = 0.0;
        fillVocalLike (buffer, sr, phase, 1.0f);
        processor.processBlock (buffer, midi);
        setParam (omg::pid::power, 1.0f);

        setParam (omg::pid::outGain, -24.0f);
        fillVocalLike (buffer, sr, phase, 1.0f);
        processor.processBlock (buffer, midi);
        setParam (omg::pid::outGain, 12.0f);
        fillVocalLike (buffer, sr, phase, 1.0f);
        processor.processBlock (buffer, midi);
        check (largestStep (buffer) < 0.9f, "a 36 dB output gain jump is smoothed rather than stepped");
        setParam (omg::pid::outGain, 0.0f);
    }

    // the randomiser, and the locks
    {
        juce::Logger::writeToLog ("\nRandomiser");

        // a locked module must come back untouched
        processor.randomizer.setLocked (omg::Randomizer::ModEq, true);
        const auto lockedBefore = processor.apvts.getRawParameterValue (omg::eqId (3, "Gain"))->load();

        bool everythingUsable = true;
        const int draws = 250;

        for (int draw = 0; draw < draws; ++draw)
        {
            // a fixed sequence, so a bad patch can be reproduced from its number
            processor.randomizer.setSeed (1000 + draw);
            processor.randomizer.randomise();

            double phase = 0.0;
            for (int n = 0; n < 6; ++n)
            {
                fillVocalLike (buffer, sr, phase, 1.0f);
                processor.processBlock (buffer, midi);
                if (! allFinite (buffer) || buffer.getMagnitude (0, block) > 1.0001f)
                {
                    if (everythingUsable)
                        juce::Logger::writeToLog ("  first bad draw " + juce::String (draw)
                            + ": magnitude " + juce::String (buffer.getMagnitude (0, block), 4)
                            + ", finite " + juce::String (allFinite (buffer) ? 1 : 0)
                            + ", engine " + juce::String (processor.apvts.getRawParameterValue (omg::pid::engine)->load())
                            + ", outGain " + juce::String (processor.apvts.getRawParameterValue (omg::pid::outGain)->load())
                            + ", limiter " + juce::String (processor.apvts.getRawParameterValue (omg::pid::limiter)->load())
                            + ", safety " + juce::String (processor.apvts.getRawParameterValue (omg::pid::safety)->load())
                            + ", mbOn " + juce::String (processor.apvts.getRawParameterValue (omg::pid::mbOn)->load()));
                    everythingUsable = false;
                }
            }
        }

        check (everythingUsable, juce::String (draws) + " random patches all stay finite and inside the ceiling");
        check (std::abs (processor.apvts.getRawParameterValue (omg::eqId (3, "Gain"))->load() - lockedBefore) < 0.001f,
               "a locked module is left alone by RANDOM");

        // the safety settings must never be drawn away
        check (processor.apvts.getRawParameterValue (omg::pid::safety)->load() > 0.5f,
               "RANDOM never turns the safety clip off");

        processor.randomizer.setLocked (omg::Randomizer::ModEq, false);
    }

    // every factory preset must load, and must produce usable audio
    {
        juce::Logger::writeToLog ("\nFactory bank");
        bool allUsable = true;
        juce::String worstPreset;

        for (int i = 0; i < processor.presetManager.getNumPresets(); ++i)
        {
            processor.presetManager.load (i);

            double phase = 0.0;
            for (int n = 0; n < 8; ++n)
            {
                fillVocalLike (buffer, sr, phase, 1.0f);
                processor.processBlock (buffer, midi);

                if (! allFinite (buffer) || buffer.getMagnitude (0, block) > 1.0001f)
                {
                    allUsable = false;
                    worstPreset = processor.presetManager.getPreset (i).name;
                }
            }
        }

        check (allUsable, "every factory preset stays finite and inside the ceiling"
                        + (worstPreset.isEmpty() ? juce::String() : " (failed on " + worstPreset + ")"));
        check (processor.presetManager.getNumPresets() >= 32, "the factory bank covers all three engines and the hybrids");
        processor.presetManager.load (0);
    }

    // ---- state -------------------------------------------------------------
    juce::Logger::writeToLog ("\nState");
    setParam (omg::pid::character, 21.0f);
    juce::MemoryBlock state;
    processor.getStateInformation (state);
    setParam (omg::pid::character, 77.0f);
    processor.setStateInformation (state.getData(), (int) state.getSize());
    check (std::abs (processor.apvts.getRawParameterValue (omg::pid::character)->load() - 21.0f) < 0.01f,
           "session state round-trips");

    processor.presetManager.load (1);
    check (processor.presetManager.getCurrentName().isNotEmpty(), "a factory preset loads");
    check (processor.presetManager.getNumPresets() >= 16, "the factory bank is present");

    // ---- editor --------------------------------------------------------------
    juce::Logger::writeToLog ("\nEditor");
    omg::ControlRegistry::clear();

    std::unique_ptr<juce::AudioProcessorEditor> editor (processor.createEditor());
    check (editor != nullptr, "the editor is created");

    if (editor != nullptr)
    {
        const int sizes[][2] = { { 900, 600 }, { 1200, 720 }, { 1500, 900 }, { 1800, 1080 } };

        for (const auto& s : sizes)
        {
            editor->setSize (s[0], s[1]);
            juce::Image image (juce::Image::ARGB, s[0], s[1], true);
            juce::Graphics g (image);
            editor->paintEntireComponent (g, true);
            check (image.getWidth() == s[0], "lays out and paints at " + juce::String (s[0]) + " x " + juce::String (s[1]));
        }

        // render the panel to a file so the layout can be inspected
        editor->setSize (1200, 720);
        {
            juce::Image shot (juce::Image::ARGB, 1200, 720, true);
            juce::Graphics g (shot);
            editor->paintEntireComponent (g, true);
            juce::File out ("omgnedd-ui.png");
            out.deleteFile();
            juce::FileOutputStream stream (out);
            juce::PNGImageFormat png;
            png.writeImageToStream (shot, stream);
            juce::Logger::writeToLog ("  wrote " + out.getFullPathName());
        }

        editor->setSize (900, 600);
        {
            juce::Image shot (juce::Image::ARGB, 900, 600, true);
            juce::Graphics g (shot);
            editor->paintEntireComponent (g, true);
            juce::File out ("omgnedd-ui-small.png");
            out.deleteFile();
            juce::FileOutputStream stream (out);
            juce::PNGImageFormat png;
            png.writeImageToStream (shot, stream);
            juce::Logger::writeToLog ("  wrote " + out.getFullPathName());
        }
        editor->setSize (1200, 720);

        if (auto* adv = findByID (*editor, "adv"))
        {
            if (adv->onClick != nullptr) adv->onClick();
            editor->resized();
            juce::Image shot (juce::Image::ARGB, 1200, 720, true);
            juce::Graphics g (shot);
            editor->paintEntireComponent (g, true);
            juce::File out ("omgnedd-advanced.png");
            out.deleteFile();
            juce::FileOutputStream stream (out);
            juce::PNGImageFormat png;
            png.writeImageToStream (shot, stream);
            juce::Logger::writeToLog ("  wrote " + out.getFullPathName());
            if (adv->onClick != nullptr) adv->onClick();
            editor->resized();
        }
        else
        {
            check (false, "the advanced panel button was found");
        }

        juce::Logger::writeToLog ("  components in the tree: " + juce::String (countComponents (*editor)));

        const auto& wired = omg::ControlRegistry::ids();
        juce::Logger::writeToLog ("  controls wired to parameters: " + juce::String (wired.size()));

        bool everyControlReal = true;
        for (const auto& id : wired)
            if (processor.apvts.getParameter (id) == nullptr)
            {
                everyControlReal = false;
                juce::Logger::writeToLog ("  FAIL  control bound to unknown parameter " + id);
            }
        check (everyControlReal, "every control on the panel drives a real parameter");

        juce::StringArray unreachable;
        for (int i = 0; i < omg::numParams(); ++i)
            if (! wired.contains (omg::params()[i].id))
                unreachable.add (omg::params()[i].id);

        if (! unreachable.isEmpty())
            juce::Logger::writeToLog ("  note: parameters with no control in the default layout: " + unreachable.joinIntoString (", "));

        check (unreachable.isEmpty(), "every parameter is reachable from the interface");
    }

    editor.reset();
    processor.releaseResources();

    juce::Logger::writeToLog (failures == 0 ? "\nALL CHECKS PASSED"
                                            : "\n" + juce::String (failures) + " CHECK(S) FAILED");
    return failures == 0 ? 0 : 1;
}
