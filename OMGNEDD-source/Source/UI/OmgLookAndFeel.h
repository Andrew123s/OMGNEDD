#pragma once
#include "Theme.h"

namespace omg::ui
{
    /** Menus, alerts, scrollbars and the three-line parameter tooltip, in the
        same materials as the panel.
    */
    class OmgLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        OmgLookAndFeel();

        juce::Rectangle<int> getTooltipBounds (const juce::String& tip, juce::Point<int> screenPos,
                                               juce::Rectangle<int> parentArea) override;
        void drawTooltip (juce::Graphics&, const juce::String& text, int width, int height) override;

        void drawPopupMenuBackground (juce::Graphics&, int width, int height) override;
        void drawPopupMenuItem (juce::Graphics&, const juce::Rectangle<int>& area,
                                bool isSeparator, bool isActive, bool isHighlighted,
                                bool isTicked, bool hasSubMenu, const juce::String& text,
                                const juce::String& shortcutKeyText, const juce::Drawable* icon,
                                const juce::Colour* textColour) override;
        juce::Font getPopupMenuFont() override;

        void drawAlertBox (juce::Graphics&, juce::AlertWindow&, const juce::Rectangle<int>& textArea,
                           juce::TextLayout&) override;

        void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int width, int height,
                            bool isVertical, int thumbStart, int thumbSize, bool isMouseOver,
                            bool isMouseDown) override;

    private:
        static juce::StringArray splitTooltip (const juce::String& text);
    };
}
