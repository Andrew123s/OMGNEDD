#include "EqGraphComponent.h"
#include "../PluginProcessor.h"

namespace omg::ui
{
    EqGraphComponent::EqGraphComponent (OmgnedProcessor& processorToUse) : processor (processorToUse)
    {
        setTooltip ("EQ DISPLAY\nDrag a handle for frequency and gain, wheel for Q. The analyser behind the curve is the plugin's own FFT.");
        for (int b = 0; b < kNumEqBands; ++b)
            for (auto* field : { "Freq", "Gain", "Q", "On", "Dyn" })
                ControlRegistry::note (eqId (b, field));

        startTimerHz (30);
    }

    EqGraphComponent::~EqGraphComponent() = default;

    void EqGraphComponent::timerCallback()
    {
        processor.spectrum.updatePoints();
        repaint();
    }

    float EqGraphComponent::freqToX (float hz) const
    {
        const float t = std::log (juce::jlimit (minHz, maxHz, hz) / minHz) / std::log (maxHz / minHz);
        return (float) getX() + t * (float) getWidth() - (float) getX();
    }

    float EqGraphComponent::xToFreq (float x) const
    {
        const float t = juce::jlimit (0.0f, 1.0f, x / juce::jmax (1.0f, (float) getWidth()));
        return minHz * std::pow (maxHz / minHz, t);
    }

    float EqGraphComponent::dbToY (float db) const
    {
        return (float) getHeight() * 0.5f - juce::jlimit (-dbRange, dbRange, db) / dbRange * (float) getHeight() * 0.5f;
    }

    float EqGraphComponent::yToDb (float y) const
    {
        return juce::jlimit (-dbRange, dbRange,
                             ((float) getHeight() * 0.5f - y) / ((float) getHeight() * 0.5f) * dbRange);
    }

    float EqGraphComponent::getBandValue (int band, const char* field) const
    {
        if (auto* v = processor.apvts.getRawParameterValue (eqId (band, field)))
            return v->load();
        return 0.0f;
    }

