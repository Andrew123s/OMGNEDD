#pragma once
#include "Utils.h"
#include "UnderwaterEngine.h"
#include "DistortionEngine.h"
#include "SaturationEngine.h"
#include "CompressorSection.h"
#include "EqSection.h"
#include "OutputStage.h"
#include "ModulationEngine.h"
#include "TransientShaper.h"
#include "SpaceEngine.h"

namespace omg::dsp
{
    /** CHARACTER and the six vocal macros.

        None of these are DSP parameters of their own. Every block they are
        applied on top of the values read from the panel, so moving one macro
        moves several real parameters at once.

        How they combine matters more than it looks. The first version added
        an offset and clamped the result, so a DEPTH knob at 70 with a +35
        macro offset sat at the ceiling: turning the knob from 70 to 100 did
        nothing at all. Offsets now *push* proportionally towards the end of
        the range instead, which keeps the panel knob live over its whole
        travel whatever the macros are doing:

            push (v, +a) = v + (hi - v) * a       push (v, -a) = v - (v - lo) * a

        CHARACTER is centred: at 50 it leaves the panel alone. The other macros
        are neutral at their defaults (BODY and COLOR at 50, the rest at 0), so
        a fresh instance does exactly what its panel says.
    */
    struct MacroEngine
    {
        float character = 50.0f;
        float body = 50, color = 50, damage = 0, depth = 0, motion = 0, space = 0;
        bool  punch = false, chaos = false, air = false;

        float chr() const   { return (character - 50.0f) / 50.0f; }      // -1 .. +1
        float chrUp() const { return juce::jmax (0.0f, chr()); }
        float tilt() const  { return (color - 50.0f) / 50.0f; }          // -1 dark .. +1 bright
        float weight() const { return (body - 50.0f) / 50.0f; }          // -1 thin .. +1 heavy

        /** Proportional push; @p amount in the parameter's own units. */
        static float push (float v, float amount, float lo, float hi)
        {
            const float a = juce::jlimit (-1.0f, 1.0f, amount / (hi - lo));
            return juce::jlimit (lo, hi, a >= 0.0f ? v + (hi - v) * a : v + (v - lo) * a);
        }

        void applyUnderwater (UnderwaterEngine::Params& u) const
        {
            const float c = chr();
            u.depth    = push (u.depth,    c * 40.0f + pct (depth) * 30.0f, 0.0f, 100.0f);
            u.water    = push (u.water,    c * 35.0f + pct (depth) * 25.0f + pct (space) * 20.0f, 0.0f, 100.0f);
            u.murk     = push (u.murk,     c * 25.0f - tilt() * 45.0f, 0.0f, 100.0f);
            u.pressure = push (u.pressure, c * 25.0f + weight() * 30.0f + pct (damage) * 25.0f, 0.0f, 100.0f);
            u.ripple   = push (u.ripple,   c * 20.0f + pct (motion) * 55.0f, 0.0f, 100.0f);
            u.wave     = push (u.wave,     c * 15.0f + pct (motion) * 45.0f, 0.0f, 100.0f);
            u.bubble   = push (u.bubble,   c * 20.0f + pct (motion) * 25.0f, 0.0f, 100.0f);
            u.grit     = pct (damage);
            u.chaos    = chaos;
        }

        void applyDistortion (DistortionEngine::Params& d) const
        {
            const float c = chr();
            d.drive  = push (d.drive,  c * 45.0f + pct (damage) * 55.0f, 0.0f, 100.0f);
            d.bite   = push (d.bite,   c * 25.0f + tilt() * 40.0f, 0.0f, 100.0f);
            d.body   = push (d.body,   weight() * 45.0f, 0.0f, 100.0f);
            d.crush  = push (d.crush,  pct (damage) * 50.0f + chrUp() * 15.0f, 0.0f, 100.0f);
            d.edge   = push (d.edge,   c * 30.0f + pct (damage) * 30.0f, 0.0f, 100.0f);
            d.smooth = push (d.smooth, -tilt() * 35.0f, 0.0f, 100.0f);
            d.chaos  = chaos;
        }

