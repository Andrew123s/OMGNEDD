#include "EqPanel.h"
#include "../PluginProcessor.h"

namespace omg::ui
{
    EqPanel::EqPanel (OmgnedProcessor& processorToUse, juce::AudioProcessorValueTreeState& state)
        : OmgPanel ("EQ"), apvts (state), graph (processorToUse), enableButton (state, pid::eqOn)
    {
        enableButton.setLegend ("EQ ON");
        enableButton.setCompact (true);
        addAndMakeVisible (enableButton);
        setHeaderRight (&enableButton);

        addAndMakeVisible (graph);
        addAndMakeVisible (bandCaption);

        // the control row below the display exposes the type of whichever band is
        // selected, so every band's type parameter is reachable from here
        for (int b = 0; b < kNumEqBands; ++b)
        {
            ControlRegistry::note (eqId (b, "Type"));
            ControlRegistry::note (eqId (b, "Slope"));
        }

        graph.onBandSelected = [this] (int band) { buildRowForBand (band); };
        buildRowForBand (graph.getSelectedBand());
        startTimerHz (8);
    }

    EqPanel::~EqPanel() { stopTimer(); }

    /** A pass band has a slope and no gain to make dynamic; every other shape
        is the other way round. The row shows whichever applies. */
    void EqPanel::timerCallback()
    {
        if (slope == nullptr || dynamic == nullptr || currentBand < 0) return;
        int t = 2;
        if (auto* v = apvts.getRawParameterValue (eqId (currentBand, "Type"))) t = (int) v->load();
        const bool pass = t == EqHighPass || t == EqLowPass;
        slope->setVisible (pass);
        dynamic->setVisible (! pass);
    }

    void EqPanel::buildRowForBand (int band)
    {
        if (band == currentBand) return;
        currentBand = band;

        freq = std::make_unique<OmgKnob> (apvts, eqId (band, "Freq"), OmgKnob::Small);
        gain = std::make_unique<OmgKnob> (apvts, eqId (band, "Gain"), OmgKnob::Small, true);
        q    = std::make_unique<OmgKnob> (apvts, eqId (band, "Q"),    OmgKnob::Small);

        freq->setLabelText ("FREQ");
        gain->setLabelText ("GAIN");
        q->setLabelText ("Q");

        type = std::make_unique<OmgChoiceButton> (apvts, eqId (band, "Type"), "TYPE");

        dynamic = std::make_unique<OmgButton> (apvts, eqId (band, "Dyn"));
        dynamic->setLegend ("DYNAMIC");
        dynamic->setCompact (true);

        slope = std::make_unique<OmgChoiceButton> (apvts, eqId (band, "Slope"));

        bandCaption.setText ("BAND  " + eqBandNames()[band]);

        addAndMakeVisible (*freq);
        addAndMakeVisible (*gain);
        addAndMakeVisible (*q);
        addAndMakeVisible (*type);
        addAndMakeVisible (*dynamic);
        addChildComponent (*slope);

        timerCallback();
        resized();
    }

    void EqPanel::resized()
    {
        OmgPanel::resized();
        auto area = getContentArea();

        auto row = area.removeFromBottom (juce::jlimit (74, OmgKnob::preferredBounds (OmgKnob::Small).getHeight() + 14,
                                                        area.getHeight() * 2 / 5));
        area.removeFromBottom (metric::space2);
        graph.setBounds (area);

        auto caption = row.removeFromTop (12);
        bandCaption.setBounds (caption.removeFromLeft (120));

        if (freq == nullptr) return;

        const int knobW = juce::jlimit (56, OmgKnob::preferredBounds (OmgKnob::Small).getWidth(),
                                        (row.getWidth() - 110) / 3);
        freq->setBounds (row.removeFromLeft (knobW));
        gain->setBounds (row.removeFromLeft (knobW));
        q->setBounds    (row.removeFromLeft (knobW));

        row.removeFromLeft (metric::space2);

        // TYPE over DYNAMIC in one column, so the row never runs out of width
        auto right = row.removeFromLeft (juce::jmax (72, juce::jmin (110, row.getWidth())));
        type->setBounds (right.removeFromTop (juce::jmin (36, right.getHeight())));
        right.removeFromTop (metric::space1);
        dynamic->setBounds (right.removeFromTop (juce::jmin (24, right.getHeight())));
        slope->setBounds (dynamic->getBounds());
    }
}
