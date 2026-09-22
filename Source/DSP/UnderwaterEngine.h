#pragma once
#include "Utils.h"
#include "Filters.h"

namespace omg::dsp
{
    /** The submerged engine, voiced for the underground / rage rap vocal.

        That sound is not a low-pass filter. It is a vocal pushed into a dark,
        resonant filter with some grit in front of it, a pitch that will not
        sit still, a filter that breathes and gurgles, a squashed pumping
        density, and the whole thing sunk into a dark wash that blooms in the
        gaps between words. This engine does everything except the wash, which
        needs to run at the session rate: WATER sends to the SPACE module, and
        the processor combines that send with the SPACE panel.

        Per channel, per sample (inside the oversampler):

          MURK      low-mid body lift at 280 Hz and a dark shelf above 2.2 kHz
          PRESSURE  drive into the filter, then a pumping compressor after it
          DEPTH     the filter cutoff, 16 kHz down to 400 Hz, exponential
          WATER     extra resonance on the filter, plus the wash send
          WAVE      a slow LFO sweeping the cutoff by up to 2.2 octaves
          BUBBLE    a smoothed random gurgle on the cutoff, 6 to 16 Hz
          RIPPLE    pitch warble: a short modulated delay, up to 45 cents

        Auto gain matches the engine's output level to its input, so DEPTH is
        heard as the vocal going under, not as the vocal getting quieter.

        The engine is told the rate it actually runs at. It used to be prepared
        once at eight times the session rate and then run at two, which put
        every filter and LFO in it at a quarter of its intended frequency.
    */
    class UnderwaterEngine
    {
    public:
        struct Params
        {
            float depth = 45, water = 50, murk = 35, pressure = 40;
            float ripple = 25, wave = 30, bubble = 20, mix = 100;
            float resonance = 1.2f, modPhaseDeg = 90.0f, pressResponse = 50.0f;
            float grit = 0.0f;          // extra pre-drive, 0..1, from the DAMAGE macro
            int   slope = 1;            // 0..3 -> 12/24/36/48 dB
            int   modShape = 0;         // wave LFO: sine, triangle, random, square
            bool  chaos = false;
        };

        /** Allocates for the highest rate the engine can ever run at. */
        void prepare (double maxRate, int /*maxBlock*/)
        {
            for (auto& ch : chans)
                ch.warble.allocate ((int) (0.008 * maxRate) + 8);
            setSampleRate (maxRate);
        }

        /** Retunes everything for the rate the engine is about to run at.
            Allocation free; resets state, so call it when the rate changes. */
        void setSampleRate (double fs)
        {
            sampleRate = fs;
            for (int c = 0; c < 2; ++c)
            {
                auto& ch = chans[(size_t) c];
                ch.cutoffSmooth.setTime (fs, 0.004);
                ch.bubbleSmooth.setTime (fs, 0.022);
                ch.swingSmooth.setTime (fs, 0.030);
                ch.pumpEnv.prepare (fs);
                ch.autoGain.prepare (fs, 0.30, 18.0f, 12.0f);
                ch.dcHp.set (BiquadCoeffs::highPass1 (fs, 60.0));

            }
            reset();
            setParams (p);
        }

        void reset()
        {
            for (auto& ch : chans)
            {
                for (auto& s : ch.svf) s.reset();
                ch.murkBody.reset(); ch.murkDark.reset(); ch.dcHp.reset();
                ch.warble.clear();
                ch.cutoffSmooth.reset (float (std::log2 (baseCutoff)));
                ch.bubbleSmooth.reset();
                ch.swingSmooth.reset (rippleSwingSamples);
                ch.pumpEnv.env = 0.0f;
                ch.autoGain.reset();
                ch.waveT = ch.rippleT = ch.bubbleT = 0.0;
                ch.bubbleTarget = 0.0f;
                ch.wander = 0.0f;
            }
            for (size_t c = 0; c < chans.size(); ++c) chans[c].random.setSeed ((juce::int64) (0xB0B + c));
        }

