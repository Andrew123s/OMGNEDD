#pragma once
#include "Theme.h"
#include "OmgLED.h"
#include "../DSP/Utils.h"

namespace omg::ui
{
    /** Vertical segmented level meter with a printed scale, peak hold and a
        latching clip lamp. Every value it paints comes from the audio thread.
    */
    class OmgMeter : public juce::Component,
                     private juce::Timer
    {
    public:
        explicit OmgMeter (omg::dsp::MeterSource& sourceToUse);

        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

        void setFloorDb (float db) { floorDb = db; }

    private:
        void timerCallback() override;

        omg::dsp::MeterSource& source;
        float floorDb { -48.0f };
        float smoothed[2] { -100.0f, -100.0f };
        float hold { -100.0f };
        bool  clipped { false };

        static constexpr int numSegments = 34;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmgMeter)
    };

    /** Horizontal gain-reduction bar, growing right to left. */
    class OmgGrMeter : public juce::Component,
                       private juce::Timer
    {
    public:
        OmgGrMeter (juce::String legend, float maxReductionDb, std::function<float()> reader);

        void paint (juce::Graphics&) override;

    private:
        void timerCallback() override;

        juce::String label;
        float maxDb { 20.0f }, shown { 0.0f };
        std::function<float()> read;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmgGrMeter)
    };

    /** Horizontal drive meter: how hard the character engine is being hit,
        measured on the audio thread at the point the engine is handed the
        signal. It reads left to right and turns warm past the point where the
        nonlinear stages start working hard.
    */
    class OmgDriveMeter : public juce::Component,
                          public juce::SettableTooltipClient,
                          private juce::Timer
    {
    public:
        explicit OmgDriveMeter (std::function<float()> reader);

        void paint (juce::Graphics&) override;

    private:
        void timerCallback() override;

        float shown { -60.0f };
        std::function<float()> read;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmgDriveMeter)
    };
}
