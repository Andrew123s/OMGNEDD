#pragma once
#include "OmgPanel.h"
#include "OmgKnob.h"
#include "OmgButton.h"
#include "OmgSwitch.h"
#include "ModeSelector.h"

namespace omg::ui
{
    /** The engineering panel behind ADV.

        Every control here is a parameter the audio thread reads; the panel only
        makes the less common ones reachable, so the main interface can stay
        short. The groups are laid out into the shortest column each time, so
        the panel reads as a rack of small modules.
    */
    class AdvancedPanel : public juce::Component,
                          private juce::Timer
    {
    public:
        AdvancedPanel (juce::AudioProcessorValueTreeState& state,
                       std::function<int()> latencyProvider,
                       std::function<bool()> sidechainProvider)
            : apvts (state),
              getLatency (std::move (latencyProvider)),
              getSidechainConnected (std::move (sidechainProvider))
        {
            buildOversampling();
            buildRouting();
            buildDistortion();
            buildSaturation();
            buildUnderwater();
            buildFilterMotion();
            buildPitchModulation();
            buildTransient();
            buildMultiband();
            buildSidechain();
            buildDetector();
            buildBlend();
            buildStereo();
            buildSafety();

            startTimerHz (4);
        }

        ~AdvancedPanel() override { stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            g.setColour (col::chassis100);
            g.fillRoundedRectangle (getLocalBounds().toFloat(), (float) metric::radiusLg);
            g.setColour (col::hairline);
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), (float) metric::radiusLg, 1.0f);

