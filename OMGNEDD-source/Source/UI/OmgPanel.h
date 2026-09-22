#pragma once
#include "Theme.h"

namespace omg::ui
{
    /** A brushed-metal module: engraved header, hairline rule, corner screws.
        Every functional block of OMGNEDD lives inside one of these.
    */
    class OmgPanel : public juce::Component
    {
    public:
        explicit OmgPanel (juce::String headerText = {}, bool withScrews = true)
            : header (std::move (headerText)), screws (withScrews) {}

        void setHeader (juce::String text) { header = std::move (text); repaint(); }
        void setScrews (bool shouldShow)   { screws = shouldShow; repaint(); }
        void setHeaderRight (juce::Component* c) { headerRight = c; resized(); }

        /** The area inside the metal, below the header, that children go in. */
        juce::Rectangle<int> getContentArea() const
        {
            auto r = getLocalBounds().reduced (metric::space4);
            if (header.isNotEmpty())
                r.removeFromTop (20 + metric::space3);
            return r;
        }

        void paint (juce::Graphics& g) override
        {
            auto r = getLocalBounds().toFloat().reduced (0.5f);
            Chassis::drawMetal (g, r, (float) metric::radiusLg);

            if (header.isNotEmpty())
            {
                auto head = getLocalBounds().reduced (metric::space4).removeFromTop (20);
                if (headerRight != nullptr)
                    head.removeFromRight (headerRight->getWidth() + metric::space3);
                Fonts::drawLabel (g, header, head, 12.0f, col::ink, 0.18f, juce::Justification::centredLeft, true);

                auto rule = getLocalBounds().reduced (metric::space4).withTop (head.getBottom() + 4).withHeight (2);
                Chassis::drawHairline (g, rule);
            }

            if (screws)
            {
                const float inset = 9.0f;
                for (auto p : { juce::Point<float> (inset, inset),
                                juce::Point<float> (r.getWidth() - inset, inset),
                                juce::Point<float> (inset, r.getHeight() - inset),
                                juce::Point<float> (r.getWidth() - inset, r.getHeight() - inset) })
                    Chassis::drawScrew (g, p, 9.0f);
            }
        }

        void resized() override
        {
            if (headerRight != nullptr && header.isNotEmpty())
            {
                auto head = getLocalBounds().reduced (metric::space4).removeFromTop (20);
                headerRight->setBounds (head.removeFromRight (juce::jlimit (60, 104, head.getWidth() * 2 / 5)));
            }
        }

    private:
        juce::String header;
        bool screws { true };
        juce::Component* headerRight { nullptr };
    };

    /** Small engraved caption used above a group of controls inside a module. */
    class OmgCaption : public juce::Component
    {
    public:
        explicit OmgCaption (juce::String t) : text (std::move (t)) { setInterceptsMouseClicks (false, false); }
        void setText (juce::String t) { text = std::move (t); repaint(); }

        void paint (juce::Graphics& g) override
        {
            Fonts::drawLabel (g, text, getLocalBounds(), 9.0f, col::inkMuted, 0.16f,
                              juce::Justification::centredLeft, true);
        }

    private:
        juce::String text;
    };
}
