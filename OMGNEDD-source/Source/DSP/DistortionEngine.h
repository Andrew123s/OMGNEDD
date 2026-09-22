#pragma once
#include "Utils.h"

namespace omg::dsp
{
    /** Eight nonlinear algorithms with pre-emphasis, bias, asymmetry, bit and
        sample-rate reduction, and a post filter. Runs inside the oversampler.
    */
    class DistortionEngine
    {
    public:
        enum Algo { Soft = 0, Hard, Tube, Tape, Fuzz, Fold, Digital, Rectify };

        struct Params
        {
            float drive = 35, bite = 40, body = 50, crush = 0, edge = 30, smooth = 25, mix = 100;
            float bias = 0, asym = 0, preEmph = 20, postFilter = 16000;
            int   algo = Soft;
            bool  chaos = false;
        };

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            for (auto& ch : chans)
            {
                ch.pre.prepare (spec); ch.post.prepare (spec);
                ch.biteFilter.prepare (spec); ch.bodyFilter.prepare (spec);
                ch.smoothFilter.prepare (spec);
                ch.holdCounter = 0.0f; ch.held = 0.0f;
            }
        }

        void reset()
        {
            for (auto& ch : chans)
            {
                ch.pre.reset(); ch.post.reset(); ch.biteFilter.reset();
                ch.bodyFilter.reset(); ch.smoothFilter.reset();
            }
        }

        void setParams (const Params& np)
        {
            p = np;

            driveGain = dbToGain (pct (p.drive) * 34.0f);
            edgeAmt   = pct (p.edge);
            crushAmt  = pct (p.crush);
            mixAmt    = pct (p.mix);
            biasAmt   = bipolar (p.bias) * 0.45f;
            asymAmt   = bipolar (p.asym);

            // bit depth from 16 down to 3, decimation from 1x down to 12x
            bits      = juce::jlimit (3.0f, 16.0f, 16.0f - crushAmt * 13.0f);
            bitStep   = std::pow (2.0f, bits - 1.0f);
            decimate  = 1.0f + crushAmt * 11.0f;

            const float preDb = pct (p.preEmph) * 12.0f;
            const float biteDb = pct (p.bite) * 10.0f - 2.0f;
            const float bodyDb = pct (p.body) * 8.0f - 2.0f;
            const float smoothHz = juce::jlimit (900.0f, 20000.0f,
                                                 p.postFilter * (1.0f - pct (p.smooth) * 0.72f));

            for (auto& ch : chans)
            {
                ch.pre.coefficients  = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 2600.0f, 0.7f, dbToGain (preDb));
                ch.post.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 2600.0f, 0.7f, dbToGain (-preDb));
                ch.biteFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, 3100.0f, 1.1f, dbToGain (biteDb));
                ch.bodyFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sampleRate, 190.0f, 0.7f, dbToGain (bodyDb));
                ch.smoothFilter.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass (sampleRate, juce::jmin (smoothHz, (float) sampleRate * 0.45f), 0.707f);
            }
        }

        static float shape (int algo, float x, float edge)
        {
            switch (algo)
            {
                case Hard:    return juce::jlimit (-1.0f, 1.0f, x * (1.0f + edge * 2.0f));
                case Tube:    return x >= 0.0f ? std::tanh (x * (1.0f + edge))
                                               : std::tanh (x * (1.0f + edge) * 0.72f) * 0.86f;
                case Tape:    { const float t = std::tanh (x * 1.4f); return t - 0.16f * t * t * t; }
                case Fuzz:    { const float s = x < 0.0f ? -1.0f : 1.0f;
                                return s * (1.0f - std::exp (-std::abs (x) * (3.0f + edge * 7.0f))); }
                case Fold:    { float y = x * (1.0f + edge * 3.0f);
                                for (int i = 0; i < 4; ++i)
                                {
                                    if (y > 1.0f)  y = 2.0f - y;
                                    if (y < -1.0f) y = -2.0f - y;
                                }
                                return y; }
                case Digital: { const float q = juce::jmax (2.0f, 40.0f - edge * 36.0f);
                                return juce::jlimit (-1.0f, 1.0f, std::round (x * q) / q); }
                case Rectify: { const float r = std::abs (x); return juce::jlimit (-1.0f, 1.0f, r * 2.0f - 1.0f) * 0.8f + x * 0.2f; }
                case Soft:
                default:      return std::tanh (x * (1.0f + edge * 1.6f));
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
                    float x = ch.bodyFilter.processSample (ch.biteFilter.processSample (dry));
                    x = ch.pre.processSample (x);

                    float chaosOffset = 0.0f;
                    if (p.chaos)
                        chaosOffset = (ch.chaosRandom.nextFloat() - 0.5f) * 0.05f;

                    x = x * driveGain + biasAmt + chaosOffset;

                    if (std::abs (asymAmt) > 0.001f)
                        x = x >= 0.0f ? x * (1.0f + asymAmt * 0.6f) : x * (1.0f - asymAmt * 0.6f);

                    x = shape (p.algo, x, edgeAmt);
                    x -= biasAmt;

                    if (crushAmt > 0.0001f)
                    {
                        ch.holdCounter += 1.0f;
                        if (ch.holdCounter >= decimate)
                        {
                            ch.holdCounter -= decimate;
                            ch.held = std::round (x * bitStep) / bitStep;
                        }
                        x = lerp (x, ch.held, crushAmt);
                    }

                    x = ch.post.processSample (x);
                    x = ch.smoothFilter.processSample (x);

                    // makeup so drive does not double as a level control
                    x *= 1.0f / (1.0f + pct (p.drive) * 2.2f) * 1.9f;

                    d[i] = dry + (x - dry) * mixAmt;
                }
            }
        }

    private:
        struct Channel
        {
            juce::dsp::IIR::Filter<float> pre, post, biteFilter, bodyFilter, smoothFilter;
            float holdCounter { 0.0f }, held { 0.0f };
            juce::Random chaosRandom;
        };

        std::array<Channel, 2> chans;
        Params p;
        double sampleRate { 44100.0 };
        float driveGain { 1.0f }, edgeAmt { 0.0f }, crushAmt { 0.0f }, mixAmt { 1.0f };
        float biasAmt { 0.0f }, asymAmt { 0.0f }, bits { 16.0f }, bitStep { 32768.0f }, decimate { 1.0f };
    };
}