        void setParams (const Params& np)
        {
            p = np;
            const double fs = sampleRate;

            const float depth = pct (p.depth), water = pct (p.water), murk = pct (p.murk);

            // DEPTH: 16 kHz fully up, 2.5 kHz at half, 400 Hz at the bottom
            baseCutoff = 16000.0 * std::pow (0.025, (double) depth);

            // resonance from the panel, and WATER makes it wetter still
            filterQ = 0.55 + (double) juce::jlimit (0.1f, 12.0f, p.resonance) * 0.75 + water * 2.4;
            stages = juce::jlimit (1, 4, p.slope + 1);

            for (auto& ch : chans)
            {
                ch.murkBody.set (BiquadCoeffs::peak (fs, 280.0, 0.7, murk * 8.0));
                ch.murkDark.set (BiquadCoeffs::highShelf (fs, 2200.0, 0.6, -murk * 14.0));
            }

            const float pr = pct (p.pressure);
            drive = 1.0f + pr * 5.0f + p.grit * 8.0f;
            pumpThresholdDb = -10.0f - pr * 22.0f;
            pumpRatio = 1.0f + pr * 9.0f;

            const float resp = pct (p.pressResponse);
            for (auto& ch : chans)
                ch.pumpEnv.setTimes (lerp (40.0f, 1.0f, resp), lerp (500.0f, 60.0f, resp));

            const float wave = pct (p.wave);
            waveHz  = 0.07 + wave * wave * 1.1;
            waveOct = wave * 2.2;

            const float bubble = pct (p.bubble);
            bubbleHz  = 6.0 + bubble * 10.0;
            bubbleOct = bubble * 1.6;
            bubbleQBoost = bubble * 2.0;

            const float ripple = pct (p.ripple);
            rippleHz = 4.5 + ripple * 3.0;
            // peak pitch deviation d relates to delay swing A by d = 2*pi*f*A
            const double cents = ripple * 45.0;
            const double ratio = std::pow (2.0, cents / 1200.0) - 1.0;
            rippleSwingSamples = (float) (ratio / (2.0 * kPi * rippleHz) * fs);

            stereoOffset = p.modPhaseDeg / 360.0;
            wet = pct (p.mix);
        }

