#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "OmgButton.h"
#include "../Presets/PresetManager.h"

namespace omg::ui
{
    /** The recessed preset LCD with previous, next and browse. */
    class PresetBar : public juce::Component
    {
    public:
        explicit PresetBar (omg::PresetManager& manager) : presets (manager)
        {
            auto make = [this] (const juce::String& glyph, std::function<void()> action, const juce::String& tip)
            {
                auto* b = new OmgButton (glyph, std::move (action));
                b->setShowLed (false);
                b->setCompact (true);
                b->setTooltip (tip);
                steppers.add (b);
                addAndMakeVisible (b);
                return b;
            };

            make ("<",  [this] { presets.loadPrevious(); refresh(); }, "PREVIOUS PRESET\nSteps back through the bank.");
            make (">",  [this] { presets.loadNext();     refresh(); }, "NEXT PRESET\nSteps forward through the bank.");
            make ("...", [this] { if (onBrowse != nullptr) onBrowse(); }, "PRESET BROWSER\nOpens the full bank with categories, search and favourites.");

            presets.onChanged = [this] { refresh(); };
        }

        ~PresetBar() override { presets.onChanged = nullptr; }

        std::function<void()> onBrowse;

        void refresh() { repaint(); }

        void paint (juce::Graphics& g) override
        {
            auto area = getLocalBounds();
            auto caption = area.removeFromLeft (52);
            Fonts::drawLabel (g, "PRESET", caption, 10.0f, col::inkMuted, 0.16f, juce::Justification::centredLeft, true);

            auto well = area.withTrimmedRight (3 * 24 + 2 * 4 + metric::space2).toFloat();
            Chassis::drawRecess (g, well, (float) metric::radiusMd);

            Fonts::drawLabel (g, presets.getCurrentName(), well.toNearestInt().reduced (metric::space3, 0),
                              13.0f, col::accent, 0.08f, juce::Justification::centredLeft, false);
        }

        void resized() override
        {
            auto area = getLocalBounds().withTrimmedLeft (52);
            auto buttons = area.removeFromRight (3 * 24 + 2 * 4);

            for (auto* b : steppers)
            {
                b->setBounds (buttons.removeFromLeft (24).reduced (0, 4));
                buttons.removeFromLeft (4);
            }
        }

    private:
        omg::PresetManager& presets;
        juce::OwnedArray<OmgButton> steppers;
    };
}
