#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace omg::ui
{
    /** The OMGNEDD design tokens, in the same names the design system uses. */
    namespace col
    {
        inline const juce::Colour chassis000  { 0xff090909 };
        inline const juce::Colour chassis100  { 0xff121212 };
        inline const juce::Colour chassis200  { 0xff1c1c1c };
        inline const juce::Colour chassis300  { 0xff2a2a2a };
        inline const juce::Colour recess      { 0xff060606 };
        inline const juce::Colour screw       { 0xff3a3a3a };

        inline const juce::Colour hairline    { 0xff393939 };
        inline const juce::Colour edge        { 0xff4a4a4a };
        inline const juce::Colour lineControl { 0xff757575 };

        inline const juce::Colour ink         { 0xffe8e4da };
        inline const juce::Colour inkMuted    { 0xff96938b };
        inline const juce::Colour inkDim      { 0xff5e5b55 };
        inline const juce::Colour onAccent    { 0xff0c1500 };

        inline const juce::Colour accent      { 0xffb7ff3c };
        inline const juce::Colour accentDim   { 0xff6d9a24 };
        inline const juce::Colour accentDeep  { 0xff2f4a12 };
        inline const juce::Colour warning     { 0xffffb52e };
        inline const juce::Colour clip        { 0xffff3b30 };
        inline const juce::Colour ledOff      { 0xff2b2f26 };
        inline const juce::Colour analyser    { 0xff4a5c33 };
    }

    namespace metric
    {
        inline constexpr int designWidth  = 1200;
        inline constexpr int designHeight = 720;
        inline constexpr int minWidth     = 900;
        inline constexpr int minHeight    = 600;

        inline constexpr int space1  = 4;
        inline constexpr int space2  = 8;
        inline constexpr int space3  = 12;
        inline constexpr int space4  = 16;
        inline constexpr int space5  = 20;
        inline constexpr int space6  = 24;
        inline constexpr int space8  = 32;
        inline constexpr int space10 = 40;
        inline constexpr int space12 = 48;

        inline constexpr int knobLarge  = 140;
        inline constexpr int knobMedium = 80;
        inline constexpr int knobSmall  = 60;

        inline constexpr int topBarHeight = 72;
        inline constexpr int radiusSm = 3;
        inline constexpr int radiusMd = 5;
        inline constexpr int radiusLg = 8;
    }

    /** Panel type is Archivo, readouts are IBM Plex Mono, both shipped with the plugin. */
    struct Fonts
    {
        static juce::Font panel (float height, bool heavy = false);

        /** Width of a run of text in a given font. */
        static float widthOf (const juce::Font&, const juce::String&);

        static juce::Font mono  (float height, bool semi = false);

        /** Uppercase label with the letterspacing the design system specifies. */
        static void drawLabel (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> area,
                               float height, juce::Colour colour, float tracking = 0.12f,
                               juce::Justification just = juce::Justification::centred,
                               bool heavy = false);
    };

    /** Shared chassis drawing: brushed metal, recesses, screws, engraved rules. */
    struct Chassis
    {
        static void drawMetal   (juce::Graphics& g, juce::Rectangle<float> r, float corner);
        static void drawRecess  (juce::Graphics& g, juce::Rectangle<float> r, float corner);
        static void drawScrew   (juce::Graphics& g, juce::Point<float> centre, float diameter);
        static void drawHairline (juce::Graphics& g, juce::Rectangle<int> r, bool horizontal = true);
    };
}
