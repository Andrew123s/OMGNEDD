#pragma once
#include "Utils.h"

namespace omg::dsp
{
    /** Feed-forward compressor with six detector characters, a filtered
        sidechain, a soft knee, auto timing and a parallel blend.
    */
    class CompressorSection
    {
    public:
        enum Mode { Clean = 0, Vocal, Punch, Smooth, Aggressive, Opto };

        struct Params
        {
            float threshold = -18, ratio = 3, attack = 12, release = 140;
            float knee = 6, makeup = 0, mix = 100, scHpf = 90;
            int   mode = Vocal;
            bool  autoMode = false, punch = false, enabled = true;
        };

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            for (auto& ch : chans)
            {
                ch.sc.prepare (spec);
                ch.env.prepare (spec.sampleRate);
                ch.rms = 0.0f;
            }
            reduction.store (0.0f);
        }

        void reset()
        {
            for (auto& ch : chans) { ch.sc.reset(); ch.env.env = 0.0f; ch.rms = 0.0f; }
            reduction.store (0.0f);
        }

        void setParams (const Params& np)
        {
            p = np;

            float atk = p.attack, rel = p.release, knee = p.knee;

            switch (p.mode)
            {
                case Vocal:      atk *= 0.8f;  rel *= 1.1f;  knee = juce::jmax (knee, 6.0f);  break;
                case Punch:      atk *= 2.4f;  rel *= 0.55f; knee *= 0.4f;                    break;
                case Smooth:     atk *= 1.6f;  rel *= 1.8f;  knee = juce::jmax (knee, 12.0f); break;
                case Aggressive: atk *= 0.25f; rel *= 0.45f; knee *= 0.2f;                    break;
                case Opto:       atk *= 3.0f;  rel *= 2.6f;  knee = juce::jmax (knee, 10.0f); break;
                case Clean:
                default: break;
            }

            if (p.punch)   { atk *= 1.8f; rel *= 0.7f; }
            if (p.autoMode) { atk = 18.0f; rel = 220.0f; }

            effectiveKnee = juce::jlimit (0.0f, 24.0f, knee);
            useRms = (p.mode == Smooth || p.mode == Opto || p.mode == Clean);

            for (auto& ch : chans)
            {
                ch.env.setTimes (juce::jlimit (0.1f, 400.0f, atk), juce::jlimit (5.0f, 2000.0f, rel));
                ch.sc.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, juce::jlimit (20.0f, 400.0f, p.scHpf), 0.707f);
            }

            rmsCoef = (float) std::exp (-1.0 / (0.012 * sampleRate));
        }

        void process (juce::AudioBuffer<float>& buffer)
        {
            if (! p.enabled) { reduction.store (0.0f); return; }

            const int numCh = juce::jmin (2, buffer.getNumChannels());
            const int n = buffer.getNumSamples();
            const float wet = pct (p.mix);
            const float ratio = juce::jmax (1.0f, p.ratio);
            float worstGr = 0.0f;

            // one detector for the pair, so stereo image is not pulled apart
            for (int i = 0; i < n; ++i)
            {
                float key = 0.0f;
                for (int c = 0; c < numCh; ++c)
                {
                    auto& ch = chans[(size_t) c];
                    const float filtered = ch.sc.processSample (buffer.getSample (c, i));

                    if (useRms)
                    {
                        ch.rms = filtered * filtered + rmsCoef * (ch.rms - filtered * filtered);
                        key = juce::jmax (key, std::sqrt (juce::jmax (0.0f, ch.rms)));
                    }
                    else
                    {
                        key = juce::jmax (key, std::abs (filtered));
                    }
                }

                const float envLin = chans[0].env.process (key);
                const float envDb = gainToDb (envLin);

                float over = envDb - p.threshold;
                float grDb = 0.0f;

                if (effectiveKnee > 0.0f && over > -effectiveKnee * 0.5f && over < effectiveKnee * 0.5f)
                {
                    const float t = over + effectiveKnee * 0.5f;
                    grDb = -(1.0f - 1.0f / ratio) * (t * t) / (2.0f * effectiveKnee);
                }
                else if (over > 0.0f)
                {
                    grDb = -over * (1.0f - 1.0f / ratio);
                }

                worstGr = juce::jmax (worstGr, -grDb);

                float makeup = p.makeup;
                if (p.autoMode)
                    makeup = juce::jlimit (-12.0f, 24.0f, -p.threshold * (1.0f - 1.0f / ratio) * 0.6f);

                const float g = dbToGain (grDb + makeup);

                for (int c = 0; c < numCh; ++c)
                {
                    const float dry = buffer.getSample (c, i);
                    buffer.setSample (c, i, dry + (dry * g - dry) * wet);
                }
            }

            reduction.store (worstGr);
        }

        float getReductionDb() const { return reduction.load(); }

    private:
        struct Channel
        {
            juce::dsp::IIR::Filter<float> sc;
            EnvFollower env;
            float rms { 0.0f };
        };

        std::array<Channel, 2> chans;
        Params p;
        double sampleRate { 44100.0 };
        float effectiveKnee { 6.0f }, rmsCoef { 0.99f };
        bool useRms { false };
        std::atomic<float> reduction { 0.0f };
    };
}
