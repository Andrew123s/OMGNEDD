#pragma once
#include "OmgPanel.h"
#include "OmgKnob.h"
#include "OmgButton.h"
#include "ModeSelector.h"
#include "EqGraphComponent.h"

class OmgnedProcessor;

namespace omg::ui
{
    /** The equaliser module: the display, plus the numeric controls for whichever
        band is selected. Selecting a node rebuilds the row against that band's
        real parameters, so the knobs are never a copy of the band's values.
    */
    class EqPanel : public OmgPanel
    {
    public:
        EqPanel (OmgnedProcessor& processorToUse, juce::AudioProcessorValueTreeState& state);
        ~EqPanel() override;

        void resized() override;

    private:
        void buildRowForBand (int band);

        juce::AudioProcessorValueTreeState& apvts;
        EqGraphComponent graph;
        OmgButton enableButton;

        std::unique_ptr<OmgKnob> freq, gain, q;
        std::unique_ptr<OmgChoiceButton> type;
        std::unique_ptr<OmgButton> dynamic;
        OmgCaption bandCaption { "" };

        int currentBand { -1 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqPanel)
    };
}
