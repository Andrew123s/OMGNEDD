#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters.h"

namespace omg
{
    /** RANDOM, and the locks that keep parts of the panel out of it.

        The specification is explicit that this must not be a loop over every
        parameter with a uniform draw: that produces settings nobody would ever
        dial and mostly sounds broken. Instead one "attitude" is drawn first -
        how far the patch is allowed to go - and every parameter is then drawn
        from a range that follows from it, with the relationships a person would
        keep in mind held by hand:

          * DRIVE and CRUSH move together, but CRUSH lags behind so a heavy
            patch is dirty before it is broken.
          * SMOOTH and the post filter come down as DRIVE goes up, which is what
            stops a random distortion patch from being unusable on a vocal.
          * MIX comes down as the character goes up, so an extreme patch lands
            as a parallel texture rather than a wall.
          * DEPTH, WATER and MURK share one underwater "sink" figure.
          * The compressor threshold and ratio are drawn against each other so
            the pair never adds up to 30 dB of reduction.
          * The EQ moves in small amounts around what is already there.
          * Frequencies, slopes and safety settings are left alone entirely.

        Locks are per module. A locked module is skipped, so a user can keep an
        EQ they like and roll the character engine underneath it.
    */
    class Randomizer
    {
    public:
        enum Module { ModEngine = 0, ModEq, ModDynamics, ModModulation, ModMacros, ModOutput, NumModules };

        static const char* moduleName (int m)
        {
            static const char* names[] = { "ENGINE", "EQ", "DYNAMICS", "MOD", "MACROS", "OUTPUT" };
            return names[juce::jlimit (0, NumModules - 1, m)];
        }

        static const char* lockPropertyName (int m)
        {
            static const char* props[] = { "lockEngine", "lockEq", "lockDynamics", "lockMod", "lockMacros", "lockOutput" };
            return props[juce::jlimit (0, NumModules - 1, m)];
        }

        explicit Randomizer (juce::AudioProcessorValueTreeState& state) : apvts (state) {}

        bool isLocked (int module) const
        {
            return (bool) apvts.state.getProperty (lockPropertyName (module), false);
        }

        void setLocked (int module, bool shouldBeLocked)
        {
            apvts.state.setProperty (lockPropertyName (module), shouldBeLocked, nullptr);
        }

        void toggleLock (int module) { setLocked (module, ! isLocked (module)); }

        /** Fixes the draw sequence. Used by the verification target so a failing
            patch can be reproduced rather than merely reported. */
        void setSeed (juce::int64 seed) { random.setSeed (seed); }

        /** Draws one musically coherent patch. Returns the attitude it used,
            0 (restrained) to 1 (extreme), so the caller can report it. */
        float randomise()
        {
            const float attitude = 0.20f + random.nextFloat() * 0.75f;
            const int engine = isLocked (ModEngine)
                                 ? (int) get (pid::engine)
                                 : random.nextInt (3);

            if (! isLocked (ModEngine))
            {
                set (pid::engine, (float) engine);
                randomiseEngine (engine, attitude);
            }

            if (! isLocked (ModEq))         randomiseEq (attitude);
            if (! isLocked (ModDynamics))   randomiseDynamics (attitude);
            if (! isLocked (ModModulation)) randomiseModulation (engine, attitude);
            if (! isLocked (ModMacros))     randomiseMacros (engine, attitude);
            if (! isLocked (ModOutput))     randomiseOutput (attitude);

            return attitude;
        }

    private:
        float uni (float lo, float hi) { return lo + random.nextFloat() * (hi - lo); }

        float get (juce::StringRef id) const
        {
            if (auto* v = apvts.getRawParameterValue (id)) return v->load();
            return 0.0f;
        }

        void set (juce::StringRef id, float plain)
        {
            if (auto* param = apvts.getParameter (id))
                param->setValueNotifyingHost (param->convertTo0to1 (plain));
        }

        void nudge (juce::StringRef id, float amount, float lo, float hi)
        {
            set (id, juce::jlimit (lo, hi, get (id) + amount));
        }

