#pragma once
#include "Theme.h"

namespace omg::ui
{
    /** A lens that is dark until something is true. Four states, one glow layer. */
    class OmgLED : public juce::Component
    {
    public:
        enum State { Off, On, Warn, Clip };

        explicit OmgLED (juce::String legend = {}) : label (std::move (legend))
        {
            setInterceptsMouseClicks (false, false);
        }

        void setState (State s)       { if (s != state) { state = s; repaint(); } }
        State getState() const        { return state; }
        void setLensSize (float size) { lens = size; repaint(); }

        static juce::Colour colourFor (State s)
        {
            switch (s)
            {
                case On:   return col::accent;
                case Warn: return col::warning;
                case Clip: return col::clip;
                case Off:
                default:   return col::ledOff;
            }
        }

        void paint (juce::Graphics& g) override
        {
            auto area = getLocalBounds().toFloat();
            auto lensArea = juce::Rectangle<float> (lens, lens)
                                .withCentre ({ area.getX() + lens * 0.5f, area.getCentreY() });

            const auto c = colourFor (state);

            if (state != Off)
            {
                g.setColour (c.withAlpha (0.28f));
                g.fillEllipse (lensArea.expanded (lens * 0.55f));
            }

            g.setColour (c);
            g.fillEllipse (lensArea);
            g.setColour (juce::Colours::black.withAlpha (0.45f));
            g.drawEllipse (lensArea.reduced (0.5f), 1.0f);

            if (label.isNotEmpty())
            {
                auto textArea = getLocalBounds().withTrimmedLeft (juce::roundToInt (lens) + metric::space2);
                const auto ink = state == Clip ? col::clip : state == Warn ? col::warning
                               : state == On   ? col::accent : col::inkMuted;
                Fonts::drawLabel (g, label, textArea, 9.0f, ink, 0.10f, juce::Justification::centredLeft, true);
            }
        }

    private:
        juce::String label;
        State state { Off };
        float lens { 8.0f };
    };
}
