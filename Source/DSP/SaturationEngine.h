#pragma once
#include "Utils.h"
#include "Filters.h"

namespace omg::dsp
{
    /** Six analogue-style saturation models. Runs inside the oversampler.

        in -> WARMTH (low lift, soft top) -> THICKNESS (low-mid weight)
           -> DENSITY (upward levelling) -> drive -> model curve
           -> HARMONICS (even-order blend) -> transformer high pass
           -> TONE tilt -> SOFT CLIP -> auto gain -> mix

        Every model does more than change the curve: TAPE rounds the top as it
        is driven, TUBE leans on the second harmonic, CONSOLE adds a little
        presence, TRANSFORMER saturates the low end hardest, WARM darkens, and
        CLEAN is level and glue with almost no colour. The difference between
        them is meant to be heard on a vocal, not measured.
    */
    class SaturationEngine
    {
    public:
        enum Model { Tape = 0, Tube, Console, Transformer, Warm, Clean };

        struct Params
        {
            float drive = 35, warmth = 45, harmonics = 40, thickness = 35;
            float tone = 0, softClip = 30, density = 40, mix = 100;
            int   model = Tape;
            bool  chaos = false;
        };

        void prepare (double maxRate, int /*maxBlock*/) { setSampleRate (maxRate); }

        void setSampleRate (double fs)
        {
            sampleRate = fs;
            for (auto& ch : chans)
            {
                ch.densityEnv.prepare (fs);
                ch.densityEnv.setTimes (6.0f, 140.0f);
                ch.autoGain.prepare (fs, 0.35, 12.0f, 18.0f);
                ch.chaosSmooth.setTime (fs, 0.08);
            }
            reset();
            setParams (p);
        }

        void reset()
        {
            for (auto& ch : chans)
            {
                for (auto* b : { &ch.warmLow, &ch.warmTop, &ch.thick, &ch.transformer, &ch.toneLow,
                                 &ch.toneHigh, &ch.modelTone, &ch.lowSplit })
                    b->reset();
                ch.densityEnv.env = 0.0f;
                ch.autoGain.reset();
            }
            for (size_t c = 0; c < chans.size(); ++c) chans[c].random.setSeed ((juce::int64) (0x5A7 + c));
        }

        void setParams (const Params& np)
        {
            p = np;
            const double fs = sampleRate;

            driveGain  = dbToGain (pct (p.drive) * 30.0f);
            harm       = pct (p.harmonics);
            thick      = pct (p.thickness);
            clipAmt    = pct (p.softClip);
            densityAmt = pct (p.density);
            mixAmt     = pct (p.mix);

            const double warm = pct (p.warmth);
            const double tilt = bipolar (p.tone) * 8.0;

            // each model's own tone, so the choice is audible before the curve does anything
            BiquadCoeffs modelTone;
            switch (p.model)
            {
                case Tape:        modelTone = BiquadCoeffs::highShelf (fs, 9000.0, 0.6, -2.0 - pct (p.drive) * 5.0); break;
                case Tube:        modelTone = BiquadCoeffs::peak (fs, 1400.0, 0.6, 1.5); break;
                case Console:     modelTone = BiquadCoeffs::peak (fs, 4500.0, 0.8, 2.5); break;
                case Transformer: modelTone = BiquadCoeffs::lowShelf (fs, 110.0, 0.7, 3.0); break;
                case Warm:        modelTone = BiquadCoeffs::highShelf (fs, 3500.0, 0.6, -5.0); break;
                case Clean:
                default:          modelTone = BiquadCoeffs::identity(); break;
            }

            for (auto& ch : chans)
            {
                ch.warmLow.set (BiquadCoeffs::lowShelf (fs, 220.0, 0.7, warm * 8.0));
                ch.warmTop.set (BiquadCoeffs::highShelf (fs, 6000.0, 0.7, -warm * 5.0));
                ch.thick.set (BiquadCoeffs::peak (fs, 350.0, 0.8, thick * 7.0));
                ch.transformer.set (BiquadCoeffs::highPass (fs, 25.0 + thick * 35.0, 0.8));
                ch.toneLow.set  (BiquadCoeffs::lowShelf  (fs, 450.0, 0.7, -tilt));
                ch.toneHigh.set (BiquadCoeffs::highShelf (fs, 3800.0, 0.7,  tilt));
                ch.modelTone.set (modelTone);
                ch.lowSplit.set (BiquadCoeffs::lowPass (fs, 180.0, 0.707));
            }
        }

