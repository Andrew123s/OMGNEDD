#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "../Parameters.h"

namespace omg::ui
{
    /** The OMGNEDD rotary control.

        One component per parameter: metal body, white indicator, accent arc,
        engraved label and a live numeric readout. It is always attached to a
        real APVTS parameter; there is no way to build a decorative one.
    */
    class OmgKnob : public juce::Slider
    {
    public:
        enum Size { Large, Medium, Small };

        OmgKnob (juce::AudioProcessorValueTreeState& state, juce::StringRef parameterID,
                 Size size = Medium, bool bipolar = false);
        ~OmgKnob() override;

        /** Total footprint including the label and readout. */
        static juce::Rectangle<int> preferredBounds (Size size);

        /** The parameter this knob drives. */
        const juce::String& getParameterID() const noexcept { return paramID; }
        static int nominalDiameter (Size size);
        static int labelBlockFor (Size size);
        int getDialDiameter() const;

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseDoubleClick (const juce::MouseEvent&) override;
        void mouseEnter (const juce::MouseEvent&) override;
        void mouseExit (const juce::MouseEvent&) override;

        juce::String getTextFromValue (double value) override;
        juce::String getTooltip() override;

        void setLabelText (const juce::String& text) { labelText = text; repaint(); }

    private:
        void showParameterMenu();
        void refreshTooltip();

        juce::AudioProcessorValueTreeState& apvts;
        juce::String paramID, labelText;
        const ParamDesc* desc { nullptr };
        Size knobSize;
        bool isBipolar { false }, hovering { false }, dragging { false };

        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

        static float clipboardValue;
        static bool  clipboardValid;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmgKnob)
    };
}
