#include "TopBar.h"
#include "../PluginProcessor.h"

namespace omg::ui
{
    TopBar::TopBar (OmgnedProcessor& processorToUse) : processor (processorToUse)
    {
        addAndMakeVisible (wordmark);

        presetBar = std::make_unique<PresetBar> (processor.presetManager);
        presetBar->onBrowse = [this] { if (onBrowsePresets != nullptr) onBrowsePresets(); };
        presetBar->setComponentID ("presetBar");
        addAndMakeVisible (*presetBar);

        auto add = [this] (const juce::String& name, std::function<void()> action, const juce::String& tip, bool led)
        {
            auto* b = commandButtons.add (new OmgButton (name, std::move (action)));
            b->setShowLed (led);
            b->setCompact (true);
            b->setTooltip (tip);
            addAndMakeVisible (b);
            return b;
        };

        slotA = add ("A", [this] { processor.stateManager.setSlot (0); }, "SLOT A\nHolds one complete plugin state. Compare against B.", false);
        slotB = add ("B", [this] { processor.stateManager.setSlot (1); }, "SLOT B\nHolds the other complete plugin state.", false);
        add ("A>B", [this] { processor.stateManager.copyAcross (0, 1); }, "COPY A TO B\nOverwrites slot B with slot A.", false);
        add ("B>A", [this] { processor.stateManager.copyAcross (1, 0); }, "COPY B TO A\nOverwrites slot A with slot B.", false);

        undoButton = add ("UNDO", [this] { processor.stateManager.undo(); }, "UNDO\nSteps back through the edits made in this session.", false);
        redoButton = add ("REDO", [this] { processor.stateManager.redo(); }, "REDO\nSteps forward again.", false);

        randomButton = add ("RANDOM", [this] { processor.randomizer.randomise(); },
                            "RANDOM\nDraws one coherent patch. The character, the dynamics, the movement "
                            "and the blend are drawn against each other rather than independently, so the "
                            "result is something you could have dialled yourself.",
                            false);
        randomButton->setComponentID ("random");

        lockButton = add ("LOCK", [this] { showRandomMenu(); },
                          "LOCKS\nChooses which modules RANDOM is allowed to touch. A locked module is "
                          "left exactly as it is, so an EQ you like survives the next draw.",
                          true);
        lockButton->setComponentID ("lock");

        advButton = add ("ADV", [this] { if (onToggleAdvanced != nullptr) onToggleAdvanced(); },
                         "ADVANCED\nOpens the engineering panel: DSP, routing, oversampling, modulation, stereo and safety.", true);
        advButton->setComponentID ("adv");
        add ("SETTINGS", [this] { if (onSettings != nullptr) onSettings(); }, "SETTINGS\nInterface scale and preset folder.", false);
        add ("?", [this] { if (onHelp != nullptr) onHelp(); }, "HELP\nWhat each section does, and the gestures every control shares.", false);

        powerButton = std::make_unique<OmgButton> (processor.apvts, pid::power);
        powerButton->setLegend ("POWER");
        powerButton->setCompact (true);
        addAndMakeVisible (*powerButton);

        startTimerHz (10);
    }

    TopBar::~TopBar() { stopTimer(); }

    void TopBar::showRandomMenu()
    {
        juce::PopupMenu menu;
        menu.addSectionHeader ("LOCKED MODULES");
        menu.addSectionHeader ("RANDOM LEAVES A TICKED MODULE ALONE");

        for (int m = 0; m < Randomizer::NumModules; ++m)
            menu.addItem (100 + m, Randomizer::moduleName (m), true, processor.randomizer.isLocked (m));

        menu.addSeparator();
        menu.addItem (1, "Unlock everything");
        menu.addItem (2, "Draw a new patch now");

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (lockButton),
                            [this] (int result)
        {
            if (result == 1)
            {
                for (int m = 0; m < Randomizer::NumModules; ++m)
                    processor.randomizer.setLocked (m, false);
            }
            else if (result == 2)
            {
                processor.randomizer.randomise();
            }
            else if (result >= 100 && result < 100 + Randomizer::NumModules)
            {
                processor.randomizer.toggleLock (result - 100);
            }
        });
    }

    void TopBar::setAdvancedActive (bool active)
    {
        if (advButton != nullptr)
            advButton->setManualOn (active);
    }

    void TopBar::timerCallback()
    {
        if (lockButton != nullptr)
        {
            bool anyLocked = false;
            for (int m = 0; m < Randomizer::NumModules; ++m)
                anyLocked = anyLocked || processor.randomizer.isLocked (m);

            lockButton->setManualOn (anyLocked);
        }

        const int slot = processor.stateManager.getSlot();
        if (slotA != nullptr) slotA->setManualOn (slot == 0);
        if (slotB != nullptr) slotB->setManualOn (slot == 1);
        if (undoButton != nullptr) undoButton->setEnabled (processor.stateManager.canUndo());
        if (redoButton != nullptr) redoButton->setEnabled (processor.stateManager.canRedo());
    }

    void TopBar::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat();
        Chassis::drawMetal (g, r.withTrimmedBottom (-4.0f), (float) metric::radiusLg);

        g.setColour (col::hairline);
        g.fillRect (0, getHeight() - 1, getWidth(), 1);
    }

    void TopBar::resized()
    {
        auto area = getLocalBounds().reduced (metric::space4, metric::space3);

        wordmark.setBounds (area.removeFromLeft (190));
        wordmark.setShowProductLine (getWidth() >= 1000);

        area.removeFromLeft (metric::space6);

        // right-hand cluster first so the preset display takes what is left
        auto right = area.removeFromRight (432);

        powerButton->setBounds (right.removeFromRight (78).reduced (0, 6));
        right.removeFromRight (metric::space2);

        // the right-hand cluster, laid out from the right: ? SETTINGS ADV RANDOM
        static const int rightWidths[] = { 24, 68, 48, 50, 64 };

        for (int i = commandButtons.size() - 1, k = 0; i >= 6; --i, ++k)
        {
            auto* b = commandButtons[i];
            const int w = k < 4 ? rightWidths[k] : 52;
            b->setBounds (right.removeFromRight (w).reduced (0, 6));
            right.removeFromRight (metric::space1);
        }

        auto middle = area;
        auto abRow = middle.removeFromRight (256);

        for (int i = 0; i < 6 && i < commandButtons.size(); ++i)
        {
            auto* b = commandButtons[i];
            const int w = i < 2 ? 32 : i < 4 ? 40 : 46;
            b->setBounds (abRow.removeFromLeft (w).reduced (0, 6));
            abRow.removeFromLeft (metric::space1);
        }

        presetBar->setBounds (middle.withTrimmedRight (metric::space4));
    }
}