        float curve (float x, float lowBand) const
        {
            switch (p.model)
            {
                case Tube:        return x >= 0.0f ? std::tanh (x * 1.2f) : std::tanh (x * 0.75f) * 0.88f;
                case Console:     return x / (1.0f + std::abs (x) * 0.85f);
                case Transformer: { const float lowHeat = std::tanh (lowBand * 2.5f) - lowBand;   // low end saturates hardest
                                    const float t = std::tanh (x);
                                    return t + 0.12f * t * t + lowHeat * 0.8f; }
                case Warm:        return std::atan (x * 1.3f) * (2.0f / juce::MathConstants<float>::pi);
                case Clean:       { const float c = juce::jlimit (-1.5f, 1.5f, x); return c - c * c * c / 6.75f; }
                case Tape:
                default:          { const float t = std::tanh (x * 1.3f); return t - 0.2f * t * t * t; }
            }
        }

        void process (juce::AudioBuffer<float>& buffer)
        {
            const int numCh = juce::jmin (2, buffer.getNumChannels());
            const int n = buffer.getNumSamples();

            for (int c = 0; c < numCh; ++c)
            {
                auto& ch = chans[(size_t) c];
                auto* d = buffer.getWritePointer (c);

                for (int i = 0; i < n; ++i)
                {
                    const float dry = d[i];
                    float x = ch.thick.process (ch.warmTop.process (ch.warmLow.process (dry)));

                    // density: quiet passages are lifted up to 12 dB into the curve
                    if (densityAmt > 0.0001f)
                    {
                        const float envDb = gainToDb (ch.densityEnv.process (std::abs (x)) + 1.0e-7f);
                        const float lift = juce::jlimit (0.0f, 12.0f, (-12.0f - envDb) * 0.6f) * densityAmt;
                        x *= dbToGain (lift);
                    }

                    float g = driveGain;
                    if (p.chaos)
                        g *= 1.0f + ch.chaosSmooth.process ((ch.random.nextFloat() - 0.5f) * 0.5f);

                    const float low = ch.lowSplit.process (x * g);
                    float y = curve (x * g, low);

                    // harmonics: an even-order term, strongest where the curve is working
                    if (harm > 0.0001f)
                        y += harm * 0.45f * (y * y - 0.25f) * (1.0f + thick * 0.5f);

                    y = ch.transformer.process (y);
                    y = ch.modelTone.process (y);
                    y = ch.toneHigh.process (ch.toneLow.process (y));

                    if (clipAmt > 0.0001f)
                    {
                        const float k = 1.0f + clipAmt * 4.0f;
                        y = lerp (y, std::tanh (y * k) / std::tanh (k), clipAmt);
                    }

                    y = ch.autoGain.process (dry, y);
                    d[i] = dry + (y - dry) * mixAmt;
                }
            }
        }

    private:
        struct Channel
        {
            Biquad warmLow, warmTop, thick, transformer, toneLow, toneHigh, modelTone, lowSplit;
            EnvFollower densityEnv;
            AutoGain autoGain;
            OnePole chaosSmooth;
            juce::Random random;
        };

        std::array<Channel, 2> chans;
        Params p;
        double sampleRate { 96000.0 };
        float driveGain { 1.0f }, harm { 0.0f }, thick { 0.0f }, clipAmt { 0.0f }, densityAmt { 0.0f }, mixAmt { 1.0f };
    };
}
