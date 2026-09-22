#pragma once
#include "Utils.h"
#include "Filters.h"
#include "Tempo.h"

namespace omg::dsp
{
    /** Wah, wobble, talk and phaser: the moving filters.

        AUTO WAH   An envelope follower on the vocal opens a resonant band pass
                   as the voice gets louder: the classic envelope filter.
        WOBBLE     A 24 dB resonant low pass swept by an LFO locked to the host
                   tempo, with drive in front of it. The wobble.
        LFO WAH    The wah band pass swept by the LFO instead of the voice.
        TALK       Three formant filters morphing through A E I O U, so the
                   vocal is pushed through a sequence of vowel shapes. The
                   talk-box, yoi-yoi effect.
        PHASER     Six first-order all passes swept by the LFO, with feedback,
                   mixed back against the input for moving notches.

        When SYNC is on and the host is playing, the LFO's phase is taken from
        the host's song position, so a 1/4 wobble lands on the beat every time
        rather than drifting against it. When the host is stopped it free-runs
        at the synced rate.
    */
    class FilterFx
    {
    public:
        enum Mode  { AutoWah = 0, Wobble, LfoWah, Talk, Phaser };
        enum Shape { Sine = 0, Triangle, SawUp, SawDown, Square, SampleHold };

        struct Params
        {
            bool  on = false;
            int   mode = Wobble;
            bool  sync = true;
            int   div = 4;                  // 1/4
            float rateHz = 2.0f;
            float freq = 700.0f;            // Hz: the bottom of the sweep
            float depth = 60.0f;            // %: sweep range, up to 5 octaves
            float reso = 55.0f;             // %
            float sens = 50.0f;             // %: envelope sensitivity, AUTO WAH
            int   shape = Sine;
            float drive = 0.0f;             // %
            float stereo = 0.0f;            // degrees between the channels
            float mix = 100.0f;             // %
        };

        void prepare (double fs, int /*maxBlock*/)
        {
            sampleRate = fs;
            for (auto& ch : chans)
            {
                ch.env.prepare (fs);
                ch.env.setTimes (2.0f, 160.0f);
                ch.cutSmooth.setTime (fs, 0.0025);
                ch.stepSmooth.setTime (fs, 0.004);
            }
            mixSmooth.setTime (fs, 0.03);
            reset();
        }

        void reset()
        {
            for (auto& ch : chans)
            {
                for (auto& s : ch.svf) s.reset();
                for (auto& s : ch.formant) s.reset();
                for (auto& a : ch.ap) a = 0.0;
                ch.phaserFb = 0.0f;
                ch.env.env = 0.0f;
                ch.cutSmooth.reset (float (std::log2 (700.0)));
                ch.stepSmooth.reset();
                ch.held = 0.0f; ch.lastPhase = 0.0;
            }
            freePhase = 0.0;
            mixSmooth.reset (0.0f);
            for (size_t c = 0; c < chans.size(); ++c) chans[c].random.setSeed ((juce::int64) (0xF17 + c));
        }

        void setParams (const Params& np, const TransportInfo& t)
        {
            p = np;
            transport = t;
            cycleSeconds = p.sync ? Divisions::seconds (p.div, t.bpm)
                                  : 1.0 / juce::jlimit (0.02, 30.0, (double) p.rateHz);
            depthOct = pct (p.depth) * 5.0;
            q = 0.6 + std::pow ((double) pct (p.reso), 1.5) * 14.0;
            driveGain = 1.0f + pct (p.drive) * 9.0f;
            sensGain = 0.8f + pct (p.sens) * pct (p.sens) * 40.0f;
            stereoTurns = juce::jlimit (0.0f, 180.0f, p.stereo) / 360.0;
        }

        bool isActive() const noexcept { return p.on || mixSmooth.z > 1.0e-4f; }

