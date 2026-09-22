#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

namespace omg
{
    /** A/B comparison and undo/redo over the whole plugin state.

        Snapshots are taken on a timer only when the state has actually changed,
        so a knob sweep collapses into one undo step rather than hundreds.
    */
    class StateManager : private juce::Timer
    {
    public:
        explicit StateManager (juce::AudioProcessorValueTreeState& state);
        ~StateManager() override;

        void setSlot (int slot);                 // 0 = A, 1 = B
        int  getSlot() const { return slot; }

        void copyAcross (int from, int to);

        void undo();
        void redo();
        bool canUndo() const { return undoStack.size() > 1; }
        bool canRedo() const { return ! redoStack.isEmpty(); }

        std::function<void()> onChanged;

    private:
        void timerCallback() override;
        void capture();
        void restore (const juce::ValueTree& tree);

        juce::AudioProcessorValueTreeState& apvts;
        juce::Array<juce::ValueTree> undoStack, redoStack;
        juce::ValueTree slots[2];
        juce::String lastHash;
        int slot { 0 };
        bool suspended { false };
    };
}
