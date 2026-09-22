#include "OmgMeter.h"

namespace omg::ui
{
    OmgMeter::OmgMeter (omg::dsp::MeterSource& sourceToUse) : source (sourceToUse)
    {
        setInterceptsMouseClicks (true, false);
        startTimerHz (40);
    }

    void OmgMeter::timerCallback()
    {
        for (int c = 0; c < 2; ++c)
        {
            const float db = source.getPeakDb (c);
            smoothed[c] = db > smoothed[c] ? db : smoothed[c] + (db - smoothed[c]) * 0.35f;
        }

        hold = source.getHoldDb();
        clipped = source.isClipped();
        repaint();
    }

    void OmgMeter::mouseDown (const juce::MouseEvent&)
    {
        source.clearClip();
        hold = -100.0f;
        repaint();
    }

    void OmgMeter::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds();

        auto foot = area.removeFromBottom (34);
        auto scaleArea = area.removeFromLeft (26);
        auto trough = area.reduced (0, 0);

        // ---- printed scale ------------------------------------------------
        static const int ticks[] = { 0, -3, -6, -12, -18, -24, -36 };
        g.setFont (Fonts::mono (9.0f));

        for (int t : ticks)
        {
            const float norm = juce::jlimit (0.0f, 1.0f, ((float) t - floorDb) / (0.0f - floorDb));
            const int y = juce::roundToInt ((float) trough.getBottom() - norm * (float) trough.getHeight());

            g.setColour (col::inkMuted);
            g.drawText (juce::String (t), scaleArea.getX(), y - 6, scaleArea.getWidth() - 4, 12,
                        juce::Justification::centredRight, false);

            g.setColour (col::hairline);
            g.fillRect (scaleArea.getRight() - 3, y, 3, 1);
        }

        // ---- trough -------------------------------------------------------
        Chassis::drawRecess (g, trough.toFloat(), (float) metric::radiusMd);

        auto inner = trough.reduced (3);
        const int gap = 1;
        const int colW = (inner.getWidth() - 3) / 2;
        const float segH = ((float) inner.getHeight() - (float) (numSegments - 1) * gap) / (float) numSegments;

        for (int c = 0; c < 2; ++c)
        {
            const int x = inner.getX() + c * (colW + 3);
            const float norm = juce::jlimit (0.0f, 1.0f, (smoothed[c] - floorDb) / (0.0f - floorDb));
            const int lit = juce::roundToInt (norm * numSegments);

            for (int s = 0; s < numSegments; ++s)
            {
                const float top = (float) inner.getBottom() - (float) (s + 1) * (segH + gap) + gap;
                const float segDb = floorDb + ((float) (s + 1) / numSegments) * (0.0f - floorDb);

                juce::Colour c2 = segDb > -1.0f ? col::clip : segDb > -6.0f ? col::warning : col::accent;
                g.setColour (s < lit ? c2 : col::accentDeep.withAlpha (0.25f));
                g.fillRect ((float) x, top, (float) colW, segH);
            }
        }

        if (hold > floorDb)
        {
            const float norm = juce::jlimit (0.0f, 1.0f, (hold - floorDb) / (0.0f - floorDb));
            const int y = juce::roundToInt ((float) inner.getBottom() - norm * (float) inner.getHeight());
            g.setColour (col::ink);
            g.fillRect (inner.getX(), y - 1, inner.getWidth(), 2);
        }

        // ---- clip lamp and peak figure -------------------------------------
        auto lampArea = foot.removeFromTop (14);
        auto lens = juce::Rectangle<float> (8.0f, 8.0f).withCentre ({ (float) lampArea.getCentreX() - 22.0f, (float) lampArea.getCentreY() });

        if (clipped)
        {
            g.setColour (col::clip.withAlpha (0.30f));
            g.fillEllipse (lens.expanded (4.0f));
        }
        g.setColour (clipped ? col::clip : col::ledOff);
        g.fillEllipse (lens);

        Fonts::drawLabel (g, "CLIP", lampArea.withTrimmedLeft (lampArea.getWidth() / 2 - 10), 9.0f,
                          clipped ? col::clip : col::inkMuted, 0.10f, juce::Justification::centredLeft, true);

        g.setFont (Fonts::mono (11.0f));
        g.setColour (col::inkMuted);
        g.drawText (hold <= floorDb ? "-inf" : juce::String (hold, 1) + " dB", foot,
                    juce::Justification::centredTop, false);
    }

    // -------------------------------------------------------------------------

    OmgGrMeter::OmgGrMeter (juce::String legend, float maxReductionDb, std::function<float()> reader)
        : label (std::move (legend)), maxDb (maxReductionDb), read (std::move (reader))
    {
        startTimerHz (40);
    }

    void OmgGrMeter::timerCallback()
    {
        const float db = read != nullptr ? read() : 0.0f;
        shown = db > shown ? db : shown + (db - shown) * 0.30f;
        repaint();
    }

    void OmgGrMeter::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds();
        auto head = area.removeFromTop (13);

        Fonts::drawLabel (g, label, head, 9.0f, col::ink, 0.10f, juce::Justification::centredLeft, true);

        g.setFont (Fonts::mono (11.0f));
        g.setColour (col::inkMuted);
        g.drawText ("-" + juce::String (juce::jmax (0.0f, shown), 1) + " dB", head,
                    juce::Justification::centredRight, false);

        area.removeFromTop (2);
        auto trough = area.removeFromTop (juce::jmin (16, area.getHeight()));
        Chassis::drawRecess (g, trough.toFloat(), (float) metric::radiusMd);

        auto inner = trough.reduced (2);
        const float norm = juce::jlimit (0.0f, 1.0f, shown / juce::jmax (1.0f, maxDb));
        const int w = juce::roundToInt (norm * (float) inner.getWidth());

        if (w > 0)
        {
            g.setColour (norm > 0.6f ? col::warning : col::accent);
            g.fillRect (inner.getRight() - w, inner.getY(), w, inner.getHeight());
        }
    }
}
