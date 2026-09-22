#include "PresetBrowserComponent.h"

namespace omg::ui
{
    static const juce::StringArray kCategories { "ALL", "FAVORITES", "UNDERWATER", "DISTORTION",
                                                 "SATURATION", "VOCAL", "EXTREME", "HYBRID", "USER" };

    PresetBrowserComponent::PresetBrowserComponent (omg::PresetManager& manager) : presets (manager)
    {
        search.setTextToShowWhenEmpty ("SEARCH PRESETS", col::inkMuted);
        search.setFont (Fonts::mono (11.0f));
        search.setColour (juce::TextEditor::backgroundColourId, col::recess);
        search.setColour (juce::TextEditor::outlineColourId, col::hairline);
        search.setColour (juce::TextEditor::focusedOutlineColourId, col::accentDim);
        search.setColour (juce::TextEditor::textColourId, col::ink);
        search.setColour (juce::CaretComponent::caretColourId, col::accent);
        search.addListener (this);
        addAndMakeVisible (search);

        for (const auto& c : kCategories)
        {
            auto* b = new OmgButton (c, [this, c] { category = c; rebuildFilter(); });
            b->setShowLed (false);
            b->setCompact (true);
            tabs.add (b);
            addAndMakeVisible (b);
        }

        auto addAction = [this] (const juce::String& name, std::function<void()> fn, const juce::String& tip)
        {
            auto* b = new OmgButton (name, std::move (fn));
            b->setShowLed (false);
            b->setCompact (true);
            b->setTooltip (tip);
            actions.add (b);
            addAndMakeVisible (b);
        };

        addAction ("SAVE",   [this] { saveAs(); }, "SAVE\nWrites the current state as a user preset.");
        addAction ("DELETE", [this]
        {
            const int row = list.getSelectedRow();
            if (juce::isPositiveAndBelow (row, filtered.size()))
                presets.deleteUser (filtered[row]);
            refresh();
        }, "DELETE\nRemoves the selected user preset. The factory bank cannot be deleted.");

        addAction ("CLOSE", [this] { if (onClose != nullptr) onClose(); }, "CLOSE\nLeaves the browser without changing the preset.");

        list.setRowHeight (26);
        list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
        list.setColour (juce::ListBox::outlineColourId, juce::Colours::transparentBlack);
        addAndMakeVisible (list);

        setWantsKeyboardFocus (true);
        rebuildFilter();
    }

    PresetBrowserComponent::~PresetBrowserComponent() = default;

    void PresetBrowserComponent::refresh()
    {
        presets.rescan();
        rebuildFilter();
    }

    void PresetBrowserComponent::rebuildFilter()
    {
        filtered.clearQuick();
        const auto query = search.getText().toUpperCase();

        for (int i = 0; i < presets.getNumPresets(); ++i)
        {
            const auto& e = presets.getPreset (i);

            if (category == "FAVORITES" && ! presets.isFavourite (i)) continue;
            if (category != "ALL" && category != "FAVORITES" && e.category != category) continue;
            if (query.isNotEmpty() && ! e.name.toUpperCase().contains (query)) continue;

            filtered.add (i);
        }

        for (auto* t : tabs)
            t->setManualOn (t->getButtonText() == category);

        list.updateContent();
        list.repaint();
        repaint();
    }

    void PresetBrowserComponent::textEditorTextChanged (juce::TextEditor&) { rebuildFilter(); }

    void PresetBrowserComponent::saveAs()
    {
        auto* window = new juce::AlertWindow ("SAVE PRESET", "Name this preset", juce::MessageBoxIconType::NoIcon);
        window->addTextEditor ("name", presets.getCurrentName().removeCharacters ("*"));
        window->addButton ("Save", 1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        window->enterModalState (true, juce::ModalCallbackFunction::create ([this, window] (int r)
        {
            if (r == 1)
                presets.saveUser (window->getTextEditorContents ("name"));
            delete window;
            refresh();
        }), false);
    }

    int PresetBrowserComponent::getNumRows() { return filtered.size(); }

    void PresetBrowserComponent::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
    {
        if (! juce::isPositiveAndBelow (row, filtered.size())) return;

        const int index = filtered[row];
        const auto& e = presets.getPreset (index);
        const bool fav = presets.isFavourite (index);

        if (selected)
            g.fillAll (col::accent.withAlpha (0.12f));

        g.setColour (col::hairline.withAlpha (0.5f));
        g.drawHorizontalLine (height - 1, 0.0f, (float) width);

        g.setFont (Fonts::panel (13.0f));
        g.setColour (fav ? col::accent : col::inkDim);
        g.drawText (fav ? juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x85"))
                        : juce::String (juce::CharPointer_UTF8 ("\xe2\x98\x86")),
                    metric::space3, 0, 18, height, juce::Justification::centredLeft, false);

        Fonts::drawLabel (g, e.name, { metric::space3 + 24, 0, width - 140, height }, 10.0f,
                          col::ink, 0.12f, juce::Justification::centredLeft, true);

        g.setFont (Fonts::mono (9.0f));
        g.setColour (col::inkMuted);
        g.drawText (e.category, width - 120, 0, 108, height, juce::Justification::centredRight, false);
    }

    void PresetBrowserComponent::listBoxItemClicked (int row, const juce::MouseEvent& e)
    {
        if (! juce::isPositiveAndBelow (row, filtered.size())) return;

        if (e.x < metric::space3 + 20)
        {
            presets.toggleFavourite (filtered[row]);
            rebuildFilter();
            return;
        }

        presets.load (filtered[row]);
    }

    void PresetBrowserComponent::listBoxItemDoubleClicked (int row, const juce::MouseEvent&)
    {
        if (! juce::isPositiveAndBelow (row, filtered.size())) return;
        presets.load (filtered[row]);
        if (onClose != nullptr) onClose();
    }

    bool PresetBrowserComponent::keyPressed (const juce::KeyPress& key)
    {
        if (key == juce::KeyPress::escapeKey)
        {
            if (onClose != nullptr) onClose();
            return true;
        }
        return false;
    }

    void PresetBrowserComponent::paint (juce::Graphics& g)
    {
        auto r = getLocalBounds().toFloat().reduced (0.5f);

        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r.translated (0.0f, 6.0f), (float) metric::radiusLg);

        g.setColour (col::chassis100);
        g.fillRoundedRectangle (r, (float) metric::radiusLg);
        g.setColour (col::lineControl);
        g.drawRoundedRectangle (r, (float) metric::radiusLg, 1.0f);

        auto listArea = getLocalBounds().reduced (metric::space3).withTrimmedTop (72);
        Chassis::drawRecess (g, listArea.toFloat(), (float) metric::radiusMd);
    }

    void PresetBrowserComponent::resized()
    {
        auto area = getLocalBounds().reduced (metric::space3);

        auto top = area.removeFromTop (26);
        for (auto* a : actions)
        {
            a->setBounds (top.removeFromRight (62));
            top.removeFromRight (metric::space2);
        }
        search.setBounds (top);

        area.removeFromTop (metric::space2);

        auto tabRow = area.removeFromTop (24);
        const int tabW = tabRow.getWidth() / juce::jmax (1, tabs.size());
        for (auto* t : tabs)
            t->setBounds (tabRow.removeFromLeft (tabW).reduced (1, 0));

        area.removeFromTop (metric::space2);
        list.setBounds (area.reduced (2));
    }
}
