#include "Theme.h"
#include "BinaryData.h"

namespace omg::ui
{
    static juce::Typeface::Ptr loadTypeface (const char* data, int size)
    {
        return juce::Typeface::createSystemTypefaceFor (data, (size_t) size);
    }

    float Fonts::widthOf (const juce::Font& font, const juce::String& text)
    {
        return juce::GlyphArrangement::getStringWidth (font, text);
    }

    juce::Font Fonts::panel (float height, bool heavy)
    {
        static juce::Typeface::Ptr semi  = loadTypeface (BinaryData::ArchivoSemiBold_ttf,  BinaryData::ArchivoSemiBold_ttfSize);
        static juce::Typeface::Ptr extra = loadTypeface (BinaryData::ArchivoExtraBold_ttf, BinaryData::ArchivoExtraBold_ttfSize);

        auto tf = heavy ? extra : semi;
        if (tf == nullptr)
            return juce::Font (juce::FontOptions (height).withStyle (heavy ? "Bold" : "Regular"));

        return juce::Font (juce::FontOptions (tf).withHeight (height));
    }

    juce::Font Fonts::mono (float height, bool semi)
    {
        static juce::Typeface::Ptr regular = loadTypeface (BinaryData::IBMPlexMonoRegular_ttf,  BinaryData::IBMPlexMonoRegular_ttfSize);
        static juce::Typeface::Ptr bold    = loadTypeface (BinaryData::IBMPlexMonoSemiBold_ttf, BinaryData::IBMPlexMonoSemiBold_ttfSize);

        auto tf = semi ? bold : regular;
        if (tf == nullptr)
            return juce::Font (juce::FontOptions (height).withStyle (semi ? "Bold" : "Regular"));

        return juce::Font (juce::FontOptions (tf).withHeight (height));
    }

    void Fonts::drawLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                           float height, juce::Colour colour, float tracking,
                           juce::Justification just, bool heavy)
    {
        const auto upper = text.toUpperCase();
        if (upper.isEmpty() || area.getWidth() <= 0)
            return;

        // JUCE has no tracking on a Font, so the glyphs are placed by hand, and
        // the size is stepped down until the tracked line fits the space it was
        // given. A panel legend is never clipped and never wraps.
        float h = height, extra = 0.0f, total = 0.0f;
        juce::Font font = panel (h, heavy);

        for (int attempt = 0; attempt < 12; ++attempt)
        {
            font = panel (h, heavy);
            extra = h * tracking;
            total = 0.0f;
            for (int i = 0; i < upper.length(); ++i)
                total += widthOf (font, upper.substring (i, i + 1)) + extra;
            total -= extra;

            if (total <= (float) area.getWidth() || h <= 5.5f)
                break;

            h *= juce::jmax (0.80f, (float) area.getWidth() / total);
        }

        float x = (float) area.getX();
        if (just.testFlags (juce::Justification::horizontallyCentred))
            x += ((float) area.getWidth() - total) * 0.5f;
        else if (just.testFlags (juce::Justification::right))
            x += (float) area.getWidth() - total;

        const float baseline = (float) area.getCentreY() + h * 0.35f;

        g.setFont (font);
        g.setColour (colour);

        for (int i = 0; i < upper.length(); ++i)
        {
            const auto ch = upper.substring (i, i + 1);
            g.drawSingleLineText (ch, juce::roundToInt (x), juce::roundToInt (baseline));
            x += widthOf (font, ch) + extra;
        }
    }

    void Chassis::drawMetal (juce::Graphics& g, juce::Rectangle<float> r, float corner)
    {
        juce::ColourGradient grad (col::chassis200.brighter (0.16f), r.getX(), r.getY(),
                                   col::chassis200.darker (0.32f),  r.getX(), r.getBottom(), false);
        grad.addColour (0.22, col::chassis200);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r, corner);

        // vertical brushed grain, kept at the texture opacity the system allows
        g.saveState();
        g.reduceClipRegion (juce::Path{ [&] { juce::Path p; p.addRoundedRectangle (r, corner); return p; }() }, {});
        g.setColour (juce::Colours::white.withAlpha (0.045f));
        for (float x = r.getX(); x < r.getRight(); x += 3.0f)
            g.fillRect (x, r.getY(), 1.0f, r.getHeight());
        g.restoreState();

        g.setColour (col::edge.withAlpha (0.55f));
        g.drawLine (r.getX() + corner, r.getY() + 0.5f, r.getRight() - corner, r.getY() + 0.5f, 1.0f);

        g.setColour (col::hairline);
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);
    }

    void Chassis::drawRecess (juce::Graphics& g, juce::Rectangle<float> r, float corner)
    {
        g.setColour (col::recess);
        g.fillRoundedRectangle (r, corner);

        juce::ColourGradient inner (juce::Colours::black.withAlpha (0.85f), r.getX(), r.getY(),
                                    juce::Colours::transparentBlack, r.getX(), r.getY() + 6.0f, false);
        g.setGradientFill (inner);
        g.fillRoundedRectangle (r, corner);

        g.setColour (col::hairline);
        g.drawRoundedRectangle (r.reduced (0.5f), corner, 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawLine (r.getX() + corner, r.getBottom() - 0.5f, r.getRight() - corner, r.getBottom() - 0.5f, 1.0f);
    }

    void Chassis::drawScrew (juce::Graphics& g, juce::Point<float> centre, float diameter)
    {
        const auto r = juce::Rectangle<float> (diameter, diameter).withCentre (centre);

        juce::ColourGradient grad (col::screw.brighter (0.25f), r.getX() + diameter * 0.3f, r.getY() + diameter * 0.25f,
                                   juce::Colour (0xff141414), r.getRight(), r.getBottom(), true);
        g.setGradientFill (grad);
        g.fillEllipse (r);

        g.setColour (juce::Colours::black.withAlpha (0.7f));
        g.drawLine (r.getX() + 1.5f, r.getCentreY(), r.getRight() - 1.5f, r.getCentreY(), 1.0f);

        g.setColour (juce::Colours::white.withAlpha (0.08f));
        g.drawEllipse (r.reduced (0.5f), 1.0f);
    }

    void Chassis::drawHairline (juce::Graphics& g, juce::Rectangle<int> r, bool horizontal)
    {
        g.setColour (col::hairline);
        if (horizontal)
        {
            g.fillRect (r.getX(), r.getCentreY(), r.getWidth(), 1);
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.fillRect (r.getX(), r.getCentreY() + 1, r.getWidth(), 1);
        }
        else
        {
            g.fillRect (r.getCentreX(), r.getY(), 1, r.getHeight());
            g.setColour (juce::Colours::white.withAlpha (0.04f));
            g.fillRect (r.getCentreX() + 1, r.getY(), 1, r.getHeight());
        }
    }
}
