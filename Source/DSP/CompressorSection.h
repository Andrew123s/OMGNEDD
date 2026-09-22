#pragma once
#include "Utils.h"
#include "Filters.h"

namespace omg::dsp
{
    /** Feed-forward compressor with six timing characters, selectable peak or
        RMS detection, a filtered internal or external sidechain, a soft knee,
        auto gain, programme-dependent auto release and a parallel blend.
    */
    class CompressorSection
    {
    public:
        enum Mode { Clean = 0, Vocal, Punch, Smooth, Aggressive, Opto };

        enum Detector { Peak = 0, Rms = 1 };

        struct Params
        {
            float threshold = -18, ratio = 3, attack = 12, release = 140;
            float knee = 6, makeup = 0, mix = 100, scHpf = 90, scAmount = 100;
            int   mode = Vocal;
            int   detector = Peak;
            bool  autoMode = false, autoRelease = false, punch = false, enabled = true;
            bool  externalSidechain = false;
        };

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            for (auto& ch : chans)
            {
                ch.sc.reset();
                ch.env.prepare (spec.sampleRate);
                ch.rms = 0.0f;
            }
            reduction.store (0.0f);
        }

        void reset()
        {
            for (auto& ch : chans) { ch.sc.reset(); ch.env.env = 0.0f; ch.rms = 0.0f; }
            autoFast = autoSlow = 0.0f;
            appliedReleaseMs = baseReleaseMs;
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
            // OPTO and SMOOTH are RMS by nature; elsewhere DETECTION decides
            useRms = p.detector == Rms || p.mode == Opto || p.mode == Smooth;

            // AGGRESSIVE compresses harder and drives its own output into a
            // soft clip: the slammed sound, not just faster timing
            ratioScale = p.mode == Aggressive ? 1.8f : p.mode == Opto ? 0.8f : 1.0f;
            slam = p.mode == Aggressive ? 1.6f : 0.0f;

            baseReleaseMs = juce::jlimit (5.0f, 2000.0f, rel);
            attackMs      = juce::jlimit (0.1f, 400.0f, atk);

            for (auto& ch : chans)
            {
                ch.env.setTimes (attackMs, baseReleaseMs);
                ch.sc.set (BiquadCoeffs::highPass (sampleRate, juce::jlimit (20.0f, 400.0f, p.scHpf), 0.707));
            }

            rmsCoef = (float) std::exp (-1.0 / (0.012 * sampleRate));
            // the two envelopes AUTO RELEASE crossfades between
            autoFastCoef = (float) std::exp (-1.0 / (0.040 * sampleRate));
            autoSlowCoef = (float) std::exp (-1.0 / (0.600 * sampleRate));
        }

        /** @param sidechain  the external sidechain bus, or nullptr when the
                               host has not connected one. */
        void process (juce::AudioBuffer<float>& buffer, const juce::AudioBuffer<float>* sidechain = nullptr)
        {
            if (! p.enabled) { reduction.store (0.0f); return; }

            const int numCh = juce::jmin (2, buffer.getNumChannels());
            const int n = buffer.getNumSamples();
            const float wet = pct (p.mix);
            const float ratio = juce::jmax (1.0f, p.ratio * ratioScale);
            float worstGr = 0.0f;

            const bool useExternal = p.externalSidechain
                                   && sidechain != nullptr
                                   && sidechain->getNumChannels() > 0
                                   && sidechain->getNumSamples() >= n;
            const float extBlend = useExternal ? pct (p.scAmount) : 0.0f;

            // one detector for the pair, so stereo image is not pulled apart
            for (int i = 0; i < n; ++i)
            {
                float key = 0.0f;
                for (int c = 0; c < numCh; ++c)
                {
                    auto& ch = chans[(size_t) c];

                    float source = buffer.getSample (c, i);
                    if (extBlend > 0.0f)
                    {
                        const int sc = juce::jmin (c, sidechain->getNumChannels() - 1);
                        source = source + (sidechain->getSample (sc, i) - source) * extBlend;
                    }

                    const float filtered = ch.sc.process (source);

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

                if (p.autoRelease)
                {
                    // two envelopes on the same key; how far apart they are says
                    // whether the material is transient or sustained, and the
                    // release time is set from that every sample
                    autoFast = key + autoFastCoef * (autoFast - key);
                    autoSlow = key + autoSlowCoef * (autoSlow - key);

                    const float spread = juce::jlimit (0.0f, 1.0f,
                                                       (autoFast - autoSlow) / juce::jmax (1.0e-5f, autoFast + 1.0e-5f));
                    const float ms = juce::jlimit (30.0f, 900.0f, lerp (520.0f, 70.0f, spread));

                    if (std::abs (ms - appliedReleaseMs) > 2.0f)
                    {
                        appliedReleaseMs = ms;
                        for (auto& ch : chans)
                            ch.env.setTimes (attackMs, appliedReleaseMs);
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
                    float y = dry * g;
                    if (slam > 0.0f)
                        y = std::tanh (y * slam) / slam * 1.15f;
                    buffer.setSample (c, i, dry + (y - dry) * wet);
                }
            }

            reduction.store (worstGr);
        }

        float getReductionDb() const { return reduction.load(); }

    private:
        struct Channel
        {
            Biquad sc;
            EnvFollower env;
            float rms { 0.0f };
        };

        std::array<Channel, 2> chans;
        Params p;
        double sampleRate { 44100.0 };
        float effectiveKnee { 6.0f }, rmsCoef { 0.99f };
        float attackMs { 12.0f }, baseReleaseMs { 140.0f }, appliedReleaseMs { 140.0f };
        float autoFast { 0.0f }, autoSlow { 0.0f }, autoFastCoef { 0.99f }, autoSlowCoef { 0.999f };
        bool useRms { false };
        float ratioScale { 1.0f }, slam { 0.0f };
        std::atomic<float> reduction { 0.0f };
    };
}
