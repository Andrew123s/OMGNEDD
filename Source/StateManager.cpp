#include "StateManager.h"

namespace omg
{
    StateManager::StateManager (juce::AudioProcessorValueTreeState& state) : apvts (state)
    {
        slots[0] = apvts.copyState();
        slots[1] = apvts.copyState();
        undoStack.add (apvts.copyState());
        lastHash = apvts.copyState().toXmlString();
        startTimer (400);
    }

    StateManager::~StateManager() { stopTimer(); }

    void StateManager::timerCallback()
    {
        if (suspended) { suspended = false; return; }
        capture();
    }

    void StateManager::capture()
    {
        auto current = apvts.copyState();
        const auto hash = current.toXmlString();

        if (hash == lastHash)
            return;

        lastHash = hash;
        undoStack.add (current);
        redoStack.clearQuick();

        while (undoStack.size() > 64)
            undoStack.remove (0);

        if (onChanged != nullptr)
            onChanged();
    }

    void StateManager::restore (const juce::ValueTree& tree)
    {
        if (! tree.isValid()) return;

        suspended = true;
        apvts.replaceState (tree.createCopy());
        lastHash = apvts.copyState().toXmlString();

        if (onChanged != nullptr)
            onChanged();
    }

    void StateManager::setSlot (int newSlot)
    {
        newSlot = juce::jlimit (0, 1, newSlot);
        if (newSlot == slot) return;

        slots[slot] = apvts.copyState();
        slot = newSlot;
        restore (slots[slot]);
    }

    void StateManager::copyAcross (int from, int to)
    {
        from = juce::jlimit (0, 1, from);
        to   = juce::jlimit (0, 1, to);
        if (from == to) return;

        slots[slot] = apvts.copyState();
        slots[to] = slots[from].createCopy();

        if (to == slot)
            restore (slots[to]);
        else if (onChanged != nullptr)
            onChanged();
    }

    void StateManager::undo()
    {
        if (! canUndo()) return;

        redoStack.add (undoStack.getLast());
        undoStack.removeLast();
        restore (undoStack.getLast());
    }

    void StateManager::redo()
    {
        if (! canRedo()) return;

        auto tree = redoStack.getLast();
        redoStack.removeLast();
        undoStack.add (tree);
        restore (tree);
    }
}
