#pragma once
#include "OmgPanel.h"
#include "OmgKnob.h"
#include "OmgButton.h"
#include "OmgSwitch.h"
#include "ModeSelector.h"

namespace omg::ui
{
    /** The engineering panel behind ADV.

        Seven groups: DSP, distortion, saturation, underwater, sidechain, stereo
        and safety. Every control here is a parameter the audio thread reads; the
        panel only makes the less common ones reachable.
    */
    class AdvancedPanel : public juce::Component
    {
    public:
        explicit AdvancedPanel (juce::AudioProcessorValueTreeState& state) : apvts (state)
        {
            buildOversampling();
            buildRouting();
            buildDistortion();
            buildSaturation();
            buildUnderwater();
            buildModulation();
            buildSidechain();
            buildStereo();
            buildSafety();
        }

        void paint (juce::Graphics& g) override
        {
            g.setColour (col::chassis100);
            g.fillRoundedRectangle (getLocalBounds().toFloat(), (float) metric::radiusLg);
            g.setColour (col::hairline);
            g.drawRoundedRectangle (getLocalBounds().toFloat().reduced (0.5f), (float) metric::radiusLg, 1.0f);
        }

        void resized() override
        {
            auto area = getLocalBounds().reduced (metric::space3);
            const int cols = area.getWidth() > 1080 ? 3 : area.getWidth() > 720 ? 2 : 1;
            const int cellW = (area.getWidth() - (cols - 1) * metric::space3) / juce::jmax (1, cols);

            // groups keep their natural height and stack into the shortest column,
            // so the panel reads as a rack of small modules rather than a grid of
            // half-empty boxes
            std::vector<int> columnBottom ((size_t) cols, area.getY());

            for (int i = 0; i < groups.size(); ++i)
            {
                int shortest = 0;
                for (int c = 1; c < cols; ++c)
                    if (columnBottom[(size_t) c] < columnBottom[(size_t) shortest])
                        shortest = c;

                const int h = juce::jmin (heights[i], area.getBottom() - columnBottom[(size_t) shortest]);
                groups[i]->setBounds (area.getX() + shortest * (cellW + metric::space3),
                                      columnBottom[(size_t) shortest], cellW, juce::jmax (60, h));
                columnBottom[(size_t) shortest] += juce::jmax (60, h) + metric::space3;
            }

            for (auto& l : layouts)
                if (l != nullptr)
                    l();
        }

    private:
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

        void buildModulation()
        {
            auto& g = addGroup ("MODULATION", 232);
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

        juce::AudioProcessorValueTreeState& apvts;
        juce::OwnedArray<OmgPanel> groups;
        juce::Array<int> heights;
        juce::OwnedArray<OmgKnob> knobs;
        juce::OwnedArray<OmgSelector> selectors;
        juce::OwnedArray<OmgSwitch> switches;
        juce::OwnedArray<OmgButton> buttons;
        juce::Array<std::function<void()>> layouts;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AdvancedPanel)
    };
}
