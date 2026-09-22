#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "OmgButton.h"
#include "../Presets/PresetManager.h"

namespace omg::ui
{
    /** The modal preset overlay: search, categories, favourites, user presets. */
    class PresetBrowserComponent : public juce::Component,
                                   private juce::ListBoxModel,
                                   private juce::TextEditor::Listener
    {
    public:
        explicit PresetBrowserComponent (omg::PresetManager& manager);
        ~PresetBrowserComponent() override;

        void paint (juce::Graphics&) override;
        void resized() override;
        bool keyPressed (const juce::KeyPress&) override;

        void refresh();
        std::function<void()> onClose;

    private:
        // ListBoxModel
        int  getNumRows() override;
        void paintListBoxItem (int row, juce::Graphics&, int width, int height, bool selected) override;
        void listBoxItemClicked (int row, const juce::MouseEvent&) override;
        void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override;

        void textEditorTextChanged (juce::TextEditor&) override;
        void rebuildFilter();
        void saveAs();

        omg::PresetManager& presets;
        juce::TextEditor search;
        juce::ListBox list { "presets", this };
        juce::OwnedArray<OmgButton> tabs, actions;
        juce::Array<int> filtered;
        juce::String category { "ALL" };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetBrowserComponent)
    };
}
