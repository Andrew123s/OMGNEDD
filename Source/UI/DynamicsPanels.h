#pragma once
#include "OmgPanel.h"
#include "OmgKnob.h"
#include "OmgButton.h"
#include "OmgMeter.h"
#include "ModeSelector.h"

namespace omg::ui
{
    /** The compressor module: seven controls, a character selector, AUTO, the
        sidechain filter and a gain reduction meter.
    */
    class CompressorPanel : public OmgPanel
    {
    public:
        CompressorPanel (juce::AudioProcessorValueTreeState& state, std::function<float()> reduction)
            : OmgPanel ("COMPRESSOR"),
              enable (state, pid::compOn),
              modes (state, pid::compMode, 3, false),
              autoButton (state, pid::compAuto),
              grMeter ("GAIN REDUCTION", 20.0f, std::move (reduction))
        {
            enable.setLegend ("COMP ON");
            enable.setCompact (true);
            addAndMakeVisible (enable);
            setHeaderRight (&enable);

            static const char* legends[] = { "THRESHOLD", "RATIO", "ATTACK", "RELEASE",
                                             "KNEE", "MAKEUP", "MIX", "SC HPF" };
            int n = 0;
            for (auto id : { pid::compThresh, pid::compRatio, pid::compAttack, pid::compRelease,
                             pid::compKnee, pid::compMakeup, pid::compMix, pid::compScHpf })
            {
                auto* k = knobs.add (new OmgKnob (state, id, OmgKnob::Small, id == pid::compMakeup));
                k->setLabelText (legends[n++]);
                addAndMakeVisible (k);
            }

            autoButton.setLegend ("AUTO");
            autoButton.setCompact (true);

            addAndMakeVisible (modes);
            addAndMakeVisible (autoButton);
            addAndMakeVisible (grMeter);
        }

        void resized() override
        {
            OmgPanel::resized();
            auto area = getContentArea();

            grMeter.setBounds (area.removeFromBottom (30));
            area.removeFromBottom (metric::space2);

            auto modeRow = area.removeFromBottom (modes.getPreferredHeight());
            autoButton.setBounds (modeRow.removeFromRight (48).withSizeKeepingCentre (48, 24));
            modeRow.removeFromRight (metric::space2);
            modes.setBounds (modeRow);
            area.removeFromBottom (metric::space2);

            // the second row of refinements appears once there is height for it;
            // KNEE, MAKEUP, MIX and SIDECHAIN HPF are always in the advanced panel
            const int perRow = 4;
            const int rows = area.getHeight() >= 140 ? 2 : 1;
            const int visible = juce::jmin (knobs.size(), rows * perRow);
            const int cellW = area.getWidth() / perRow;
            const int cellH = juce::jmax (1, area.getHeight() / rows);

            for (int i = 0; i < knobs.size(); ++i)
            {
                knobs[i]->setVisible (i < visible);
                if (i < visible)
                    knobs[i]->setBounds (area.getX() + (i % perRow) * cellW,
                                         area.getY() + (i / perRow) * cellH, cellW, cellH);
            }
        }

    private:
        OmgButton enable;
        juce::OwnedArray<OmgKnob> knobs;
        OmgSelector modes;
        OmgButton autoButton;
        OmgGrMeter grMeter;
    };

    /** The de-esser: six controls, LISTEN, and its own reduction meter. */
    class DeEsserPanel : public OmgPanel
    {
    public:
        DeEsserPanel (juce::AudioProcessorValueTreeState& state, std::function<float()> reduction)
            : OmgPanel ("DE-ESSER"),
              enable (state, pid::deOn),
              listen (state, pid::deListen),
              grMeter ("REDUCTION", 12.0f, std::move (reduction))
        {
            enable.setLegend ("DE-ESS ON");
            enable.setCompact (true);
            addAndMakeVisible (enable);
            setHeaderRight (&enable);

            static const char* legends[] = { "FREQ", "THRESHOLD", "AMOUNT", "RANGE", "ATTACK", "RELEASE" };
            int n = 0;
            for (auto id : { pid::deFreq, pid::deThresh, pid::deAmount,
                             pid::deRange, pid::deAttack, pid::deRelease })
            {
                auto* k = knobs.add (new OmgKnob (state, id, OmgKnob::Small));
                k->setLabelText (legends[n++]);
                addAndMakeVisible (k);
            }

            listen.setLegend ("LISTEN");
            listen.setCompact (true);
            listen.setLedState (OmgLED::Warn);
            addAndMakeVisible (listen);
            addAndMakeVisible (grMeter);
        }

        void resized() override
        {
            OmgPanel::resized();
            auto area = getContentArea();

            grMeter.setBounds (area.removeFromBottom (30));
            area.removeFromBottom (metric::space2);
            listen.setBounds (area.removeFromBottom (24).removeFromLeft (80));
            area.removeFromBottom (metric::space2);

            const int perRow = 3;
            const int rows = area.getHeight() >= 140 ? 2 : 1;
            const int visible = juce::jmin (knobs.size(), rows * perRow);
            const int cellW = area.getWidth() / perRow;
            const int cellH = juce::jmax (1, area.getHeight() / rows);

            for (int i = 0; i < knobs.size(); ++i)
            {
                knobs[i]->setVisible (i < visible);
                if (i < visible)
                    knobs[i]->setBounds (area.getX() + (i % perRow) * cellW,
                                         area.getY() + (i / perRow) * cellH, cellW, cellH);
            }
        }

    private:
        OmgButton enable, listen;
        juce::OwnedArray<OmgKnob> knobs;
        OmgGrMeter grMeter;
    };
}
