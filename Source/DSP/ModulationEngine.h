#pragma once
#include "Utils.h"

namespace omg::dsp
{
    /** Pitch and time modulation: chorus, micro pitch, vibrato, tape wow and
        tape flutter.

        All five modes are built from the same primitive - a short interpolated
        delay line whose read position is moved by an LFO - because that is what
        the analogue and tape machines they are named after actually did. The
        modes differ in how the read position is driven:

          CHORUS       two voices around a 14 ms delay, moved by a slow sine in
                       opposite directions, mixed back against the dry signal.
          MICRO PITCH  two crossfaded taps sliding at a constant rate, which is
                       a fixed pitch offset in cents rather than a vibrato.
          VIBRATO      one voice, fully wet, sine modulated.
          TAPE WOW     one voice, fully wet, very slow drift with a second
                       incommensurate component so it never repeats obviously.
          TAPE FLUTTER fast shallow modulation plus a filtered random walk.

        DETUNE is always a real pitch offset - the micro pitch taps run at a
        constant rate in both directions, so the two voices sit a few cents
        either side of the original.
    */
    class ModulationEngine
    {
    public:
        enum Mode { Chorus = 0, MicroPitch, Vibrato, TapeWow, TapeFlutter };

        struct Params
        {
            float rate = 35, depth = 30, detune = 20, width = 50, motion = 25, mix = 35;
            int   mode = Chorus;
            bool  enabled = false, chaos = false;
        };

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            maxDelaySamples = juce::jmax (64, (int) (0.060 * sampleRate) + 4);

            for (auto& ch : chans)
            {
                ch.line.assign ((size_t) maxDelaySamples, 0.0f);
                ch.writePos = 0;
                ch.lfo.prepare (sampleRate);
                ch.drift.prepare (sampleRate);
                ch.phase = 0.0;
                ch.randomWalk = 0.0f;
                ch.delaySmooth.reset (sampleRate, 0.010, 14.0f);
                ch.hp.prepare (spec);
                ch.hp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 90.0f, 0.707f);
            }