        void applySaturation (SaturationEngine::Params& s) const
        {
            const float c = chr();
            s.drive     = push (s.drive,     c * 42.0f + pct (damage) * 45.0f, 0.0f, 100.0f);
            s.warmth    = push (s.warmth,    weight() * 35.0f - tilt() * 20.0f, 0.0f, 100.0f);
            s.harmonics = push (s.harmonics, c * 28.0f + pct (damage) * 40.0f, 0.0f, 100.0f);
            s.thickness = push (s.thickness, weight() * 45.0f, 0.0f, 100.0f);
            s.tone      = push (s.tone,      tilt() * 80.0f, -100.0f, 100.0f);
            s.density   = push (s.density,   c * 22.0f + weight() * 25.0f, 0.0f, 100.0f);
            s.chaos     = chaos;
        }

        void applyCompressor (CompressorSection::Params& comp) const
        {
            comp.threshold = juce::jlimit (-60.0f, 0.0f, comp.threshold - juce::jmax (0.0f, weight()) * 8.0f - chrUp() * 5.0f);
            comp.ratio     = push (comp.ratio, pct (damage) * 6.0f, 1.0f, 20.0f);
            comp.punch     = punch;
        }

        void applyEq (std::array<EqSection::Band, kNumEqBands>& bands) const
        {
            bands[1].gain = juce::jlimit (-24.0f, 24.0f, bands[1].gain + weight() * 6.0f);     // LOW
            bands[2].gain = juce::jlimit (-24.0f, 24.0f, bands[2].gain - weight() * 3.0f);     // LOW MID
            bands[5].gain = juce::jlimit (-24.0f, 24.0f, bands[5].gain + tilt() * 7.0f
                                                                        - pct (depth) * 5.0f   // distance darkens
                                                                        + (air ? 2.5f : 0.0f));
        }

        void applyTransient (TransientShaper::Params& t) const
        {
            if (punch)
            {
                t.enabled = true;
                t.attack = juce::jlimit (-100.0f, 100.0f, t.attack + 35.0f);
            }

            if (std::abs (weight()) > 0.04f)
            {
                t.enabled = true;
                t.body = juce::jlimit (-100.0f, 100.0f, t.body + weight() * 40.0f);
            }
        }

        void applyModulation (ModulationEngine::Params& m) const
        {
            const float mo = pct (motion);
            if (mo > 0.02f)
                m.enabled = true;

            m.depth  = push (m.depth,  mo * 45.0f, 0.0f, 100.0f);
            m.rate   = push (m.rate,   mo * 20.0f, 0.0f, 100.0f);
            m.width  = push (m.width,  pct (space) * 40.0f, 0.0f, 100.0f);
            m.motion = push (m.motion, mo * 55.0f, 0.0f, 100.0f);
            if (mo > 0.02f)
                m.mix = push (m.mix, mo * 35.0f + chrUp() * 10.0f, 0.0f, 100.0f);
            m.chaos  = chaos;
        }

        /** SPACE opens a room around the vocal; DEPTH pushes it further back,
            with a longer pre-delay, a darker tail and more of it. Both leave a
            reverb the user has set up on the SPACE page in charge of its own
            size and decay, and only add to its level. */
        void applySpace (SpaceEngine::Params& sp) const
        {
            const float s = pct (space), dp = pct (depth);
            const float amount = juce::jmax (s, dp);
            if (amount < 0.005f) return;

            if (! sp.revOn)
            {
                sp.revOn = true;
                sp.revMix = 0.0f;
                sp.revSize = 40.0f + s * 30.0f + dp * 25.0f;
                sp.revDecay = 1.1f + s * 1.6f + dp * 2.4f;
                sp.revDamp = 40.0f + dp * 45.0f;
                sp.revPre = 12.0f + dp * 50.0f;
                sp.revDuck = 35.0f;
            }

            sp.revMix = push (sp.revMix, s * 32.0f + dp * 28.0f, 0.0f, 100.0f);
            if (sp.dlyOn)
                sp.dlyMix = push (sp.dlyMix, s * 12.0f, 0.0f, 100.0f);
        }

        void applyOutput (OutputStage::Params& o) const
        {
            o.width = juce::jlimit (0.0f, 200.0f, o.width + pct (space) * 55.0f);
            o.air   = air;
        }
    };
}
