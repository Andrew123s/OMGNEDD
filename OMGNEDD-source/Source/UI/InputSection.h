#pragma once
#include "OmgPanel.h"
#include "OmgKnob.h"
#include "OmgMeter.h"
#include "../DSP/Utils.h"

namespace omg::ui
{
    /** The input module: level meter with peak hold and clip, and input gain. */
    class InputSection : public OmgPanel
    {
    public:
        InputSection (juce::AudioProcessorValueTreeState& state, omg::dsp::MeterSource& source)
            : OmgPanel ("INPUT"), meter (source), gain (state, pid::inGain, OmgKnob::Medium, true)
        {
            addAndMakeVisible (meter);
            addAndMakeVisible (gain);
        }

        void resized() override
        {
            OmgPanel::resized();
            auto area = getContentArea();

            auto knobArea = area.removeFromBottom (juce::jlimit (74, OmgKnob::preferredBounds (OmgKnob::Medium).getHeight(),
                                                                 area.getHeight() / 3));
            gain.setBounds (knobArea);

            area.removeFromBottom (metric::space4);
            meter.setBounds (area.withSizeKeepingCentre (juce::jmin (86, area.getWidth()), area.getHeight()));
        }

    private:
        OmgMeter meter;
        OmgKnob gain;
    };
}
