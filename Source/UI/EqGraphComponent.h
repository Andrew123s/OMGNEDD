#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "../Parameters.h"

class OmgnedProcessor;

namespace omg::ui
{
    /** The seven band display: log frequency grid, the real FFT behind it, the
        summed response curve, and draggable band handles.

        Nothing here is decorative. The curve is the magnitude of the filters the
        processor is actually running, and the analyser is the processor's FFT.
    */
    class EqGraphComponent : public juce::Component,
                             public juce::SettableTooltipClient,
                             private juce::Timer
    {
    public:
        explicit EqGraphComponent (OmgnedProcessor& processorToUse);
        ~EqGraphComponent() override;

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;
        void mouseDrag (const juce::MouseEvent&) override;
        void mouseUp (const juce::MouseEvent&) override;
        void mouseMove (const juce::MouseEvent&) override;
        void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

        int  getSelectedBand() const { return selected; }
        std::function<void (int)> onBandSelected;

    private:
        void timerCallback() override;

        float freqToX (float hz) const;
        float xToFreq (float x) const;
        float dbToY (float db) const;
        float yToDb (float y) const;

        juce::Point<float> nodePosition (int band) const;
        int  hitTestNode (juce::Point<float> p) const;

        float getBandValue (int band, const char* field) const;
        void  setBandValue (int band, const char* field, float value);

        OmgnedProcessor& processor;
        int selected { 3 }, dragging { -1 }, hovered { -1 };

        static constexpr float minHz = 20.0f, maxHz = 20000.0f, dbRange = 24.0f;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EqGraphComponent)
    };
}
