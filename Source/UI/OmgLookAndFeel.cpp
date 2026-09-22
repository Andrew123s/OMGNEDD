#include "OmgLookAndFeel.h"

namespace omg::ui
{
    OmgLookAndFeel::OmgLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, col::chassis000);
        setColour (juce::PopupMenu::backgroundColourId, col::chassis100);
        setColour (juce::PopupMenu::textColourId, col::ink);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, col::accent.withAlpha (0.16f));
        setColour (juce::PopupMenu::highlightedTextColourId, col::accent);
        setColour (juce::AlertWindow::backgroundColourId, col::chassis100);
        setColour (juce::AlertWindow::textColourId, col::ink);
        setColour (juce::AlertWindow::outlineColourId, col::lineControl);
        setColour (juce::TextButton::buttonColourId, col::chassis300);
        setColour (juce::TextButton::textColourOffId, col::ink);
        setColour (juce::TextEditor::backgroundColourId, col::recess);
        setColour (juce::TextEditor::textColourId, col::ink);
        setColour (juce::TextEditor::highlightColourId, col::accent.withAlpha (0.25f));
        setColour (juce::Label::textColourId, col::ink);
    }

    juce::StringArray OmgLookAndFeel::splitTooltip (const juce::String& text)
    {
        juce::StringArray lines;
        lines.addLines (text);
        lines.removeEmptyStrings();
        return lines;
    }

    juce::Rectangle<int> OmgLookAndFeel::getTooltipBounds (const juce::String& tip, juce::Point<int> screenPos,
                                                           juce::Rectangle<int> parentArea)
    {
        const auto lines = splitTooltip (tip);
        auto bodyFont = Fonts::panel (11.0f);

        int width = 160;
        for (const auto& l : lines)
            width = juce::jmax (width, juce::jmin (250, (int) Fonts::widthOf (bodyFont, l) + 24));

        int height = 16;
        for (int i = 0; i < lines.size(); ++i)
        {
            const int usable = width - 24;
            const int wrapped = juce::jmax (1, (int) std::ceil (Fonts::widthOf (bodyFont, lines[i]) / juce::jmax (1.0f, (float) usable)));
            height += (i == 0 ? 18 : 16 * wrapped) + 4;
        }

        return juce::Rectangle<int> (screenPos.x > parentArea.getCentreX() ? screenPos.x - width - 12 : screenPos.x + 14,
                                     screenPos.y > parentArea.getCentreY() ? screenPos.y - height - 6 : screenPos.y + 18,
                                     width, height)
                   .constrainedWithin (parentArea);
    }

    void OmgLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int width, int height)
    {
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);

        g.setColour (col::recess);
        g.fillRoundedRectangle (r, (float) metric::radiusMd);
        g.setColour (col::lineControl);
        g.drawRoundedRectangle (r, (float) metric::radiusMd, 1.0f);

        auto area = juce::Rectangle<int> (width, height).reduced (metric::space3, metric::space2);
        const auto lines = splitTooltip (text);

        for (int i = 0; i < lines.size(); ++i)
        {
            if (i == 0)
            {
                Fonts::drawLabel (g, lines[i], area.removeFromTop (16), 11.0f, col::accent, 0.10f,
                                  juce::Justification::centredLeft, true);
                area.removeFromTop (2);
            }
            else if (lines[i].startsWith ("Current:"))
            {
                g.setFont (Fonts::mono (11.0f));
                g.setColour (col::ink);
                g.drawFittedText (lines[i], area.removeFromTop (16), juce::Justification::centredLeft, 1);
            }
            else
            {
                g.setFont (Fonts::panel (11.0f));
                g.setColour (col::inkMuted);
                const int lineHeight = 16 * juce::jmax (1, (int) std::ceil (
                    Fonts::widthOf (Fonts::panel (11.0f), lines[i]) / juce::jmax (1.0f, (float) area.getWidth())));
                g.drawFittedText (lines[i], area.removeFromTop (lineHeight), juce::Justification::topLeft, 3);
                area.removeFromTop (2);
            }
        }
    }

    void OmgLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int width, int height)
    {
        auto r = juce::Rectangle<float> (0.0f, 0.0f, (float) width, (float) height).reduced (0.5f);
        g.setColour (col::chassis100);
        g.fillRoundedRectangle (r, (float) metric::radiusMd);
        g.setColour (col::lineControl);
        g.drawRoundedRectangle (r, (float) metric::radiusMd, 1.0f);
    }

    juce::Font OmgLookAndFeel::getPopupMenuFont() { return Fonts::panel (13.0f); }

    void OmgLookAndFeel::drawPopupMenuItem (juce::Graphics& g, const juce::Rectangle<int>& area,
                                            bool isSeparator, bool isActive, bool isHighlighted,
                                            bool isTicked, bool hasSubMenu, const juce::String& text,
                                            const juce::String& shortcutKeyText, const juce::Drawable* icon,
                                            const juce::Colour* textColour)
    {
        juce::ignoreUnused (hasSubMenu, shortcutKeyText, icon, textColour);

        if (isSeparator)
        {
            g.setColour (col::hairline);
            g.fillRect (area.reduced (8, 0).withHeight (1).withY (area.getCentreY()));
            return;
        }

        if (isHighlighted && isActive)
            g.fillAll (col::accent.withAlpha (0.14f));

        g.setFont (Fonts::panel (13.0f));
        g.setColour (! isActive ? col::inkDim : isHighlighted ? col::accent : col::ink);
        g.drawFittedText (text, area.reduced (metric::space3, 0), juce::Justification::centredLeft, 1);

        if (isTicked)
        {
            g.setColour (col::accent);
            g.fillEllipse (juce::Rectangle<float> (6.0f, 6.0f).withCentre ({ (float) area.getRight() - 12.0f, (float) area.getCentreY() }));
        }
    }

    void OmgLookAndFeel::drawAlertBox (juce::Graphics& g, juce::AlertWindow& window,
                                       const juce::Rectangle<int>& textArea, juce::TextLayout& layout)
    {
        auto r = window.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (col::chassis100);
        g.fillRoundedRectangle (r, (float) metric::radiusLg);
        g.setColour (col::lineControl);
        g.drawRoundedRectangle (r, (float) metric::radiusLg, 1.0f);

        layout.draw (g, textArea.toFloat());
    }

    void OmgLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int width, int height,
                                        bool isVertical, int thumbStart, int thumbSize, bool isMouseOver, bool)
    {
        juce::Rectangle<int> thumb = isVertical ? juce::Rectangle<int> (x + width / 3, thumbStart, width / 3, thumbSize)
                                                : juce::Rectangle<int> (thumbStart, y + height / 3, thumbSize, height / 3);
        g.setColour (isMouseOver ? col::accentDim : col::hairline);
        g.fillRoundedRectangle (thumb.toFloat(), 2.0f);
    }
}
