#pragma once
#include "Utils.h"

namespace omg::dsp
{
    /** Frequency-dependent drive into the character engine.

        The signal is split with two Linkwitz-Riley crossovers into low, mid and
        high, each band is given its own gain, and the three are summed again
        before the engine runs. Because the engine's curve is level dependent,
        a band pushed 8 dB harder is distorted considerably more than one left
        alone - which is what LOW / MID / HIGH are for. After the engine the
        same split is applied in reverse, so the tonal balance the user set with
        the EQ comes back and only the amount of character differs per band.

        One engine instance still does the work, so this costs two crossover
        pairs rather than three times the nonlinear processing.

        Linkwitz-Riley is phase-coherent on recombination at the crossover, so
        with all three bands at 0 dB the pair is transparent apart from the
        all-pass phase response the crossover itself has.
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

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;

            for (auto& ch : chans)
            {
                for (auto* f : { &ch.lowSplit, &ch.highSplit, &ch.lowJoin, &ch.highJoin })
                {
                    f->prepare (spec);
                    f->setType (juce::dsp::LinkwitzRileyFilterType::allpass);
                }
            }

            setParams (p);
        }

        void reset()
        {
            for (auto& ch : chans)
                for (auto* f : { &ch.lowSplit, &ch.highSplit, &ch.lowJoin, &ch.highJoin })
                    f->reset();
        }

        void setParams (const Params& np)
        {
            p = np;

            const float lo = juce::jlimit (40.0f, 800.0f, p.crossLow);
            const float hi = juce::jlimit (900.0f, juce::jmin (12000.0f, (float) sampleRate * 0.4f), p.crossHigh);

            for (auto& ch : chans)
            {
                ch.lowSplit.setCutoffFrequency (lo);
                ch.lowJoin.setCutoffFrequency (lo);
                ch.highSplit.setCutoffFrequency (hi);
                ch.highJoin.setCutoffFrequency (hi);
            }

            gLow  = dbToGain (juce::jlimit (-12.0f, 12.0f, p.low));
            gMid  = dbToGain (juce::jlimit (-12.0f, 12.0f, p.mid));
            gHigh = dbToGain (juce::jlimit (-12.0f, 12.0f, p.high));
        }

        bool isActive() const noexcept
        {
            return p.enabled && (std::abs (p.low) > 0.05f || std::abs (p.mid) > 0.05f || std::abs (p.high) > 0.05f);
        }

        /** Splits, applies the three gains and sums back, so the engine that
            follows sees each band at a different level. */
        void applyPre (juce::AudioBuffer<float>& buffer)
        {
            if (! isActive()) return;
            split (buffer, /*forward*/ true);
        }

        /** The inverse gains, applied through the same crossover, so the tonal
            balance returns and only the character differs by band. */
        void applyPost (juce::AudioBuffer<float>& buffer)
        {
            if (! isActive()) return;
            split (buffer, /*forward*/ false);
        }

    private:
        void split (juce::AudioBuffer<float>& buffer, bool forward)
        {
            const int numCh = juce::jmin (2, buffer.getNumChannels());
            const int n = buffer.getNumSamples();

            const float a = forward ? gLow  : 1.0f / juce::jmax (1.0e-4f, gLow);
            const float b = forward ? gMid  : 1.0f / juce::jmax (1.0e-4f, gMid);
            const float c = forward ? gHigh : 1.0f / juce::jmax (1.0e-4f, gHigh);

            for (int ci = 0; ci < numCh; ++ci)
            {
                auto& ch = chans[(size_t) ci];
                auto& lowF  = forward ? ch.lowSplit  : ch.lowJoin;
                auto& highF = forward ? ch.highSplit : ch.highJoin;
                auto* d = buffer.getWritePointer (ci);

                for (int i = 0; i < n; ++i)
                {
                    float low = 0.0f, rest = 0.0f;
                    lowF.processSample (ci, d[i], low, rest);

                    float mid = 0.0f, high = 0.0f;
                    highF.processSample (ci, rest, mid, high);

                    d[i] = low * a + mid * b + high * c;
                }
            }
        }

        struct Channel
        {
            juce::dsp::LinkwitzRileyFilter<float> lowSplit, highSplit, lowJoin, highJoin;
        };

        std::array<Channel, 2> chans;
        Params p;
        double sampleRate { 44100.0 };
        float gLow { 1.0f }, gMid { 1.0f }, gHigh { 1.0f };
    };
}
