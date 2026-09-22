#pragma once
#include "OmgPanel.h"
#include "OmgKnob.h"
#include "OmgButton.h"

namespace omg::ui
{
    /** The six vocal macros plus the three signature switches.

        Each macro moves several real DSP parameters at once, which is why the
        tooltips say what they do to the sound rather than naming one control.
    */
    class MacroPanel : public OmgPanel
    {
    public:
        explicit MacroPanel (juce::AudioProcessorValueTreeState& state)
            : OmgPanel ("VOCAL / CHARACTER"),
              punch (state, pid::sigPunch),
              chaos (state, pid::sigChaos),
              air (state, pid::sigAir)
        {
            for (auto id : { pid::macBody, pid::macColor, pid::macDamage,
                             pid::macDepth, pid::macMotion, pid::macSpace })
                addAndMakeVisible (knobs.add (new OmgKnob (state, id, OmgKnob::Medium, id == pid::macColor)));

            for (auto* b : { &punch, &chaos, &air })
            {
                b->setCompact (true);
                addAndMakeVisible (b);
            }
        }

        void resized() override
        {
            OmgPanel::resized();
            auto area = getContentArea();

            auto row = area.removeFromBottom (24);
            const int w = (row.getWidth() - 2 * metric::space1) / 3;
            punch.setBounds (row.removeFromLeft (w));
            row.removeFromLeft (metric::space1);
            chaos.setBounds (row.removeFromLeft (w));
            row.removeFromLeft (metric::space1);
            air.setBounds (row);

            area.removeFromBottom (metric::space2);

            const int perRow = 3;
            const int cellW = area.getWidth() / perRow;
            const int cellH = juce::jmax (1, area.getHeight() / 2);

            for (int i = 0; i < knobs.size(); ++i)
                knobs[i]->setBounds (area.getX() + (i % perRow) * cellW,
                                     area.getY() + (i / perRow) * cellH, cellW, cellH);
        }

    private:
        juce::OwnedArray<OmgKnob> knobs;
        OmgButton punch, chaos, air;
    };

    /** The final mix module: MIX, the dry and wet figures, PHASE and MONO. */
    class MixPanel : public OmgPanel,
                     private juce::Timer
    {
    public:
        explicit MixPanel (juce::AudioProcessorValueTreeState& state)
            : OmgPanel ("MIX / OUTPUT"),
              apvts (state),
              mix (state, pid::mix, OmgKnob::Large),
              phase (state, pid::phase),
              mono (state, pid::mono)
        {
            addAndMakeVisible (mix);

            phase.setCompact (true);
            mono.setCompact (true);
            addAndMakeVisible (phase);
            addAndMakeVisible (mono);

            startTimerHz (12);
        }

        ~MixPanel() override { stopTimer(); }

        void paint (juce::Graphics& g) override
        {
            OmgPanel::paint (g);

            auto area = getContentArea();
            auto figures = area.removeFromBottom (24 + metric::space2 + 15).removeFromTop (15);

            g.setFont (Fonts::mono (11.0f));
            g.setColour (col::inkMuted);
            g.drawText ("DRY " + juce::String (juce::roundToInt (100.0f - mixPercent)) + "%",
                        figures.removeFromLeft (figures.getWidth() / 2), juce::Justification::centredLeft, false);
            g.setColour (col::accent);
            g.drawText ("WET " + juce::String (juce::roundToInt (mixPercent)) + "%",
                        figures, juce::Justification::centredRight, false);
        }

        void resized() override
        {
            OmgPanel::resized();
            auto area = getContentArea();

            auto row = area.removeFromBottom (24);
            const int w = (row.getWidth() - metric::space2) / 2;
            phase.setBounds (row.removeFromLeft (w));
            row.removeFromLeft (metric::space2);
            mono.setBounds (row);

            area.removeFromBottom (metric::space2);
            area.removeFromBottom (15);

            mix.setBounds (area.withSizeKeepingCentre (OmgKnob::preferredBounds (OmgKnob::Large).getWidth(),
                                                       juce::jmin (area.getHeight(),
                                                                   OmgKnob::preferredBounds (OmgKnob::Large).getHeight())));
        }

    private:
        void timerCallback() override
        {
            if (auto* v = apvts.getRawParameterValue (pid::mix))
            {
                const float p = v->load();
                if (std::abs (p - mixPercent) > 0.4f) { mixPercent = p; repaint(); }
            }
        }

        juce::AudioProcessorValueTreeState& apvts;
        OmgKnob mix;
        OmgButton phase, mono;
        float mixPercent { 100.0f };
    };
}