        void randomiseEngine (int engine, float a)
        {
            switch (engine)
            {
                case Underwater:
                {
                    const float sink = uni (0.35f, 0.55f) + a * 0.45f;      // the shared figure
                    set (pid::uwDepth,    juce::jlimit (0.0f, 100.0f, sink * 100.0f + uni (-12.0f, 12.0f)));
                    set (pid::uwWater,    juce::jlimit (0.0f, 100.0f, sink * 92.0f + uni (-15.0f, 15.0f)));
                    set (pid::uwMurk,     juce::jlimit (0.0f, 100.0f, sink * 70.0f + uni (-18.0f, 18.0f)));
                    set (pid::uwPressure, juce::jlimit (0.0f, 100.0f, 25.0f + a * 50.0f + uni (-10.0f, 10.0f)));
                    set (pid::uwRipple,   juce::jlimit (0.0f, 100.0f, uni (5.0f, 30.0f) + a * 40.0f));
                    set (pid::uwWave,     juce::jlimit (0.0f, 100.0f, uni (10.0f, 40.0f) + a * 35.0f));
                    set (pid::uwBubble,   juce::jlimit (0.0f, 100.0f, uni (0.0f, 25.0f) + a * 35.0f));
                    // resonance stays modest: the filter whistles above about 6
                    set (pid::uwResonance, uni (0.6f, 1.4f) + a * 2.2f);
                    // deep patches want less of themselves at the end
                    set (pid::uwMix, juce::jlimit (35.0f, 100.0f, 100.0f - a * uni (0.0f, 40.0f)));
                    break;
                }

                case Distortion:
                {
                    const float drive = 18.0f + a * 70.0f + uni (-8.0f, 8.0f);
                    set (pid::dsDrive, juce::jlimit (0.0f, 100.0f, drive));
                    // crush lags drive, so a hard patch is dirty before it is broken
                    set (pid::dsCrush, juce::jlimit (0.0f, 100.0f, juce::jmax (0.0f, (a - 0.45f) * 120.0f) * uni (0.3f, 1.0f)));
                    set (pid::dsBite,  juce::jlimit (0.0f, 100.0f, 25.0f + a * 45.0f + uni (-15.0f, 15.0f)));
                    set (pid::dsBody,  juce::jlimit (25.0f, 100.0f, 45.0f + a * 25.0f + uni (-12.0f, 12.0f)));
                    set (pid::dsEdge,  juce::jlimit (0.0f, 100.0f, 15.0f + a * 55.0f + uni (-12.0f, 12.0f)));
                    // smooth rises with drive: this is what keeps it usable on a vocal
                    set (pid::dsSmooth, juce::jlimit (0.0f, 100.0f, 12.0f + a * 55.0f + uni (-8.0f, 8.0f)));
                    set (pid::dsPostFilter, juce::jlimit (2500.0f, 20000.0f, 18000.0f - a * uni (6000.0f, 13000.0f)));
                    set (pid::dsType, (float) random.nextInt (11));
                    set (pid::dsBias, uni (-1.0f, 1.0f) * a * 35.0f);
                    set (pid::dsAsym, uni (-1.0f, 1.0f) * a * 45.0f);
                    set (pid::dsPreEmph, uni (5.0f, 45.0f));
                    // the harder the patch, the less of it goes through
                    set (pid::dsMix, juce::jlimit (30.0f, 100.0f, 100.0f - a * uni (10.0f, 55.0f)));
                    break;
                }

                case Saturation:
                default:
                {
                    set (pid::satDrive,     juce::jlimit (0.0f, 100.0f, 20.0f + a * 55.0f + uni (-10.0f, 10.0f)));
                    set (pid::satWarmth,    juce::jlimit (0.0f, 100.0f, 30.0f + a * 35.0f + uni (-15.0f, 15.0f)));
                    set (pid::satHarmonics, juce::jlimit (0.0f, 100.0f, 25.0f + a * 45.0f + uni (-12.0f, 12.0f)));
                    set (pid::satThickness, juce::jlimit (0.0f, 100.0f, 25.0f + a * 40.0f + uni (-12.0f, 12.0f)));
                    set (pid::satTone,      uni (-1.0f, 1.0f) * 45.0f);
                    set (pid::satSoftClip,  juce::jlimit (0.0f, 100.0f, 20.0f + a * 45.0f));
                    set (pid::satDensity,   juce::jlimit (0.0f, 100.0f, 25.0f + a * 40.0f + uni (-10.0f, 10.0f)));
                    set (pid::satModel,     (float) random.nextInt (6));
                    set (pid::satMix,       juce::jlimit (45.0f, 100.0f, 100.0f - a * uni (0.0f, 35.0f)));
                    break;
                }
            }

            // CHARACTER carries the attitude, centred so 50 is still neutral
            set (pid::character, juce::jlimit (0.0f, 100.0f, 45.0f + a * 45.0f + uni (-8.0f, 8.0f)));
        }

        void randomiseEq (float a)
        {
            // small moves around whatever is there, and only on the bands a
            // vocal engineer would actually reach for
            const float scale = 0.5f + a;
            nudge (eqId (1, "Gain"), uni (-2.5f, 3.0f) * scale, -9.0f, 9.0f);      // LOW
            nudge (eqId (2, "Gain"), uni (-3.5f, 1.5f) * scale, -9.0f, 6.0f);      // LOW MID, mud
            nudge (eqId (3, "Gain"), uni (-2.0f, 2.0f) * scale, -8.0f, 8.0f);      // MID
            nudge (eqId (4, "Gain"), uni (-3.0f, 2.0f) * scale, -9.0f, 7.0f);      // HIGH MID, harshness
            nudge (eqId (5, "Gain"), uni (-1.5f, 3.5f) * scale, -8.0f, 9.0f);      // HIGH, air
        }

