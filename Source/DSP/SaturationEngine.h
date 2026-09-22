#pragma once
#include "Utils.h"

namespace omg::dsp
{
    /** Six saturation models, each a different mix of curve, pre-density and
        tone shaping, plus an independent soft clipper on the way out.
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

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            for (auto& ch : chans)
            {
                ch.warmFilter.prepare (spec);
                ch.toneLow.prepare (spec);
                ch.toneHigh.prepare (spec);
                ch.transformer.prepare (spec);
                ch.env.prepare (spec.sampleRate);
                ch.env.setTimes (8.0f, 120.0f);
            }
        }

        void reset()
        {
            for (auto& ch : chans)
            {
                ch.warmFilter.reset(); ch.toneLow.reset(); ch.toneHigh.reset(); ch.transformer.reset();
            }
        }

        void setParams (const Params& np)
        {
            p = np;
            driveGain = dbToGain (pct (p.drive) * 26.0f);
            harm      = pct (p.harmonics);
            thick     = pct (p.thickness);
            clipAmt   = pct (p.softClip);
            densityAmt = pct (p.density);
            mixAmt    = pct (p.mix);

            const float warmDb = pct (p.warmth) * 6.0f;
            const float tilt   = bipolar (p.tone) * 7.0f;

            for (auto& ch : chans)
            {
                ch.warmFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sampleRate, 260.0f, 0.7f, dbToGain (warmDb));
                ch.toneLow.coefficients    = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sampleRate, 500.0f, 0.7f, dbToGain (-tilt));
                ch.toneHigh.coefficients   = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 4200.0f, 0.7f, dbToGain (tilt));
                ch.transformer.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, 22.0f + thick * 30.0f, 0.9f);
            }
        }

        float curve (float x) const
        {
            switch (p.model)
            {
                case Tube:        return x >= 0.0f ? std::tanh (x * 1.15f) : std::tanh (x * 0.8f) * 0.9f;
                case Console:     return x / (1.0f + std::abs (x) * 0.85f);
                case Transformer: { const float t = std::tanh (x); return t + 0.12f * t * t - 0.08f * t * t * t; }
                case Warm:        return std::atan (x * 1.25f) * (2.0f / juce::MathConstants<float>::pi);
                case Clean:       // third order only, and only near the top of the
                                  // range: level and glue with almost no colour
                                  return juce::jlimit (-1.4f, 1.4f, x - (1.0f / 9.0f) * x * x * x);
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
                    float x = ch.warmFilter.processSample (dry);

                    // density: gentle upward levelling before the curve
                    if (densityAmt > 0.0001f)
                    {
                        const float e = ch.env.process (std::abs (x));
                        const float boost = dbToGain (juce::jlimit (0.0f, 12.0f, (-24.0f - gainToDb (e + 1.0e-6f)) * -0.35f) * densityAmt * -1.0f);
                        x *= juce::jlimit (0.25f, 4.0f, boost);
                    }

                    float g = driveGain;
                    if (p.chaos)
                        g *= 1.0f + (ch.chaosRandom.nextFloat() - 0.5f) * 0.05f;

                    float y = curve (x * g);

                    // harmonics: blend a squared term for even order content
                    if (harm > 0.0001f)
                        y = lerp (y, y + 0.35f * (y * y - 0.33f) * (y < 0.0f ? -1.0f : 1.0f), harm);

                    y = ch.transformer.processSample (y);
                    y = ch.toneHigh.processSample (ch.toneLow.processSample (y));

                    if (clipAmt > 0.0001f)
                    {
                        const float k = 1.0f + clipAmt * 3.0f;
                        y = lerp (y, std::tanh (y * k) / std::tanh (k), clipAmt);
                    }

                    y *= 1.0f / (1.0f + pct (p.drive) * 1.6f) * 1.5f;
                    y = lerp (y, y * (1.0f + thick * 0.35f), thick);

                    d[i] = dry + (y - dry) * mixAmt;
                }
            }
        }

    private:
        struct Channel
        {
            juce::dsp::IIR::Filter<float> warmFilter, toneLow, toneHigh, transformer;
            EnvFollower env;
            juce::Random chaosRandom;
        };

        std::array<Channel, 2> chans;
        Params p;
        double sampleRate { 44100.0 };
        float driveGain { 1.0f }, harm { 0.0f }, thick { 0.0f }, clipAmt { 0.0f }, densityAmt { 0.0f }, mixAmt { 1.0f };
    };
}
