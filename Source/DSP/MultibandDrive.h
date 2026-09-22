#pragma once
#include "Utils.h"
#include "Filters.h"

namespace omg::dsp
{
    /** Frequency-dependent drive into the character engine.

        Two 4th-order Linkwitz-Riley crossovers split low, mid and high. Each
        band gets its own gain, the three are summed, and the engine runs on
        the result: a band pushed 8 dB harder is distorted considerably more
        than one left alone. After the engine the same split applies the
        inverse gains, so the tonal balance returns and only the amount of
        character differs per band.

        The low band passes through an all pass at the upper crossover, which
        is what a Linkwitz-Riley tree needs for the three bands to sum flat.
        Runs inside the oversampler and is told the rate it runs at.
    */
    class MultibandDrive
    {
    public:
        struct Params
        {
            float low = 0, mid = 0, high = 0;          // dB, -12 .. +12
            float crossLow = 220, crossHigh = 2600;    // Hz
            bool  enabled = false;
        };

        void prepare (double maxRate, int /*maxBlock*/) { setSampleRate (maxRate); }

        void setSampleRate (double fs)
        {
            sampleRate = fs;
            reset();
            designed = false;
            setParams (p);
        }

        void reset()
        {
            for (auto& ch : chans)
                for (auto* s : { &ch.pre, &ch.post })
                {
                    s->x1.reset(); s->x2.reset(); s->lowAllPass.reset();
                }
        }

        void setParams (const Params& np)
        {
            const bool retune = ! designed || np.crossLow != p.crossLow || np.crossHigh != p.crossHigh;
            p = np;

            if (retune)
            {
                const double lo = juce::jlimit (40.0, 800.0, (double) p.crossLow);
                const double hi = juce::jlimit (900.0, juce::jmin (12000.0, sampleRate * 0.4), (double) p.crossHigh);
                for (auto& ch : chans)
                    for (auto* s : { &ch.pre, &ch.post })
                    {
                        s->x1.design (sampleRate, lo);
                        s->x2.design (sampleRate, hi);
                        s->lowAllPass.set (BiquadCoeffs::allPass (sampleRate, hi, 0.70710678));
                    }
                designed = true;
            }

            gLow  = dbToGain (juce::jlimit (-12.0f, 12.0f, p.low));
            gMid  = dbToGain (juce::jlimit (-12.0f, 12.0f, p.mid));
            gHigh = dbToGain (juce::jlimit (-12.0f, 12.0f, p.high));
        }

        bool isActive() const noexcept
        {
            return p.enabled && (std::abs (p.low) > 0.05f || std::abs (p.mid) > 0.05f || std::abs (p.high) > 0.05f);
        }

        void applyPre (juce::AudioBuffer<float>& b)  { if (isActive()) run (b, true); }
        void applyPost (juce::AudioBuffer<float>& b) { if (isActive()) run (b, false); }

    private:
        struct Split
        {
            Lr4Crossover x1, x2;
            Biquad lowAllPass;
        };

        void run (juce::AudioBuffer<float>& buffer, bool forward)
        {
            const int numCh = juce::jmin (2, buffer.getNumChannels());
            const int n = buffer.getNumSamples();

            const float a = forward ? gLow  : 1.0f / gLow;
            const float b = forward ? gMid  : 1.0f / gMid;
            const float c = forward ? gHigh : 1.0f / gHigh;

            for (int ci = 0; ci < numCh; ++ci)
            {
                auto& s = forward ? chans[(size_t) ci].pre : chans[(size_t) ci].post;
                auto* d = buffer.getWritePointer (ci);

                for (int i = 0; i < n; ++i)
                {
                    float low, rest, mid, high;
                    s.x1.process (d[i], low, rest);
                    s.x2.process (rest, mid, high);
                    low = s.lowAllPass.process (low);
                    d[i] = low * a + mid * b + high * c;
                }
            }
        }

        struct Channel { Split pre, post; };

        std::array<Channel, 2> chans;
        Params p;
        double sampleRate { 96000.0 };
        float gLow { 1.0f }, gMid { 1.0f }, gHigh { 1.0f };
        bool designed { false };
    };
}
