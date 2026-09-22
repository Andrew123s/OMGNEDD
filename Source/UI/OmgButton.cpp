#include "OmgButton.h"

namespace omg::ui
{
    OmgButton::OmgButton (juce::AudioProcessorValueTreeState& state, juce::StringRef parameterID)
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
    }

    OmgButton::OmgButton (const juce::String& buttonLegend, std::function<void()> action)
        : juce::Button (buttonLegend), legend (buttonLegend)
    {
        showLed = false;
        setClickingTogglesState (false);
        setWantsKeyboardFocus (true);
        if (action != nullptr)
            onClick = std::move (action);
    }

    void OmgButton::paintButton (juce::Graphics& g, bool highlighted, bool down)
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);
        const bool on = isLit();
        const float corner = (float) metric::radiusSm;

        if (on)
        {
            juce::ColourGradient grad (juce::Colour (0xff232a16), r.getCentreX(), r.getY(),
                                       juce::Colour (0xff141a0c), r.getCentreX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, corner);
            g.setColour (juce::Colours::black.withAlpha (0.55f));
            g.drawLine (r.getX() + corner, r.getY() + 1.0f, r.getRight() - corner, r.getY() + 1.0f, 1.5f);
        }
        else
        {
            juce::ColourGradient grad (col::chassis300.brighter (down ? 0.0f : 0.10f), r.getCentreX(), r.getY(),
                                       juce::Colour (0xff1b1b1b), r.getCentreX(), r.getBottom(), false);
            g.setGradientFill (grad);
            g.fillRoundedRectangle (r, corner);
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.drawLine (r.getX() + corner, r.getY() + 0.5f, r.getRight() - corner, r.getY() + 0.5f, 1.0f);
        }

        g.setColour (on ? col::accentDim : (highlighted ? col::accentDim : col::lineControl));
        g.drawRoundedRectangle (r, corner, 1.0f);

        auto legendArea = getLocalBounds();
        const float pad = compact ? 6.0f : 12.0f;
        legendArea.removeFromLeft (juce::roundToInt (pad));
        legendArea.removeFromRight (juce::roundToInt (pad));

        if (showLed)
        {
            auto lensArea = legendArea.removeFromLeft (14);
            auto lens = juce::Rectangle<float> (7.0f, 7.0f).withCentre (lensArea.toFloat().getCentre().withX (lensArea.toFloat().getX() + 3.5f));
            const auto c = on ? OmgLED::colourFor (ledState) : col::ledOff;

            if (on)
            {
                g.setColour (c.withAlpha (0.30f));
                g.fillEllipse (lens.expanded (3.5f));
            }
            g.setColour (c);
            g.fillEllipse (lens);
        }

        Fonts::drawLabel (g, legend, legendArea, compact ? 9.0f : 10.0f,
                          on ? col::accent : col::ink, 0.14f,
                          (leftAligned || showLed) ? juce::Justification::centredLeft : juce::Justification::centred,
                          true);

        if (hasKeyboardFocus (false))
        {
            g.setColour (col::accent);
            g.drawRoundedRectangle (r.expanded (1.5f), corner + 1.0f, 2.0f);
        }
    }
}