    void EqGraphComponent::setBandValue (int band, const char* field, float value)
    {
        if (auto* p = processor.apvts.getParameter (eqId (band, field)))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (p->convertTo0to1 (value));
            p->endChangeGesture();
        }
    }

    juce::Point<float> EqGraphComponent::nodePosition (int band) const
    {
        const float f = getBandValue (band, "Freq");
        const int type = (int) getBandValue (band, "Type");
        const bool pass = (type == EqHighPass || type == EqLowPass || type == EqNotch);
        const float gain = pass ? 0.0f : getBandValue (band, "Gain");
        return { freqToX (f), dbToY (gain) };
    }

    int EqGraphComponent::hitTestNode (juce::Point<float> p) const
    {
        for (int b = 0; b < kNumEqBands; ++b)
            if (nodePosition (b).getDistanceFrom (p) < 12.0f)
                return b;
        return -1;
    }

    void EqGraphComponent::mouseMove (const juce::MouseEvent& e)
    {
        const int h = hitTestNode (e.position);
        if (h != hovered) { hovered = h; repaint(); }
        setMouseCursor (h >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }

    void EqGraphComponent::mouseDown (const juce::MouseEvent& e)
    {
        int band = hitTestNode (e.position);

        if (band < 0)
        {
            float best = 1.0e9f;
            for (int b = 0; b < kNumEqBands; ++b)
            {
                const float d = std::abs (nodePosition (b).x - e.position.x);
                if (d < best) { best = d; band = b; }
            }
        }

        if (e.mods.isPopupMenu() && band >= 0)
        {
            juce::PopupMenu menu;
            menu.addSectionHeader (eqBandNames()[band]);
            const bool on = getBandValue (band, "On") > 0.5f;
            const bool dyn = getBandValue (band, "Dyn") > 0.5f;
            menu.addItem (1, on ? "Bypass band" : "Enable band");
            menu.addItem (2, dyn ? "Static" : "Dynamic");
            menu.addItem (3, "Reset gain");

            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                                [this, band, on, dyn] (int r)
            {
                if (r == 1) setBandValue (band, "On", on ? 0.0f : 1.0f);
                else if (r == 2) setBandValue (band, "Dyn", dyn ? 0.0f : 1.0f);
                else if (r == 3) setBandValue (band, "Gain", 0.0f);
            });
            return;
        }

        selected = band;
        dragging = band;

        if (onBandSelected != nullptr)
            onBandSelected (selected);

        repaint();
    }

    void EqGraphComponent::mouseDrag (const juce::MouseEvent& e)
    {
        if (dragging < 0) return;

        const float fine = e.mods.isShiftDown() ? 0.25f : 1.0f;
        const auto start = nodePosition (dragging);
        const auto p = start + (e.position - start) * fine;

        setBandValue (dragging, "Freq", xToFreq (p.x));

        const int type = (int) getBandValue (dragging, "Type");
        if (type != EqHighPass && type != EqLowPass && type != EqNotch)
            setBandValue (dragging, "Gain", yToDb (p.y));

        repaint();
    }

    void EqGraphComponent::mouseUp (const juce::MouseEvent&)
    {
        dragging = -1;
        repaint();
    }

    void EqGraphComponent::mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails& w)
    {
        if (! juce::isPositiveAndBelow (selected, kNumEqBands)) return;

        const float q = getBandValue (selected, "Q");
        setBandValue (selected, "Q", juce::jlimit (0.1f, 18.0f, q * (1.0f + w.deltaY * 0.6f)));
        repaint();
    }

    void EqGraphComponent::paint (juce::Graphics& g)
    {
        auto area = getLocalBounds().toFloat();
        Chassis::drawRecess (g, area, (float) metric::radiusMd);

        g.saveState();
        g.reduceClipRegion (getLocalBounds().reduced (1));

        // ---- grid ------------------------------------------------------------
        static const float gridF[] = { 30, 50, 100, 200, 500, 1000, 2000, 5000, 10000 };
        g.setColour (col::hairline.withAlpha (0.7f));
        for (float f : gridF)
            g.drawVerticalLine (juce::roundToInt (freqToX (f)), 0.0f, (float) getHeight());

        static const float gridDb[] = { -18, -12, -6, 0, 6, 12, 18 };
        for (float db : gridDb)
        {
            g.setColour (col::hairline.withAlpha (juce::approximatelyEqual (db, 0.0f) ? 1.0f : 0.5f));
            g.drawHorizontalLine (juce::roundToInt (dbToY (db)), 0.0f, (float) getWidth());
        }

        // ---- analyser --------------------------------------------------------
        const auto& points = processor.spectrum.getPoints();
        juce::Path spec;
        spec.startNewSubPath (0.0f, (float) getHeight());

        for (size_t i = 0; i < points.size(); ++i)
        {
            const float t = (float) i / (float) (points.size() - 1);
            const float hz = minHz * std::pow (maxHz / minHz, t);
            spec.lineTo (freqToX (hz), (float) getHeight() * (1.0f - points[i]));
        }

        spec.lineTo ((float) getWidth(), (float) getHeight());
        spec.closeSubPath();
        g.setColour (col::analyser.withAlpha (0.45f));
        g.fillPath (spec);

        // ---- curve -----------------------------------------------------------
        juce::Path curve;
        for (int x = 0; x <= getWidth(); ++x)
        {
            const float db = processor.getEqResponseDb (xToFreq ((float) x));
            const float y = dbToY (db);
            if (x == 0) curve.startNewSubPath ((float) x, y);
            else        curve.lineTo ((float) x, y);
        }

        g.setColour (col::accent);
        g.strokePath (curve, juce::PathStrokeType (2.0f));

        // ---- handles ----------------------------------------------------------
        for (int b = 0; b < kNumEqBands; ++b)
        {
            const auto p = nodePosition (b);
            const bool on = getBandValue (b, "On") > 0.5f;
            const bool dyn = getBandValue (b, "Dyn") > 0.5f;
            const bool isSel = (b == selected);

            auto node = juce::Rectangle<float> (12.0f, 12.0f).withCentre (p);

            g.setColour (isSel ? col::accent : col::chassis100);
            g.fillEllipse (node);
            g.setColour (! on ? col::inkDim : isSel ? col::accent : (b == hovered ? col::ink : col::ink.withAlpha (0.8f)));
            g.drawEllipse (node.reduced (1.0f), 2.0f);

            if (dyn)
            {
                g.setColour (col::warning);
                g.drawEllipse (node.expanded (3.0f), 1.0f);
            }
        }

        // ---- axis labels and selected band readout ------------------------------
        g.setFont (Fonts::mono (9.0f));
        g.setColour (col::inkMuted);
        for (auto p : { std::make_pair (100.0f, "100"), std::make_pair (1000.0f, "1k"), std::make_pair (10000.0f, "10k") })
            g.drawText (p.second, juce::roundToInt (freqToX (p.first)) + 3, getHeight() - 14, 40, 12,
                        juce::Justification::centredLeft, false);

        if (juce::isPositiveAndBelow (selected, kNumEqBands))
        {
            const float f = getBandValue (selected, "Freq");
            const auto typeNames = juce::StringArray::fromTokens (juce::String (findParam (eqId (selected, "Type"))->choices), "|", "");
            const int type = juce::jlimit (0, typeNames.size() - 1, (int) getBandValue (selected, "Type"));

            juce::String line;
            line << eqBandNames()[selected] << "   "
                 << (f >= 1000.0f ? juce::String (f / 1000.0f, 2) + "k" : juce::String (juce::roundToInt (f))) << " Hz   "
                 << juce::String (getBandValue (selected, "Gain"), 1) << " dB   Q "
                 << juce::String (getBandValue (selected, "Q"), 2) << "   "
                 << typeNames[type];

            g.setFont (Fonts::mono (9.0f));
            g.setColour (col::ink);
            g.drawText (line, 8, 6, getWidth() - 16, 12, juce::Justification::centredLeft, false);
        }

        g.restoreState();
    }
}
