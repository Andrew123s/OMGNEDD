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

namespace omg::dsp
{
    /** CHARACTER and the six vocal macros.

        None of these are parameters of their own in the DSP; they are offsets
        applied on top of the per-engine controls every block, so a user can move
        one macro and see several real parameters move with it.
    */
    struct MacroEngine
    {
        float character = 50.0f;
        float body = 50, color = 50, damage = 0, depth = 30, motion = 20, space = 25;
        bool  punch = false, chaos = false, air = false;

        // CHARACTER is centred: 50 % leaves the panel settings alone.
        float chr() const { return (character - 50.0f) / 50.0f; }      // -1 .. +1
        float chrUp() const { return juce::jmax (0.0f, chr()); }

        static float add (float v, float amount, float lo, float hi)
        {
            return juce::jlimit (lo, hi, v + amount);
        }

        void applyUnderwater (UnderwaterEngine::Params& u) const
        {
            const float c = chr();
            u.depth    = add (u.depth,    c * 42.0f + pct (depth) * 30.0f, 0.0f, 100.0f);
            u.water    = add (u.water,    c * 38.0f + pct (depth) * 20.0f, 0.0f, 100.0f);
            u.murk     = add (u.murk,     c * 30.0f - bipolar (color - 50.0f) * 40.0f, 0.0f, 100.0f);
            u.pressure = add (u.pressure, c * 26.0f + pct (body) * 16.0f - 8.0f, 0.0f, 100.0f);
            u.ripple   = add (u.ripple,   c * 18.0f + pct (motion) * 45.0f, 0.0f, 100.0f);
            u.wave     = add (u.wave,     c * 16.0f + pct (motion) * 40.0f, 0.0f, 100.0f);
            u.bubble   = add (u.bubble,   c * 20.0f + pct (space) * 25.0f, 0.0f, 100.0f);
            u.chaos    = chaos;
        }

        void applyDistortion (DistortionEngine::Params& d) const
        {
            const float c = chr();
            d.drive  = add (d.drive,  c * 45.0f + pct (damage) * 55.0f, 0.0f, 100.0f);
            d.bite   = add (d.bite,   c * 25.0f + bipolar (color - 50.0f) * 35.0f, 0.0f, 100.0f);
            d.body   = add (d.body,   pct (body) * 40.0f - 20.0f, 0.0f, 100.0f);
            d.crush  = add (d.crush,  pct (damage) * 45.0f + chrUp() * 15.0f, 0.0f, 100.0f);
            d.edge   = add (d.edge,   c * 30.0f + pct (damage) * 25.0f, 0.0f, 100.0f);
            d.smooth = add (d.smooth, -bipolar (color - 50.0f) * 30.0f, 0.0f, 100.0f);
            d.chaos  = chaos;
        }

        void applySaturation (SaturationEngine::Params& s) const
        {
            const float c = chr();
            s.drive     = add (s.drive,     c * 42.0f + pct (damage) * 40.0f, 0.0f, 100.0f);
            s.warmth    = add (s.warmth,    pct (body) * 35.0f - 17.0f, 0.0f, 100.0f);
            s.harmonics = add (s.harmonics, c * 28.0f + pct (damage) * 35.0f, 0.0f, 100.0f);
            s.thickness = add (s.thickness, pct (body) * 40.0f - 20.0f, 0.0f, 100.0f);
            s.tone      = add (s.tone,      bipolar (color - 50.0f) * 70.0f, -100.0f, 100.0f);
            s.density   = add (s.density,   c * 22.0f + pct (body) * 20.0f, 0.0f, 100.0f);
            s.chaos     = chaos;
        }

        void applyCompressor (CompressorSection::Params& comp) const
        {
            comp.threshold = juce::jlimit (-60.0f, 0.0f, comp.threshold - pct (body) * 8.0f - chrUp() * 5.0f);
            comp.ratio     = juce::jlimit (1.0f, 20.0f, comp.ratio + pct (damage) * 4.0f);
            comp.punch     = punch;
        }

        void applyEq (std::array<EqSection::Band, kNumEqBands>& bands) const
        {
            const float tilt = bipolar (color - 50.0f);       // -1 dark .. +1 bright
            const float weight = pct (body) - 0.5f;

            bands[1].gain = juce::jlimit (-24.0f, 24.0f, bands[1].gain + weight * 5.0f);   // LOW
            bands[2].gain = juce::jlimit (-24.0f, 24.0f, bands[2].gain - weight * 2.5f);   // LOW MID
            bands[5].gain = juce::jlimit (-24.0f, 24.0f, bands[5].gain + tilt * 5.0f);     // HIGH

            if (air)
                bands[5].gain = juce::jlimit (-24.0f, 24.0f, bands[5].gain + 2.0f);
        }

        /** MOTION is the macro that owns the modulation section: it turns it on
            once it is past a whisper, and takes the depth, the width and the
            blend with it. The panel's own MOD controls stay the floor. */
        void applyModulation (ModulationEngine::Params& m) const
        {
            const float mo = pct (motion);
            if (mo > 0.02f)
                m.enabled = true;

            m.depth  = add (m.depth,  mo * 35.0f, 0.0f, 100.0f);
            m.rate   = add (m.rate,   mo * 20.0f, 0.0f, 100.0f);
            m.width  = add (m.width,  pct (space) * 30.0f, 0.0f, 100.0f);
            m.motion = add (m.motion, mo * 40.0f, 0.0f, 100.0f);
            m.mix    = add (m.mix,    mo * 25.0f + chrUp() * 8.0f, 0.0f, 100.0f);
            m.chaos  = chaos;
        }

        /** PUNCH and BODY both reach the transient shaper: PUNCH sharpens the
            front of the word, BODY fills what follows it. */
        void applyTransient (TransientShaper::Params& t) const
        {
            if (punch)
            {
                t.enabled = true;
                t.attack = juce::jlimit (-100.0f, 100.0f, t.attack + 30.0f);
            }

            const float weight = pct (body) - 0.5f;
            if (std::abs (weight) > 0.04f)
            {
                t.enabled = true;
                t.body = juce::jlimit (-100.0f, 100.0f, t.body + weight * 40.0f);
            }
        }

        void applyOutput (OutputStage::Params& o) const
        {
            o.width = juce::jlimit (0.0f, 200.0f, o.width + pct (space) * 45.0f);
            o.air   = air;
        }
    };
}