            setParams (p);
        }

        void reset()
        {
            for (auto& ch : chans)
            {
                std::fill (ch.line.begin(), ch.line.end(), 0.0f);
                ch.writePos = 0;
                ch.randomWalk = 0.0f;
                ch.hp.reset();
            }
        }

        void setParams (const Params& np)
        {
            p = np;

            const float r = pct (p.rate);
            const float d = pct (p.depth);

            switch (p.mode)
            {
                case MicroPitch:  lfoHz = lerp (0.05f, 0.5f, r);  break;
                case Vibrato:     lfoHz = lerp (0.8f, 8.0f, r);   break;
                case TapeWow:     lfoHz = lerp (0.15f, 1.6f, r);  break;
                case TapeFlutter: lfoHz = lerp (5.0f, 18.0f, r);  break;
                case Chorus:
                default:          lfoHz = lerp (0.12f, 3.2f, r);  break;
            }

            switch (p.mode)
            {
                case MicroPitch:  baseDelayMs = 22.0f; depthMs = 0.0f;            break;
                case Vibrato:     baseDelayMs = 6.0f;  depthMs = d * 4.5f;        break;
                case TapeWow:     baseDelayMs = 9.0f;  depthMs = d * 7.0f;        break;
                case TapeFlutter: baseDelayMs = 5.0f;  depthMs = d * 0.9f;        break;
                case Chorus:
                default:          baseDelayMs = 14.0f; depthMs = d * 7.5f;        break;
            }

            // detune in cents -> the rate at which a micro pitch tap slides
            centsUp   =  pct (p.detune) * 22.0f;
            centsDown = -pct (p.detune) * 22.0f;

            widthAmt  = pct (p.width);
            motionAmt = pct (p.motion);
            mixAmt    = pct (p.mix);

            // MOTION widens the modulation and, on the tape modes, adds drift
            const double spread = 0.5 * (double) widthAmt;

            for (int c = 0; c < 2; ++c)
            {
                auto& ch = chans[(size_t) c];
                ch.lfo.setFrequency (lfoHz * (c == 1 ? 1.0 + 0.04 * motionAmt : 1.0));
                ch.lfo.setPhaseOffset (c == 1 ? spread : 0.0);
                ch.drift.setFrequency (lfoHz * 0.173 + 0.07);
                ch.drift.setPhaseOffset (c == 1 ? 0.37 : 0.0);
            }

            // a micro pitch tap slides by this many samples per sample
            slideUp   = (float) (std::pow (2.0, centsUp   / 1200.0) - 1.0);
            slideDown = (float) (std::pow (2.0, centsDown / 1200.0) - 1.0);

            // a 25 ms window is long enough that the crossfade is not heard as
            // tremolo and short enough that the doubling is not heard as an echo
            crossfadeSamples = juce::jmax (64.0f, (float) sampleRate * 0.025f);
        }

        void process (juce::AudioBuffer<float>& buffer)
        {
            if (! p.enabled || mixAmt <= 0.0001f)
                return;

            const int numCh = juce::jmin (2, buffer.getNumChannels());
            const int n = buffer.getNumSamples();

            for (int c = 0; c < numCh; ++c)
            {
                auto& ch = chans[(size_t) c];
                auto* d = buffer.getWritePointer (c);

                for (int i = 0; i < n; ++i)
                {
                    const float dry = d[i];

                    // the line is fed the dry signal, high passed so modulation
                    // never smears the low end of the vocal
                    ch.line[(size_t) ch.writePos] = ch.hp.processSample (dry);

                    float wet = 0.0f;

                    if (p.mode == MicroPitch)
                    {
                        wet = readPitchVoice (ch, slideUp, false) * 0.5f
                            + readPitchVoice (ch, slideDown, true) * 0.5f;
                    }
                    else
                    {
                        float mod = ch.lfo.next (Lfo::Sine);

                        if (p.mode == TapeWow)
                            mod = mod * 0.75f + ch.drift.next (Lfo::Sine) * 0.25f * (0.4f + motionAmt);

                        if (p.mode == TapeFlutter)
                        {
                            // a filtered random walk, bounded, so flutter is not
                            // a clean tone and never runs away
                            const float step = (ch.random.nextFloat() - 0.5f) * 0.06f;
                            ch.randomWalk = juce::jlimit (-1.0f, 1.0f, ch.randomWalk * 0.995f + step);
                            mod = mod * 0.7f + ch.randomWalk * 0.3f * (0.5f + motionAmt);
                        }

                        if (p.chaos)
                            mod += (ch.random.nextFloat() - 0.5f) * 0.02f;

                        const float sideSign = (c == 1 && p.mode == Chorus) ? -1.0f : 1.0f;
                        const float targetMs = baseDelayMs + mod * depthMs * sideSign;
                        const float ms = ch.delaySmooth.next (juce::jlimit (0.5f, 55.0f, targetMs));

                        wet = readInterpolated (ch, ms * 0.001f * (float) sampleRate);

                        if (p.mode == Chorus && pct (p.detune) > 0.001f)
                        {
                            // a second voice a few cents away thickens the chorus
                            wet = wet * 0.6f + readPitchVoice (ch, slideUp, false) * 0.4f;
                        }
                    }

                    if (++ch.writePos >= maxDelaySamples)
                        ch.writePos = 0;

                    d[i] = dry + (wet - dry) * mixAmt;
                }
            }
        }

    private:
        struct Channel
        {
            std::vector<float> line;
            int   writePos { 0 };
            Lfo   lfo, drift;
            Smooth delaySmooth;
            juce::dsp::IIR::Filter<float> hp;
            juce::Random random;
            double phase { 0.0 };
            float randomWalk { 0.0f };
            float tapA { 0.0f }, tapB { 0.5f };       // pitch voice phases, 0 .. 1
        };

        /** Third-order-free linear read; the line is oversampled relative to the
            modulation rates in use, so linear interpolation is inaudible here and
            costs a fraction of a Lagrange read per sample. */
        float readInterpolated (Channel& ch, float delaySamples) const noexcept
        {
            delaySamples = juce::jlimit (1.0f, (float) maxDelaySamples - 2.0f, delaySamples);

            float pos = (float) ch.writePos - delaySamples;
            while (pos < 0.0f) pos += (float) maxDelaySamples;

            const int i0 = (int) pos;
            const float frac = pos - (float) i0;
            const int i1 = (i0 + 1) % maxDelaySamples;

            return ch.line[(size_t) i0] + frac * (ch.line[(size_t) i1] - ch.line[(size_t) i0]);
        }

        /** One pitch-shift voice built from two crossfaded taps.

            The read position slides at (1 - ratio) samples per sample, which is
            exactly a constant pitch offset, and is wrapped with a raised-cosine
            pair whose gains sum to one at every point, so the wrap is inaudible.
        */
        float readPitchVoice (Channel& ch, float ratioMinusOne, bool second) noexcept
        {
            float& phase = second ? ch.tapB : ch.tapA;
            const float window = crossfadeSamples;

            phase -= ratioMinusOne / window;
            phase -= std::floor (phase);                       // wrap into 0 .. 1

            const float p1 = phase;
            const float p2 = p1 >= 0.5f ? p1 - 0.5f : p1 + 0.5f;

            const float base = baseDelayMs * 0.001f * (float) sampleRate;
            const float g1 = 0.5f - 0.5f * std::cos (p1 * juce::MathConstants<float>::twoPi);
            const float g2 = 0.5f - 0.5f * std::cos (p2 * juce::MathConstants<float>::twoPi);

            return readInterpolated (ch, base + p1 * window) * g1
                 + readInterpolated (ch, base + p2 * window) * g2;
        }

        std::array<Channel, 2> chans;
        Params p;
        double sampleRate { 44100.0 };
        int    maxDelaySamples { 4096 };
        float  lfoHz { 1.0f }, baseDelayMs { 14.0f }, depthMs { 3.0f };
        float  centsUp { 0.0f }, centsDown { 0.0f }, slideUp { 0.0f }, slideDown { 0.0f };
        float  widthAmt { 0.5f }, motionAmt { 0.25f }, mixAmt { 0.35f };
        float  crossfadeSamples { 1024.0f };
    };
}
