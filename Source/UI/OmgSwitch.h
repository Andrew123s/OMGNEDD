#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "../Parameters.h"

namespace omg::ui
{
    /** A vertical lever switch for a standalone routing or safety choice. */
    class OmgSwitch : public juce::Button,
                      private juce::Timer
    {
    public:
        OmgSwitch (juce::AudioProcessorValueTreeState& state, juce::StringRef parameterID)
            : juce::Button (parameterID)
        {
            const auto* desc = findParam (parameterID);
            jassert (desc != nullptr);

            legend = desc != nullptr ? juce::String (desc->name) : juce::String (parameterID);
            if (desc != nullptr)
                setTooltip (juce::String (desc->name).toUpperCase() + "\n" + desc->tip);

            setClickingTogglesState (true);
            setWantsKeyboardFocus (true);
            attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (state, parameterID, *this);
            ControlRegistry::note (parameterID);
            travel = getToggleState() ? 1.0f : 0.0f;
            onStateChange = [this] { startTimerHz (60); };
        }

        static juce::Rectangle<int> preferredBounds() { return { 42, 74 }; }

        void paintButton (juce::Graphics& g, bool highlighted, bool) override
        {
            auto area = getLocalBounds();
            auto labelArea = area.removeFromBottom (14);
            auto body = juce::Rectangle<float> (34.0f, 56.0f).withCentre (area.toFloat().getCentre());

            Chassis::drawRecess (g, body, (float) metric::radiusSm);

            const float lift = travel * 23.0f;
            auto lever = juce::Rectangle<float> (body.getX() + 3.0f, body.getBottom() - 27.0f - lift, body.getWidth() - 6.0f, 24.0f);

            juce::ColourGradient grad (col::chassis300.brighter (0.28f), lever.getCentreX(), lever.getY(),
                                       juce::Colour (0xff171717), lever.getCentreX(), lever.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (lever, 2.0f);

            g.setColour (highlighted ? col::accentDim : col::lineControl);
            g.drawRoundedRectangle (lever, 2.0f, 1.0f);

            g.setColour (juce::Colours::black.withAlpha (0.5f));
            for (int i = 0; i < 3; ++i)
                g.drawLine (lever.getX() + 5.0f, lever.getCentreY() - 4.0f + (float) i * 4.0f,
                            lever.getRight() - 5.0f, lever.getCentreY() - 4.0f + (float) i * 4.0f, 1.0f);

            Fonts::drawLabel (g, legend, labelArea, 9.0f,
                              getToggleState() ? col::accent : col::ink, 0.10f,
                              juce::Justification::centred, true);

            if (hasKeyboardFocus (false))
            {
                g.setColour (col::accent);
                g.drawRoundedRectangle (body.expanded (2.0f), (float) metric::radiusSm + 1.0f, 2.0f);
            }
        }

    private:
        void timerCallback() override
        {
            const float target = getToggleState() ? 1.0f : 0.0f;
            travel += (target - travel) * 0.35f;

            if (std::abs (target - travel) < 0.005f)
            {
                travel = target;
                stopTimer();
            }
            repaint();
        }

        juce::String legend;
        float travel { 0.0f };
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmgSwitch)
    };
}