        void randomiseDynamics (float a)
        {
            // threshold and ratio drawn against each other so the pair never
            // asks for an absurd amount of reduction
            const float ratio = 1.8f + a * uni (1.0f, 5.0f);
            const float headroom = 26.0f / ratio;
            set (pid::compRatio,   juce::jlimit (1.0f, 20.0f, ratio));
            set (pid::compThresh,  juce::jlimit (-45.0f, -4.0f, -8.0f - headroom * uni (0.6f, 1.4f)));
            set (pid::compAttack,  juce::jlimit (0.5f, 60.0f, uni (3.0f, 25.0f)));
            set (pid::compRelease, juce::jlimit (40.0f, 500.0f, uni (80.0f, 260.0f)));
            set (pid::compMode,    (float) random.nextInt (6));
            set (pid::compMix,     juce::jlimit (55.0f, 100.0f, 100.0f - a * uni (0.0f, 35.0f)));

            set (pid::deAmount, juce::jlimit (0.0f, 100.0f, 30.0f + a * 35.0f + uni (-15.0f, 15.0f)));
            set (pid::deThresh, juce::jlimit (-45.0f, -8.0f, -22.0f + uni (-8.0f, 6.0f)));

            set (pid::trAttack, uni (-1.0f, 1.0f) * a * 45.0f);
            set (pid::trBody,   uni (-1.0f, 1.0f) * a * 35.0f);
        }

        void randomiseModulation (int engine, float a)
        {
            // underwater wants movement; the other two want a little or none
            const bool wantMod = engine == Underwater ? random.nextFloat() < 0.85f
                                                      : random.nextFloat() < 0.45f;
            set (pid::modOn, wantMod ? 1.0f : 0.0f);

            if (! wantMod) return;

            const int mode = engine == Underwater
                               ? (random.nextFloat() < 0.5f ? 0 : 3)          // chorus or wow
                               : random.nextInt (5);

            set (pid::modMode,   (float) mode);
            set (pid::modRate,   juce::jlimit (0.0f, 100.0f, 15.0f + a * 45.0f + uni (-12.0f, 12.0f)));
            set (pid::modDepth,  juce::jlimit (0.0f, 100.0f, 12.0f + a * 45.0f + uni (-10.0f, 10.0f)));
            set (pid::modDetune, juce::jlimit (0.0f, 100.0f, uni (5.0f, 30.0f) + a * 25.0f));
            set (pid::modWidth,  juce::jlimit (0.0f, 100.0f, 35.0f + a * 50.0f));
            set (pid::modMotion, juce::jlimit (0.0f, 100.0f, 15.0f + a * 45.0f));
            // subtle by default, as the specification asks
            set (pid::modMix,    juce::jlimit (8.0f, 65.0f, 15.0f + a * 35.0f));
        }

        void randomiseMacros (int engine, float a)
        {
            set (pid::macBody,   juce::jlimit (0.0f, 100.0f, 50.0f + uni (-22.0f, 22.0f)));
            set (pid::macColor,  juce::jlimit (0.0f, 100.0f, 50.0f + uni (-28.0f, 28.0f)));
            set (pid::macDamage, engine == Distortion ? juce::jlimit (0.0f, 100.0f, a * uni (25.0f, 70.0f))
                                                      : juce::jlimit (0.0f, 100.0f, a * uni (0.0f, 35.0f)));
            set (pid::macDepth,  engine == Underwater ? juce::jlimit (0.0f, 100.0f, 25.0f + a * 55.0f)
                                                      : juce::jlimit (0.0f, 100.0f, uni (0.0f, 35.0f)));
            set (pid::macMotion, juce::jlimit (0.0f, 100.0f, uni (5.0f, 35.0f) + a * 30.0f));
            set (pid::macSpace,  juce::jlimit (0.0f, 100.0f, uni (10.0f, 40.0f) + a * 25.0f));

            set (pid::sigPunch, random.nextFloat() < 0.35f ? 1.0f : 0.0f);
            set (pid::sigChaos, random.nextFloat() < (0.15f + a * 0.35f) ? 1.0f : 0.0f);
            set (pid::sigAir,   random.nextFloat() < 0.40f ? 1.0f : 0.0f);
        }

        void randomiseOutput (float a)
        {
            set (pid::stWidth,  juce::jlimit (70.0f, 155.0f, 100.0f + uni (-15.0f, 45.0f) * a));
            set (pid::mix,      juce::jlimit (35.0f, 100.0f, 100.0f - a * uni (0.0f, 45.0f)));
            // frequencies, slopes, oversampling, the limiter and the safety
            // clip are never touched: a random patch should still be safe
        }

        juce::AudioProcessorValueTreeState& apvts;
        juce::Random random { (juce::int64) juce::Time::currentTimeMillis() };
    };
}
