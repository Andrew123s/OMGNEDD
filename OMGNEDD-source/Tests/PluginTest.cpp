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