        void process (juce::AudioBuffer<float>& buffer)
        {
            const int numCh = juce::jmin (2, buffer.getNumChannels());
            const int n = buffer.getNumSamples();
            const double fs = sampleRate;
            const double logBase = std::log2 (baseCutoff);
            const float pumpSlope = 1.0f - 1.0f / pumpRatio;
            const float driveNorm = 1.0f / std::tanh (drive);

            for (int c = 0; c < numCh; ++c)
            {
                auto& ch = chans[(size_t) c];
                auto* d = buffer.getWritePointer (c);
                const double offset = c == 1 ? stereoOffset : 0.0;

                for (int i = 0; i < n; ++i)
                {
                    const float dry = d[i];

                    // --- murk, then drive into the filter
                    float x = ch.murkDark.process (ch.murkBody.process (dry));
                    x = std::tanh (x * drive) * driveNorm;

                    // --- cutoff modulation: wave, bubble, chaos
                    ch.waveT += waveHz / fs;       if (ch.waveT >= 1.0) ch.waveT -= 1.0;
                    const float waveLfo = shapeAt (p.modShape, ch.waveT + offset, ch);

                    ch.bubbleT += bubbleHz / fs;
                    if (ch.bubbleT >= 1.0)
                    {
                        ch.bubbleT -= 1.0;
                        ch.bubbleTarget = ch.random.nextFloat() * 2.0f - 1.0f;
                    }
                    const float gurgle = ch.bubbleSmooth.process (ch.bubbleTarget);

                    if (p.chaos)
                        ch.wander = juce::jlimit (-1.0f, 1.0f, ch.wander * 0.9995f + (ch.random.nextFloat() - 0.5f) * 0.004f);

                    const double targetLog = logBase + waveLfo * waveOct + gurgle * bubbleOct + ch.wander * 0.6;
                    const double cut = std::pow (2.0, (double) ch.cutoffSmooth.process ((float) targetLog));

                    // --- the resonant filter, one tan() shared across the cascade
                    const double g = Svf::gFor (fs, cut);
                    const double qMain = filterQ + std::abs (gurgle) * bubbleQBoost;

                    for (int s = 0; s < stages; ++s)
                    {
                        auto& f = ch.svf[(size_t) s];
                        f.tuneG (g, s == 0 ? qMain : 0.707);
                        f.process (x);
                        x = (float) f.lp;
                    }

                    // --- pitch warble. Always read through the line, with the
                    // swing smoothed, so turning RIPPLE never steps the delay.
                    ch.warble.push (x);
                    {
                        ch.rippleT += rippleHz / fs;  if (ch.rippleT >= 1.0) ch.rippleT -= 1.0;
                        const float lfo = (float) std::sin (2.0 * kPi * (ch.rippleT + offset))
                                        + (p.chaos ? ch.wander * 0.4f : 0.0f);
                        const float swing = ch.swingSmooth.process (rippleSwingSamples);
                        x = ch.warble.read (swing * (1.2f + lfo));
                    }

                    // --- the pump
                    if (pumpRatio > 1.01f)
                    {
                        const float envDb = gainToDb (ch.pumpEnv.process (std::abs (x)) + 1.0e-9f);
                        const float over = juce::jmax (0.0f, envDb - pumpThresholdDb);
                        x *= dbToGain (-over * pumpSlope);
                    }

                    x = ch.dcHp.process (x);

                    // --- level back to where it came in
                    x = ch.autoGain.process (dry, x);

                    d[i] = dry + (x - dry) * wet;
                }
            }
        }

        /** How much WATER wants the SPACE module to wash the vocal, 0..1. */
        float getWashAmount() const noexcept { return pct (p.water); }

    private:
        struct Channel
        {
            std::array<Svf, 4> svf;
            Biquad murkBody, murkDark, dcHp;
            DelayLine warble;
            OnePole cutoffSmooth, bubbleSmooth, swingSmooth;
            EnvFollower pumpEnv;
            AutoGain autoGain;
            juce::Random random;
            double waveT { 0.0 }, rippleT { 0.0 }, bubbleT { 0.0 };
            float bubbleTarget { 0.0f }, wander { 0.0f }, heldRandom { 0.0f };
            double lastWaveCycle { -1.0 };
        };

        /** The WAVE LFO, bipolar. RANDOM holds a new value each cycle. */
        float shapeAt (int shape, double t, Channel& ch) const noexcept
        {
            t -= std::floor (t);
            switch (shape)
            {
                case 1:  return (float) (4.0 * std::abs (t - 0.5) - 1.0);
                case 2:
                {
                    if (t < ch.lastWaveCycle) ch.heldRandom = ch.random.nextFloat() * 2.0f - 1.0f;
                    ch.lastWaveCycle = t;
                    return ch.heldRandom;
                }
                case 3:  return t < 0.5 ? 1.0f : -1.0f;       // the cutoff smoother rounds the edge
                default: return (float) std::sin (2.0 * kPi * t);
            }
        }

        std::array<Channel, 2> chans;
        Params p;
        double sampleRate { 88200.0 };
        double baseCutoff { 3000.0 }, filterQ { 1.2 };
        double waveHz { 0.3 }, waveOct { 0.5 }, bubbleHz { 8.0 }, bubbleOct { 0.3 }, rippleHz { 5.0 };
        double stereoOffset { 0.25 };
        float bubbleQBoost { 0.0f }, rippleSwingSamples { 0.0f };
        float drive { 1.0f }, pumpThresholdDb { -20.0f }, pumpRatio { 1.0f }, wet { 1.0f };
        int stages { 2 };
    };
}
