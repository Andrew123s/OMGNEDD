#pragma once
#include "OmgPanel.h"
#include "OmgKnob.h"
#include "OmgMeter.h"
#include "OmgSwitch.h"
#include "OmgLED.h"
#include "../DSP/Utils.h"

class OmgnedProcessor;

namespace omg::ui
{
    /** The output module: meter, output gain, ceiling, and the safety and
        limiter switches with a lamp that lights while the limiter is working.
    */
    class OutputSection : public OmgPanel,
                          private juce::Timer
    {
    public:
        OutputSection (juce::AudioProcessorValueTreeState& state, omg::dsp::MeterSource& source,
                       std::function<float()> limiterReduction)
            : OmgPanel ("OUTPUT"),
              meter (source),
              gain (state, pid::outGain, OmgKnob::Medium, true),
              ceiling (state, pid::ceiling, OmgKnob::Small),
              safety (state, pid::safety),
              limiter (state, pid::limiter),
              readLimiter (std::move (limiterReduction))
        {
            addAndMakeVisible (meter);
            addAndMakeVisible (gain);
            addAndMakeVisible (ceiling);
            addAndMakeVisible (safety);
            addAndMakeVisible (limiter);
            addAndMakeVisible (lamp);
            setHeaderRight (&lamp);
            startTimerHz (20);
        }

        ~OutputSection() override { stopTimer(); }

        void resized() override
        {
            OmgPanel::resized();
            auto area = getContentArea();

            auto switches = area.removeFromBottom (juce::jlimit (56, OmgSwitch::preferredBounds().getHeight(),
                                                                 area.getHeight() / 4));
            const int half = switches.getWidth() / 2;
            safety.setBounds (switches.removeFromLeft (half));
            limiter.setBounds (switches);

            area.removeFromBottom (metric::space3);
            auto knobRow = area.removeFromBottom (juce::jlimit (74, OmgKnob::preferredBounds (OmgKnob::Medium).getHeight(),
                                                                area.getHeight() / 3));
            gain.setBounds (knobRow.removeFromLeft (knobRow.getWidth() / 2));
            ceiling.setBounds (knobRow);

            area.removeFromBottom (metric::space4);
            meter.setBounds (area.withSizeKeepingCentre (juce::jmin (86, area.getWidth()), area.getHeight()));
        }

    private:
        void timerCallback() override
        {
            const float gr = readLimiter != nullptr ? readLimiter() : 0.0f;
            lamp.setState (gr > 3.0f ? OmgLED::Clip : gr > 0.15f ? OmgLED::Warn : OmgLED::Off);
        }

        OmgMeter meter;
        OmgKnob gain, ceiling;
        OmgSwitch safety, limiter;
        OmgLED lamp { "LIMITING" };
        std::function<float()> readLimiter;
    };
}