        void process (juce::AudioBuffer<float>& buffer)
        {
            const int numCh = juce::jmin (2, buffer.getNumChannels());
            const int n = buffer.getNumSamples();
            if (! isActive()) return;

            const double fs = sampleRate;
            const double baseLog = std::log2 (juce::jlimit (60.0, 8000.0, (double) p.freq));
            const float mixTarget = p.on ? pct (p.mix) : 0.0f;

            // phase for sample 0 of this block
            const bool locked = p.sync && transport.playing;
            const double beatsPerCycle = Divisions::beats (p.div);
            const double beatsPerSample = transport.bpm / 60.0 / fs;
            const double phaseInc = 1.0 / (cycleSeconds * fs);

            for (int i = 0; i < n; ++i)
            {
                double phase;
                if (locked)
                {
                    const double ppq = transport.ppqAtBlockStart + i * beatsPerSample;
                    phase = ppq / beatsPerCycle;
                }
                else
                {
                    freePhase += phaseInc;
                    if (freePhase >= 1.0) freePhase -= 1.0;
                    phase = freePhase;
                }

                const float m = mixSmooth.process (mixTarget);

                for (int c = 0; c < numCh; ++c)
                {
                    auto& ch = chans[(size_t) c];
                    auto* d = buffer.getWritePointer (c);
                    const float dry = d[i];

                    double ph = phase + (c == 1 ? stereoTurns : 0.0);
                    ph -= std::floor (ph);
                    const float lfo = ch.stepSmooth.process (lfoValue (ph, ch));   // 0..1

                    // the modulation position, 0..1
                    float pos = lfo;
                    if (p.mode == AutoWah)
                    {
                        const float e = ch.env.process (std::abs (dry)) * sensGain;
                        pos = juce::jlimit (0.0f, 1.0f, std::sqrt (e));
                    }

                    const double cutLog = ch.cutSmooth.process ((float) (baseLog + pos * depthOct));
                    const double cut = std::pow (2.0, cutLog);

                    float wet = dry;
                    switch (p.mode)
                    {
                        case AutoWah:
                        case LfoWah:
                        {
                            auto& f = ch.svf[0];
                            f.tune (fs, cut, q);
                            f.process (dry * driveGain);
                            wet = (float) f.bandNorm() * (1.2f / std::sqrt (driveGain));
                            break;
                        }

                        case Wobble:
                        {
                            const double g = Svf::gFor (fs, cut);
                            float x = std::tanh (dry * driveGain) / std::sqrt (driveGain);
                            ch.svf[0].tuneG (g, 0.54);
                            ch.svf[0].process (x);
                            ch.svf[1].tuneG (g, juce::jmax (0.7, q * 0.6));
                            ch.svf[1].process ((float) ch.svf[0].lp);
                            wet = (float) ch.svf[1].lp * 1.4f;
                            break;
                        }

                        case Talk:
                        {
                            // vowel position across A E I O U
                            const float v = juce::jlimit (0.0f, 3.999f, pos * 4.0f);
                            const int a = (int) v;
                            const float fr = v - (float) a;
                            const float shift = (float) (std::pow (2.0, baseLog) / 700.0);
                            float sum = 0.0f;
                            for (int k = 0; k < 3; ++k)
                            {
                                const float hz = (kVowels[a][k] + (kVowels[a + 1][k] - kVowels[a][k]) * fr) * shift;
                                auto& f = ch.formant[(size_t) k];
                                f.tune (fs, hz, 4.0 + q * 0.8);
                                f.process (dry * driveGain);
                                sum += (float) f.bandNorm() * kFormantGain[k];
                            }
                            wet = sum * (1.6f / std::sqrt (driveGain));
                            break;
                        }

                        case Phaser:
                        default:
                        {
                            // six one-pole all passes at the swept frequency
                            const double g = std::tan (kPi * juce::jlimit (20.0, fs * 0.45, cut * 0.5) / fs);
                            const double coef = (g - 1.0) / (g + 1.0);
                            double x = dry + ch.phaserFb * juce::jmin (0.85, q / 16.0);
                            for (auto& s : ch.ap)
                            {
                                const double y = coef * x + s;
                                s = x - coef * y;
                                x = y;
                            }
                            ch.phaserFb = (float) x;
                            wet = 0.5f * (dry + (float) x) * 1.3f;
                            break;
                        }
                    }

                    d[i] = dry + (wet - dry) * m;
                }
            }
        }

    private:
        struct Channel
        {
            std::array<Svf, 2> svf;
            std::array<Svf, 3> formant;
            std::array<double, 6> ap {};
            float phaserFb { 0.0f };
            EnvFollower env;
            OnePole cutSmooth, stepSmooth;
            juce::Random random;
            float held { 0.0f };
            double lastPhase { 0.0 };
        };

        float lfoValue (double ph, Channel& ch) const noexcept
        {
            switch (p.shape)
            {
                case Triangle:   return (float) (1.0 - std::abs (2.0 * ph - 1.0));
                case SawUp:      return (float) ph;
                case SawDown:    return (float) (1.0 - ph);
                case Square:     return ph < 0.5 ? 1.0f : 0.0f;
                case SampleHold:
                {
                    if (ph < ch.lastPhase) ch.held = ch.random.nextFloat();
                    ch.lastPhase = ph;
                    return ch.held;
                }
                case Sine:
                default:         return (float) (0.5 - 0.5 * std::cos (2.0 * kPi * ph));
            }
        }

        // formants in Hz for A E I O U
        static constexpr float kVowels[5][3] = {
            { 730.0f, 1090.0f, 2440.0f },   // A
            { 530.0f, 1840.0f, 2480.0f },   // E
            { 270.0f, 2290.0f, 3010.0f },   // I
            { 570.0f,  840.0f, 2410.0f },   // O
            { 300.0f,  870.0f, 2240.0f } }; // U
        static constexpr float kFormantGain[3] = { 1.0f, 0.7f, 0.45f };

        std::array<Channel, 2> chans;
        Params p;
        TransportInfo transport;
        double sampleRate { 48000.0 };
        double cycleSeconds { 0.5 }, depthOct { 3.0 }, q { 4.0 }, stereoTurns { 0.0 };
        double freePhase { 0.0 };
        float driveGain { 1.0f }, sensGain { 10.0f };
        OnePole mixSmooth;
    };
}
