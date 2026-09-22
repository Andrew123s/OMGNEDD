#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "Wordmark.h"
#include "OmgButton.h"
#include "PresetBar.h"

class OmgnedProcessor;

namespace omg::ui
{
    /** Logo, preset display, A/B, undo/redo, and the right-hand cluster. */
    class TopBar : public juce::Component,
                   private juce::Timer
    {
    public:
        explicit TopBar (OmgnedProcessor& processorToUse);
        ~TopBar() override;

        void paint (juce::Graphics&) override;
        void resized() override;

        std::function<void()> onBrowsePresets, onToggleAdvanced, onSettings, onHelp;
        void setAdvancedActive (bool active);

    private:
        void timerCallback() override;

        OmgnedProcessor& processor;
        Wordmark wordmark;
        std::unique_ptr<PresetBar> presetBar;
        juce::OwnedArray<OmgButton> commandButtons;
        OmgButton* slotA { nullptr };
        OmgButton* slotB { nullptr };
        OmgButton* undoButton { nullptr };
        OmgButton* redoButton { nullptr };
        OmgButton* advButton { nullptr };
        std::unique_ptr<OmgButton> powerButton;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TopBar)
    };
}
