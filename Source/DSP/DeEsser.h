#pragma once
#include "Utils.h"

namespace omg::dsp
{
    /** Split-band de-esser. The sibilant band is separated, measured and pulled
        down; LISTEN monitors that band on its own so the frequency can be aimed.
    */
    class DeEsser
    {
    public:
        struct Params
        {
            float freq = 6500, threshold = -26, amount = 45, range = 8;
            float attack = 1, release = 60;
            bool  listen = false, enabled = true;
        };

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            for (auto& ch : chans)
            {
                ch.band.prepare (spec);
                ch.detect.prepare (spec);
                ch.env.prepare (spec.sampleRate);
            }
            reduction.store (0.0f);
        }

        void reset()
        {
            for (auto& ch : chans) { ch.band.reset(); ch.detect.reset(); ch.env.env = 0.0f; }
            reduction.store (0.0f);
        }

        void setParams (const Params& np)
        {
            p = np;
            const float f = juce::jlimit (1000.0f, (float) sampleRate * 0.45f, p.freq);
            const float q = 1.4f;

            for (auto& ch : chans)
            {
                ch.band.coefficients   = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, f, q);
                ch.detect.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, f * 0.8f, 0.707f);
                ch.env.setTimes (p.attack, p.release);
            }
        }

        void process (juce::AudioBuffer<float>& buffer)
        {
            if (! p.enabled) { reduction.store (0.0f); return; }

            const int numCh = juce::jmin (2, buffer.getNumChannels());
            const int n = buffer.getNumSamples();
            const float amount = pct (p.amount);
            float worst = 0.0f;

            for (int c = 0; c < numCh; ++c)
            {
                auto& ch = chans[(size_t) c];
                auto* d = buffer.getWritePointer (c);

                for (int i = 0; i < n; ++i)
                {
                    const float dry = d[i];
                    const float band = ch.band.processSample (dry);
                    const float key  = ch.detect.processSample (dry);
                    const float envDb = gainToDb (ch.env.process (std::abs (key)));

                    const float over = juce::jmax (0.0f, envDb - p.threshold);
                    const float grDb = -juce::jmin (p.range, over * amount * 1.6f);
                    worst = juce::jmax (worst, -grDb);

                    if (p.listen)
                    {
                        d[i] = band;
                    }
                    else
                    {
                        // subtract the part of the band we are reducing
                        d[i] = dry + band * (dbToGain (grDb) - 1.0f);
                    }
                }
            }

            reduction.store (worst);
        }

        float getReductionDb() const { return reduction.load(); }

    private:
        struct Channel
        {
            juce::dsp::IIR::Filter<float> band, detect;
            EnvFollower env;
        };

        std::array<Channel, 2> chans;
        Params p;
        double sampleRate { 44100.0 };
        std::atomic<float> reduction { 0.0f };
    };
}