            // the figures a host user actually needs: what the oversampler is
            // costing them in delay, and whether the sidechain has a source
            auto strip = getLocalBounds().removeFromBottom (metric::space3 + 16).reduced (metric::space4, 0);
            g.setColour (col::chassis000);
            g.fillRect (strip.expanded (metric::space4, 0));
            g.setFont (Fonts::mono (11.0f));
            g.setColour (col::inkMuted);
            g.drawText (statusText, strip, juce::Justification::centredLeft, false);
        }

        /** How tall the panel needs to be to show every group at the given
            width. The editor puts the panel in a viewport at this height, so a
            group is never clipped and nothing on it becomes unreachable. */
        int preferredHeightForWidth (int widthToUse) const
        {
            return pack (juce::Rectangle<int> (0, 0, widthToUse, 10).reduced (metric::space3, metric::space3), nullptr)
                 + metric::space3 * 2 + 16;
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (metric::space3);
            area.removeFromBottom (16);                       // the status strip

            pack (area, this);

            for (auto& l : layouts)
                if (l != nullptr)
                    l();
        }

    private:
        /** Groups keep their natural height and drop into the shortest column,
            so the panel reads as a rack of small modules rather than a grid of
            half-empty boxes. Returns the height the tallest column needs.
            With a null @p applyTo it measures without moving anything. */
        int pack (juce::Rectangle<int> area, const AdvancedPanel* applyTo) const
        {
            const int cols = area.getWidth() > 1080 ? 3 : area.getWidth() > 720 ? 2 : 1;
            const int cellW = (area.getWidth() - (cols - 1) * metric::space3) / juce::jmax (1, cols);

            std::vector<int> columnBottom ((size_t) cols, area.getY());

            for (int i = 0; i < groups.size(); ++i)
            {
                int shortest = 0;
                for (int c = 1; c < cols; ++c)
                    if (columnBottom[(size_t) c] < columnBottom[(size_t) shortest])
                        shortest = c;

                const int h = juce::jmax (60, heights[i]);

                if (applyTo != nullptr)
                    groups[i]->setBounds (area.getX() + shortest * (cellW + metric::space3),
                                          columnBottom[(size_t) shortest], cellW, h);

                columnBottom[(size_t) shortest] += h + metric::space3;
            }

            int tallest = area.getY();
            for (int c = 0; c < cols; ++c)
                tallest = juce::jmax (tallest, columnBottom[(size_t) c]);

            return tallest - area.getY();
        }

        OmgPanel& addGroup (const juce::String& title, int preferredHeight)
        {
            auto* p = groups.add (new OmgPanel (title, false));
            heights.add (preferredHeight);
            addAndMakeVisible (p);
            return *p;
        }

        OmgKnob* addKnob (OmgPanel& panel, juce::StringRef id, const char* legend, bool bipolar = false)
        {
            auto* k = addKnob (panel, id, bipolar);
            k->setLabelText (legend);
            return k;
        }

        OmgKnob* addKnob (OmgPanel& panel, juce::StringRef id, bool bipolar = false)
        {
            auto* k = knobs.add (new OmgKnob (apvts, id, OmgKnob::Small, bipolar));
            panel.addAndMakeVisible (k);
            return k;
        }

        static void layoutGrid (juce::Rectangle<int> area, const std::vector<juce::Component*>& items, int perRow)
        {
            if (items.empty()) return;
            const int rows = ((int) items.size() + perRow - 1) / juce::jmax (1, perRow);
            const int cellW = area.getWidth() / juce::jmax (1, perRow);
            const int cellH = juce::jmax (1, area.getHeight() / juce::jmax (1, rows));

            for (size_t i = 0; i < items.size(); ++i)
                items[i]->setBounds (area.getX() + ((int) i % perRow) * cellW,
                                     area.getY() + ((int) i / perRow) * cellH, cellW, cellH);
        }

        void buildOversampling()
        {
            auto& g = addGroup ("DSP / OVERSAMPLING", 100);
            auto* sel = selectors.add (new OmgSelector (apvts, pid::oversampling, 4, false));
            g.addAndMakeVisible (sel);
            auto* panel = &g;

            layouts.add ([panel, sel]
            {
                auto a = panel->getContentArea();
                sel->setBounds (a.removeFromTop (sel->getPreferredHeight()));
            });
        }

        void buildRouting()
        {
            auto& g = addGroup ("ROUTING", 150);
            std::vector<juce::Component*> items;
            for (auto id : { pid::eqOn, pid::compOn, pid::deOn, pid::phase, pid::mono })
            {
                auto* sw = switches.add (new OmgSwitch (apvts, id));
                g.addAndMakeVisible (sw);
                items.push_back (sw);
            }
            auto* panel = &g;
            layouts.add ([panel, items]
            {
                auto a = panel->getContentArea();
                layoutGrid (a.removeFromTop (juce::jmin (a.getHeight(), OmgSwitch::preferredBounds().getHeight())), items, 5);
            });
        }

        void buildFilterMotion()
        {
            auto& g = addGroup ("FILTER MOTION", 232);
            std::vector<juce::Component*> items {
                addKnob (g, pid::uwModPhase, "MOD PHASE"), addKnob (g, pid::uwRipple, "RIPPLE"),
                addKnob (g, pid::uwWave, "WAVE") };

            auto* shape = selectors.add (new OmgSelector (apvts, pid::uwModShape, 4, false));
            auto* chaosButton = buttons.add (new OmgButton (apvts, pid::sigChaos));
            chaosButton->setCompact (true);
            g.addAndMakeVisible (shape);
            g.addAndMakeVisible (chaosButton);

            auto* panel = &g;
            layouts.add ([panel, items, shape, chaosButton]
            {
                auto a = panel->getContentArea();
                layoutGrid (a.removeFromTop (OmgKnob::preferredBounds (OmgKnob::Small).getHeight()), items, 3);
                a.removeFromTop (metric::space2);
                shape->setBounds (a.removeFromTop (shape->getPreferredHeight()));
                a.removeFromTop (metric::space2);
                chaosButton->setBounds (a.removeFromTop (juce::jmin (24, juce::jmax (0, a.getHeight()))).removeFromLeft (90));
            });
        }

        void buildPitchModulation()
        {
            auto& g = addGroup ("PITCH / MODULATION", 272);

            auto* on = switches.add (new OmgSwitch (apvts, pid::modOn));
            g.addAndMakeVisible (on);

            std::vector<juce::Component*> row1 {
                addKnob (g, pid::modRate, "RATE"), addKnob (g, pid::modDepth, "DEPTH"),
                addKnob (g, pid::modDetune, "DETUNE") };
            std::vector<juce::Component*> row2 {
                addKnob (g, pid::modWidth, "WIDTH"), addKnob (g, pid::modMotion, "MOTION"),
                addKnob (g, pid::modMix, "MIX") };

            auto* mode = choices.add (new OmgChoiceButton (apvts, pid::modMode, "MOD MODE"));
            g.addAndMakeVisible (mode);

            auto* panel = &g;
            layouts.add ([panel, on, row1, row2, mode]
            {
                auto a = panel->getContentArea();
                const int knobH = OmgKnob::preferredBounds (OmgKnob::Small).getHeight();

                auto top = a.removeFromTop (juce::jmax (knobH, OmgSwitch::preferredBounds().getHeight()));
                on->setBounds (top.removeFromRight (OmgSwitch::preferredBounds().getWidth()));
                layoutGrid (top, row1, 3);

                a.removeFromTop (metric::space2);
                layoutGrid (a.removeFromTop (knobH), row2, 3);
                a.removeFromTop (metric::space2);
                mode->setBounds (a.removeFromTop (juce::jmin (a.getHeight(), 40)));
            });
        }

        void buildTransient()
        {
            auto& g = addGroup ("VOCAL ATTACK", 160);

            auto* on = switches.add (new OmgSwitch (apvts, pid::trOn));
            g.addAndMakeVisible (on);

            std::vector<juce::Component*> items {
                addKnob (g, pid::trAttack, "ATTACK", true),
                addKnob (g, pid::trBody, "BODY", true) };

            auto* panel = &g;
            layouts.add ([panel, on, items]
            {
                auto a = panel->getContentArea();
                auto row = a.removeFromTop (juce::jmax (OmgSwitch::preferredBounds().getHeight(),
                                                        OmgKnob::preferredBounds (OmgKnob::Small).getHeight()));
                on->setBounds (row.removeFromRight (OmgSwitch::preferredBounds().getWidth()));
                layoutGrid (row, items, 2);
            });
        }

        void buildMultiband()
        {
            auto& g = addGroup ("MULTIBAND DRIVE", 250);

            auto* on = switches.add (new OmgSwitch (apvts, pid::mbOn));
            g.addAndMakeVisible (on);

            std::vector<juce::Component*> bands {
                addKnob (g, pid::mbLow, "LOW", true),
                addKnob (g, pid::mbMid, "MID", true),
                addKnob (g, pid::mbHigh, "HIGH", true) };

            std::vector<juce::Component*> crossovers {
                addKnob (g, pid::mbCrossLow, "X-LOW"),
                addKnob (g, pid::mbCrossHigh, "X-HIGH") };

            auto* panel = &g;
            layouts.add ([panel, on, bands, crossovers]
            {
                auto a = panel->getContentArea();
                const int knobH = OmgKnob::preferredBounds (OmgKnob::Small).getHeight();

                auto top = a.removeFromTop (juce::jmax (knobH, OmgSwitch::preferredBounds().getHeight()));
                on->setBounds (top.removeFromRight (OmgSwitch::preferredBounds().getWidth()));
                layoutGrid (top, bands, 3);

                a.removeFromTop (metric::space2);
                layoutGrid (a.removeFromTop (juce::jmin (a.getHeight(), knobH)), crossovers, 2);
            });
        }

        void buildDetector()
        {
            auto& g = addGroup ("COMPRESSOR DETECTOR", 258);

            auto* place  = selectors.add (new OmgSelector (apvts, pid::compPlace, 2, false));
            auto* detect = selectors.add (new OmgSelector (apvts, pid::compDetect, 2, false));
            auto* placeCaption  = captions.add (new OmgCaption ("PLACEMENT"));
            auto* detectCaption = captions.add (new OmgCaption ("DETECTION"));

            for (auto* c : std::initializer_list<juce::Component*> { place, detect, placeCaption, detectCaption })
                g.addAndMakeVisible (c);

            auto* autoRel = switches.add (new OmgSwitch (apvts, pid::compAutoRel));
            auto* ext     = switches.add (new OmgSwitch (apvts, pid::compScExt));
            g.addAndMakeVisible (autoRel);
            g.addAndMakeVisible (ext);

            auto* amount = addKnob (g, pid::compScAmount, "SC AMOUNT");

            auto* panel = &g;
            layouts.add ([panel, place, detect, placeCaption, detectCaption, autoRel, ext, amount]
            {
                auto a = panel->getContentArea();
                placeCaption->setBounds (a.removeFromTop (13));
                place->setBounds (a.removeFromTop (place->getPreferredHeight()));
                a.removeFromTop (metric::space2);
                detectCaption->setBounds (a.removeFromTop (13));
                detect->setBounds (a.removeFromTop (detect->getPreferredHeight()));
                a.removeFromTop (metric::space2);

                auto row = a.removeFromTop (juce::jmin (a.getHeight(),
                                                        juce::jmax (OmgSwitch::preferredBounds().getHeight(),
                                                                    OmgKnob::preferredBounds (OmgKnob::Small).getHeight())));
                const int w = row.getWidth() / 3;
                autoRel->setBounds (row.removeFromLeft (w));
                ext->setBounds (row.removeFromLeft (w));
                amount->setBounds (row);
            });
        }

        void buildBlend()
        {
            auto& g = addGroup ("GAIN STAGING", 168);

            std::vector<juce::Component*> items {
                addKnob (g, pid::dryLevel, "DRY", true),
                addKnob (g, pid::wetLevel, "WET", true),
                addKnob (g, pid::inGain, "IN", true),
                addKnob (g, pid::outGain, "OUT", true) };

            auto* panel = &g;
            layouts.add ([panel, items] { layoutGrid (panel->getContentArea(), items, 4); });
        }

        void buildDistortion()
        {
            auto& g = addGroup ("DISTORTION", 154);
            std::vector<juce::Component*> items {
                addKnob (g, pid::dsBias, "BIAS", true), addKnob (g, pid::dsAsym, "ASYM", true),
                addKnob (g, pid::dsPreEmph, "PRE-EMPH"), addKnob (g, pid::dsPostFilter, "POST LPF") };
            auto* panel = &g;
            layouts.add ([panel, items] { layoutGrid (panel->getContentArea(), items, 4); });
        }

        void buildSaturation()
        {
            auto& g = addGroup ("SATURATION", 154);
            std::vector<juce::Component*> items {
                addKnob (g, pid::satDensity, "DENSITY"), addKnob (g, pid::satHarmonics, "HARMONICS"),
                addKnob (g, pid::satWarmth, "WARMTH") };
            auto* panel = &g;
            layouts.add ([panel, items] { layoutGrid (panel->getContentArea(), items, 3); });
        }

        void buildUnderwater()
        {
            auto& g = addGroup ("UNDERWATER", 190);
            std::vector<juce::Component*> items {
                addKnob (g, pid::uwResonance, "RESONANCE"), addKnob (g, pid::uwPressResp, "RESPONSE"),
                addKnob (g, pid::uwMurk, "MURK") };

            auto* slope = selectors.add (new OmgSelector (apvts, pid::uwSlope, 4, false));
            g.addAndMakeVisible (slope);

            auto* panel = &g;
            layouts.add ([panel, items, slope]
            {
                auto a = panel->getContentArea();
                layoutGrid (a.removeFromTop (OmgKnob::preferredBounds (OmgKnob::Small).getHeight()), items, 3);
                a.removeFromTop (metric::space2);
                slope->setBounds (a.removeFromTop (slope->getPreferredHeight()));
            });
        }

        void buildSidechain()
        {
            auto& g = addGroup ("COMPRESSOR DETAIL", 154);
            std::vector<juce::Component*> items {
                addKnob (g, pid::compScHpf, "SC HPF"), addKnob (g, pid::compKnee, "KNEE"),
                addKnob (g, pid::compMakeup, "MAKEUP", true), addKnob (g, pid::compMix, "MIX") };
            auto* panel = &g;
            layouts.add ([panel, items] { layoutGrid (panel->getContentArea(), items, 4); });
        }

        void buildStereo()
        {
            auto& g = addGroup ("STEREO", 160);
            std::vector<juce::Component*> items {
                addKnob (g, pid::stWidth, "WIDTH"), addKnob (g, pid::stSide, "SIDE", true),
                addKnob (g, pid::stMonoComp, "MONO COMP") };

            auto* ms = switches.add (new OmgSwitch (apvts, pid::stMS));
            g.addAndMakeVisible (ms);

            auto* panel = &g;
            layouts.add ([panel, items, ms]
            {
                auto a = panel->getContentArea();
                auto row = a.removeFromTop (juce::jmax (OmgSwitch::preferredBounds().getHeight(),
                                                        OmgKnob::preferredBounds (OmgKnob::Small).getHeight()));
                ms->setBounds (row.removeFromRight (OmgSwitch::preferredBounds().getWidth()));
                layoutGrid (row, items, 3);
            });
        }

        void buildSafety()
        {
            auto& g = addGroup ("SAFETY", 160);
            auto* limiter  = switches.add (new OmgSwitch (apvts, pid::limiter));
            auto* safety   = switches.add (new OmgSwitch (apvts, pid::safety));
            auto* truePeak = switches.add (new OmgSwitch (apvts, pid::truePeak));
            for (auto* s : { limiter, safety, truePeak })
                g.addAndMakeVisible (s);

            auto* ceiling = addKnob (g, pid::ceiling, "CEILING");
            auto* panel = &g;

            layouts.add ([panel, limiter, safety, truePeak, ceiling]
            {
                auto a = panel->getContentArea();
                auto row = a.removeFromTop (juce::jmax (OmgSwitch::preferredBounds().getHeight(),
                                                        OmgKnob::preferredBounds (OmgKnob::Small).getHeight()));
                const int w = row.getWidth() / 4;
                limiter->setBounds (row.removeFromLeft (w));
                safety->setBounds (row.removeFromLeft (w));
                truePeak->setBounds (row.removeFromLeft (w));
                ceiling->setBounds (row);
            });
        }

        void timerCallback() override
        {
            const int latency = getLatency != nullptr ? getLatency() : 0;
            const bool sc = getSidechainConnected != nullptr && getSidechainConnected();

            auto next = "LATENCY " + juce::String (latency) + " SAMPLES"
                      + "     SIDECHAIN " + juce::String (sc ? "CONNECTED" : "NOT CONNECTED")
                      + "     ALL CONTROLS ON THIS PANEL ARE AUTOMATABLE PARAMETERS";

            if (next != statusText) { statusText = next; repaint(); }
        }

        juce::AudioProcessorValueTreeState& apvts;
        std::function<int()> getLatency;
        std::function<bool()> getSidechainConnected;
        juce::String statusText;
        juce::OwnedArray<OmgPanel> groups;
        juce::Array<int> heights;
        juce::OwnedArray<OmgKnob> knobs;
        juce::OwnedArray<OmgSelector> selectors;
        juce::OwnedArray<OmgSwitch> switches;
        juce::OwnedArray<OmgButton> buttons;
        juce::OwnedArray<OmgCaption> captions;
        juce::OwnedArray<OmgChoiceButton> choices;
        juce::Array<std::function<void()>> layouts;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdvancedPanel)
    };
}
