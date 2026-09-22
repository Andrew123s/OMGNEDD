#pragma once
#include "Theme.h"

namespace omg::ui
{
    /** The OMGNEDD lockup: the name in heavy Archivo with an accent bar struck
        through the counter of the first O, over the product line.
    */
    class Wordmark : public juce::Component
    {
    public:
        Wordmark() { setInterceptsMouseClicks (false, false); }

        void setShowProductLine (bool shouldShow) { showSub = shouldShow; repaint(); }

        void paint (juce::Graphics& g) override
        {
            auto area = getLocalBounds();
            auto nameArea = showSub ? area.removeFromTop (area.getHeight() - 13) : area;

            const float h = juce::jlimit (22.0f, 34.0f, (float) nameArea.getHeight() * 0.86f);
            auto font = Fonts::panel (h, true);

            g.setFont (font);
            g.setColour (col::ink);

            const float baseline = (float) nameArea.getCentreY() + h * 0.36f;
            g.drawSingleLineText ("OMGNEDD", nameArea.getX(), juce::roundToInt (baseline));

            // the bar through the counter of the first O
            const float oWidth = Fonts::widthOf (font, "O");
            g.setColour (col::accent);
            g.fillRect ((float) nameArea.getX() + oWidth * 0.10f,
                        baseline - h * 0.46f,
                        oWidth * 0.80f,
                        juce::jmax (3.0f, h * 0.10f));

            if (showSub)
                Fonts::drawLabel (g, "VOCAL CHARACTER PROCESSOR", area, 9.0f, col::inkMuted,
                                  0.28f, juce::Justification::centredLeft, false);
        }

    private:
        bool showSub { true };
    };
}
