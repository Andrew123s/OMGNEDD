#include "OmgKnob.h"

namespace omg::ui
{
    float OmgKnob::clipboardValue = 0.0f;
    bool  OmgKnob::clipboardValid = false;

    static constexpr float kStartAngle = juce::MathConstants<float>::pi * 1.25f;   // -135 deg
    static constexpr float kEndAngle   = juce::MathConstants<float>::pi * 2.75f;   // +135 deg

    OmgKnob::OmgKnob (juce::AudioProcessorValueTreeState& state, juce::StringRef parameterID,
                      Size size, bool bipolar)
        : apvts (state), paramID (parameterID), knobSize (size), isBipolar (bipolar)
    {
        desc = findParam (parameterID);
        jassert (desc != nullptr);                       // no control without a parameter

        labelText = desc != nullptr ? juce::String (desc->name) : juce::String (parameterID);

        setSliderStyle (juce::Slider::RotaryVerticalDrag);
        setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        setRotaryParameters (kStartAngle, kEndAngle, true);
        setMouseDragSensitivity (220);
        setWantsKeyboardFocus (true);
        setVelocityBasedMode (false);

        attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (apvts, paramID, *this);
        ControlRegistry::note (paramID);
        refreshTooltip();

        onValueChange = [this] { refreshTooltip(); repaint(); };
    }

    OmgKnob::~OmgKnob() = default;

    int OmgKnob::labelBlockFor (Size size) { return size == Large ? 34 : 26; }

    int OmgKnob::nominalDiameter (Size size)
    {
        return size == Large ? metric::knobLarge : size == Small ? metric::knobSmall : metric::knobMedium;
    }

    int OmgKnob::getDialDiameter() const
    {
        // never draw outside the cell the layout gave us: the dial shrinks, it
        // does not overflow, and it stays circular at every size
        const int available = juce::jmin (getWidth() - 4, getHeight() - labelBlockFor (knobSize));
        return juce::jlimit (24, nominalDiameter (knobSize), available);
    }

    juce::Rectangle<int> OmgKnob::preferredBounds (Size size)
    {
        const int d = nominalDiameter (size);
        return { juce::jmax (d + 16, 72), d + labelBlockFor (size) };
    }

    juce::String OmgKnob::getTextFromValue (double value)
    {
        if (desc == nullptr)
            return juce::String (value, 2);

        if (desc->type == PType::Choice)
        {
            auto choices = juce::StringArray::fromTokens (juce::String (desc->choices), "|", "");
            return choices[juce::jlimit (0, choices.size() - 1, (int) std::round (value))];
        }

        const float span = desc->max - desc->min;
        const int decimals = span > 400.0f ? 0 : span > 40.0f ? 1 : 2;

        juce::String text (value, decimals);
        if (desc->unit != nullptr)
            text += juce::String (" ") + desc->unit;

        return text;
    }

    void OmgKnob::refreshTooltip()
    {
        if (desc == nullptr) return;
        setTooltip (juce::String (desc->name).toUpperCase() + "\n" + desc->tip + "\nCurrent: " + getTextFromValue (getValue()));
    }

    juce::String OmgKnob::getTooltip()
    {
        refreshTooltip();
        return juce::SettableTooltipClient::getTooltip();
    }

    void OmgKnob::mouseEnter (const juce::MouseEvent& e) { hovering = true;  repaint(); juce::Slider::mouseEnter (e); }
    void OmgKnob::mouseExit  (const juce::MouseEvent& e) { hovering = false; repaint(); juce::Slider::mouseExit (e); }

    void OmgKnob::mouseDown (const juce::MouseEvent& e)
    {
        if (e.mods.isPopupMenu())
        {
            showParameterMenu();
            return;
        }

        dragging = true;
        setMouseDragSensitivity (e.mods.isShiftDown() ? 900 : 220);
        repaint();
        juce::Slider::mouseDown (e);
    }

    void OmgKnob::mouseUp (const juce::MouseEvent& e)
    {
        dragging = false;
        repaint();
        juce::Slider::mouseUp (e);
    }

    void OmgKnob::mouseDoubleClick (const juce::MouseEvent&)
    {
        if (desc != nullptr)
            setValue ((double) desc->def, juce::sendNotificationSync);
    }

