#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Theme.h"
#include "OmgLED.h"
#include "../Parameters.h"

namespace omg::ui
{
    /** The raised metal push button: latching when attached to a bool parameter,
        momentary when given an action. Lit means pressed in.
    */
    class OmgButton : public juce::Button
    {
    public:
        /** Latching button attached to a real bool parameter. */
        OmgButton (juce::AudioProcessorValueTreeState& state, juce::StringRef parameterID);

        /** Momentary command button: no parameter, no lamp by default. */
        explicit OmgButton (const juce::String& buttonLegend, std::function<void()> action = {});

        void setLegend (const juce::String& newLegend) { legend = newLegend; repaint(); }
        void setShowLed (bool shouldShow)           { showLed = shouldShow; repaint(); }
        void setLedState (OmgLED::State s)          { ledState = s; repaint(); }
        void setCompact (bool shouldBeCompact)      { compact = shouldBeCompact; repaint(); }
        void setLeftAligned (bool shouldBeLeft)     { leftAligned = shouldBeLeft; repaint(); }

        /** Used by the selectors, which drive their own lit state. */
        void setManualOn (bool shouldBeOn)          { manualOn = shouldBeOn; repaint(); }

        bool isLit() const { return manualOn || getToggleState(); }

        void paintButton (juce::Graphics&, bool shouldDrawAsHighlighted, bool shouldDrawAsDown) override;

    private:
        juce::String legend;
        bool showLed { true }, compact { false }, leftAligned { false }, manualOn { false };
        OmgLED::State ledState { OmgLED::On };

        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmgButton)
    };
}
