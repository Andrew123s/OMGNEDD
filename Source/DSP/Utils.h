#pragma once
#include <juce_dsp/juce_dsp.h>
#include <atomic>
#include <cmath>

namespace omg::dsp
{
    inline float dbToGain (float db)          { return juce::Decibels::decibelsToGain (db, -100.0f); }
    inline float gainToDb (float g)           { return juce::Decibels::gainToDecibels (g, -100.0f); }
    inline float pct (float p)                { return juce::jlimit (0.0f, 1.0f, p * 0.01f); }
    inline float bipolar (float p)            { return juce::jlimit (-1.0f, 1.0f, p * 0.01f); }
    inline float lerp (float a, float b, float t) { return a + (b - a) * juce::jlimit (0.0f, 1.0f, t); }

    /** One-pole smoother used for anything that would otherwise zipper. */
    struct Smooth
    {
        void reset (double sampleRate, double seconds, float initial = 0.0f)
        {
            a = (float) std::exp (-1.0 / juce::jmax (1.0, seconds * sampleRate));
            z = initial;
        }
        inline float next (float target) noexcept { z = target + a * (z - target); return z; }
        inline float value() const noexcept       { return z; }
        void setImmediate (float v) noexcept      { z = v; }

        float a { 0.99f }, z { 0.0f };
    };

    /** Envelope follower with independent attack and release times. */
    struct EnvFollower
    {
        void prepare (double sr) { sampleRate = sr; setTimes (attackMs, releaseMs); env = 0.0f; }

        void setTimes (float atkMs, float relMs)
        {
            attackMs = atkMs; releaseMs = relMs;
            atkCoef = (float) std::exp (-1.0 / juce::jmax (1.0, (double) atkMs * 0.001 * sampleRate));
            relCoef = (float) std::exp (-1.0 / juce::jmax (1.0, (double) relMs * 0.001 * sampleRate));
        }

        inline float process (float rectified) noexcept
        {
            const float c = rectified > env ? atkCoef : relCoef;
            env = rectified + c * (env - rectified);
            return env;
        }

        double sampleRate { 44100.0 };
        float attackMs { 10.0f }, releaseMs { 100.0f };
        float atkCoef { 0.0f }, relCoef { 0.0f }, env { 0.0f };
    };

    /** Multi-shape LFO used by the underwater engine and the motion macro. */
    struct Lfo
    {
        enum Shape { Sine = 0, Triangle, Random, Square };

        void prepare (double sr) { sampleRate = sr; phase = 0.0; }
        void setFrequency (double hz) { inc = hz / juce::jmax (1.0, sampleRate); }
        void setPhaseOffset (double turns) { offset = turns; }

        inline float next (int shape) noexcept
        {
            phase += inc;
            if (phase >= 1.0)
            {
                phase -= 1.0;
                held = random.nextFloat() * 2.0f - 1.0f;
            }

            const double p = phase + offset - std::floor (phase + offset);

            switch (shape)
            {
                case Triangle: return (float) (4.0 * std::abs (p - 0.5) - 1.0);
                case Random:   return held;
                case Square:   return p < 0.5 ? 1.0f : -1.0f;
                case Sine:
                default:       return (float) std::sin (p * juce::MathConstants<double>::twoPi);
            }
        }

        double sampleRate { 44100.0 }, phase { 0.0 }, inc { 0.0 }, offset { 0.0 };
        float held { 0.0f };
        juce::Random random { 0x10F0 };
    };

    /** Peak, RMS and clip state handed from the audio thread to the editor. */
    struct MeterSource
    {
        void prepare (double sr, int numChannels)
        {
            sampleRate = sr;
            channels = juce::jlimit (1, 2, numChannels);
            decay = (float) std::exp (-1.0 / (0.30 * sr));      // 300 ms analogue fall
            for (int c = 0; c < 2; ++c) { peak[c] = 0.0f; rms[c] = 0.0f; }
            hold.store (-100.0f);
            clipped.store (false);
        }

        void push (const juce::AudioBuffer<float>& buffer)
        {
            const int n = buffer.getNumSamples();
            if (n <= 0) return;

            for (int c = 0; c < juce::jmin (channels, buffer.getNumChannels()); ++c)
            {
                const auto* d = buffer.getReadPointer (c);
                float mx = 0.0f, sum = 0.0f;

                for (int i = 0; i < n; ++i)
                {
                    const float a = std::abs (d[i]);
                    mx = juce::jmax (mx, a);
                    sum += d[i] * d[i];
                }

                float p = peak[c];
                p *= std::pow (decay, (float) n);
                peak[c] = juce::jmax (p, mx);
                rms[c]  = std::sqrt (sum / (float) n);

                displayPeak[c].store (gainToDb (peak[c]));
                displayRms[c].store  (gainToDb (rms[c]));

                if (mx >= 0.999f) clipped.store (true);

                const float db = gainToDb (mx);
                if (db > hold.load()) hold.store (db);
            }
        }

        void clearClip() { clipped.store (false); hold.store (-100.0f); }

        float getPeakDb (int c) const { return displayPeak[juce::jlimit (0, 1, c)].load(); }
        float getRmsDb  (int c) const { return displayRms [juce::jlimit (0, 1, c)].load(); }
        float getHoldDb() const       { return hold.load(); }
        bool  isClipped() const       { return clipped.load(); }

        double sampleRate { 44100.0 };
        int channels { 2 };
        float decay { 0.999f };
        float peak[2] { 0.0f, 0.0f }, rms[2] { 0.0f, 0.0f };
        std::atomic<float> displayPeak[2] { { -100.0f }, { -100.0f } };
        std::atomic<float> displayRms[2]  { { -100.0f }, { -100.0f } };
        std::atomic<float> hold { -100.0f };
        std::atomic<bool>  clipped { false };
    };
}
