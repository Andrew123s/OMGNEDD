#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "OmgButton.h"

namespace omg::ui
{
    /** A row or grid of push buttons driving one choice parameter.

        Exactly one segment is lit at a time, and the lit segment is the
        parameter's current index, so host automation and the UI cannot drift
        apart. Used for the engine selector, the distortion algorithms, the
        saturation models, the compressor characters and the advanced panel's
        oversampling, slope and modulation-shape choices.
    */
    class OmgSelector : public juce::Component
    {
    public:
        OmgSelector (juce::AudioProcessorValueTreeState& state, juce::StringRef parameterID,
                     int columnsToUse = 0, bool tallButtons = false);
        ~OmgSelector() override;

        void resized() override;
        int  getPreferredHeight() const;

    private:
        void refresh();
        void setIndex (int index);

        juce::AudioProcessorValueTreeState& apvts;
        juce::String paramID;
        juce::RangedAudioParameter* parameter { nullptr };
        juce::OwnedArray<OmgButton> buttons;
        std::unique_ptr<juce::ParameterAttachment> attachment;
        int columns { 0 };
        bool tall { false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmgSelector)
    };
}

namespace omg::ui
{
    /** A compact choice control for dense rows: prints the current option and
        opens a menu to change it. Same parameter contract as OmgSelector.
    */
    class OmgChoiceButton : public juce::Component
    {
    public:
        OmgChoiceButton (juce::AudioProcessorValueTreeState& state, juce::StringRef parameterID,
                         juce::String captionText = {});
        ~OmgChoiceButton() override;

        void resized() override;
        void paint (juce::Graphics&) override;

    private:
        void refresh();

        juce::AudioProcessorValueTreeState& apvts;
        juce::String paramID, caption;
        juce::StringArray choices;
        juce::RangedAudioParameter* parameter { nullptr };
        std::unique_ptr<juce::ParameterAttachment> attachment;
        std::unique_ptr<OmgButton> button;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmgChoiceButton)
    };
}