    void OmgKnob::showParameterMenu()
    {
        juce::PopupMenu menu;
        menu.setLookAndFeel (&getLookAndFeel());
        menu.addSectionHeader (juce::String (desc != nullptr ? desc->name : "PARAMETER").toUpperCase());
        menu.addItem (1, "Reset to default");
        menu.addItem (2, "Enter value...");
        menu.addItem (3, "Copy value");
        menu.addItem (4, "Paste value", clipboardValid);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                            [this] (int result)
        {
            switch (result)
            {
                case 1:
                    if (desc != nullptr) setValue ((double) desc->def, juce::sendNotificationSync);
                    break;

                case 2:
                {
                    auto* window = new juce::AlertWindow (juce::String (desc != nullptr ? desc->name : "Value"),
                                                          "Enter a new value", juce::MessageBoxIconType::NoIcon);
                    window->addTextEditor ("value", juce::String (getValue(), 3));
                    window->addButton ("Set", 1, juce::KeyPress (juce::KeyPress::returnKey));
                    window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));
                    window->enterModalState (true, juce::ModalCallbackFunction::create (
                        [this, window] (int r)
                        {
                            if (r == 1)
                                setValue (window->getTextEditorContents ("value").getDoubleValue(), juce::sendNotificationSync);
                            delete window;
                        }), false);
                    break;
                }

                case 3: clipboardValue = (float) getValue(); clipboardValid = true; break;
                case 4: if (clipboardValid) setValue ((double) clipboardValue, juce::sendNotificationSync); break;
                default: break;
            }
        });
    }

    void OmgKnob::paint (juce::Graphics& g)
    {
        const int d = getDialDiameter();
        auto area = getLocalBounds();
        const int dialTop = area.getY() + juce::jmax (0, (area.getHeight() - labelBlockFor (knobSize) - d) / 2);
        auto dial = juce::Rectangle<int> (d, d).withCentre ({ area.getCentreX(), dialTop + d / 2 }).toFloat();

        const bool lit = dragging || hovering;
        const double range = getMaximum() - getMinimum();
        const float norm = range > 0.0 ? (float) ((getValue() - getMinimum()) / range) : 0.0f;

        const float stroke = juce::jmax (3.0f, (float) d * 0.055f);
        auto arcBounds = dial.reduced (stroke * 0.5f + 1.0f);
        const float radius = arcBounds.getWidth() * 0.5f;
        const auto centre = dial.getCentre();

        // --- arc track -------------------------------------------------------
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, kStartAngle, kEndAngle, true);
        g.setColour (col::accentDeep);
        g.strokePath (track, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::butt));

        // --- arc fill --------------------------------------------------------
        const float from = isBipolar ? 0.5f : 0.0f;
        const float a = juce::jmin (from, norm), b = juce::jmax (from, norm);
        if (b - a > 0.001f)
        {
            juce::Path fill;
            fill.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                                kStartAngle + a * (kEndAngle - kStartAngle),
                                kStartAngle + b * (kEndAngle - kStartAngle), true);
            g.setColour (lit ? col::accent : col::accent.withAlpha (0.88f));
            g.strokePath (fill, juce::PathStrokeType (stroke, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
        }

        // --- body ------------------------------------------------------------
        auto body = dial.reduced ((float) d * 0.135f);
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillEllipse (body.translated (0.0f, (float) d * 0.035f));

        juce::ColourGradient metal (col::chassis300.brighter (0.22f), body.getCentreX(), body.getY(),
                                    juce::Colour (0xff141414), body.getCentreX(), body.getBottom(), false);
        metal.addColour (0.38, col::chassis300);
        g.setGradientFill (metal);
        g.fillEllipse (body);

        g.setColour (lit ? col::accentDim : col::lineControl);
        g.drawEllipse (body.reduced (0.5f), 1.0f);

        auto cap = dial.reduced ((float) d * 0.30f);
        juce::ColourGradient capGrad (juce::Colour (0xff1f1f1f), cap.getCentreX(), cap.getY(),
                                      juce::Colour (0xff060606), cap.getCentreX(), cap.getBottom(), false);
        g.setGradientFill (capGrad);
        g.fillEllipse (cap);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawEllipse (cap.reduced (0.5f), 1.0f);

        // --- indicator --------------------------------------------------------
        const float angle = kStartAngle + norm * (kEndAngle - kStartAngle);
        juce::Path pointer;
        const float pw = juce::jmax (2.0f, (float) d * 0.028f);
        pointer.addRoundedRectangle (-pw * 0.5f, -radius + (float) d * 0.055f, pw, (float) d * 0.24f, pw * 0.5f);
        pointer.applyTransform (juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));
        g.setColour (col::ink);
        g.fillPath (pointer);

        // --- label and readout --------------------------------------------------
        auto below = area.withTop (juce::roundToInt (dial.getBottom()) + 2);
        const float labelH = knobSize == Small ? 9.0f : 10.0f;
        auto labelArea = below.removeFromTop (knobSize == Large ? 15 : 12);

        Fonts::drawLabel (g, labelText, labelArea, labelH,
                          lit ? col::accent : col::ink,
                          knobSize == Small ? 0.10f : 0.12f, juce::Justification::centred, true);

        g.setFont (Fonts::mono (knobSize == Large ? 15.0f : 11.0f, knobSize == Large));
        g.setColour (lit ? col::accent : col::inkMuted);
        g.drawFittedText (getTextFromValue (getValue()), below, juce::Justification::centredTop, 1);

        if (hasKeyboardFocus (false))
        {
            g.setColour (col::accent);
            g.drawRoundedRectangle (dial.expanded (3.0f), (float) d, 2.0f);
        }
    }
}
