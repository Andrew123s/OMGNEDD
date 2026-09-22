#pragma once
#include "Utils.h"
#include "../Parameters.h"

namespace omg::dsp
{
    /** Seven band equaliser. Every band can be any of six shapes, and the four
        bell bands can be made dynamic, in which case their gain is scaled by how
        far the band's own energy sits above a threshold derived from the gain.
    */
    class EqSection
    {
    public:
        struct Band
        {
            float freq = 1000.0f, gain = 0.0f, q = 1.0f;
            int   type = EqBell;
            bool  on = true, dynamic = false;
        };

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            for (auto& ch : chans)
                for (int b = 0; b < kNumEqBands; ++b)
                {
                    ch.filters[(size_t) b].prepare (spec);
                    ch.detectors[(size_t) b].prepare (spec);
                    ch.env[(size_t) b].prepare (spec.sampleRate);
                    ch.env[(size_t) b].setTimes (6.0f, 120.0f);
                }
            updateCoefficients (true);
        }

        void reset()
        {
            for (auto& ch : chans)
                for (int b = 0; b < kNumEqBands; ++b)
                {
                    ch.filters[(size_t) b].reset();
                    ch.detectors[(size_t) b].reset();
                }
        }

        void setBand (int index, const Band& b)
        {
            if (juce::isPositiveAndBelow (index, kNumEqBands))
                bands[(size_t) index] = b;
        }

        const Band& getBand (int index) const { return bands[(size_t) juce::jlimit (0, kNumEqBands - 1, index)]; }

        void setEnabled (bool shouldBeOn) { enabled = shouldBeOn; }

        void updateCoefficients (bool force = false)
        {
            juce::ignoreUnused (force);

            for (int b = 0; b < kNumEqBands; ++b)
            {
                const auto& band = bands[(size_t) b];
                const float f = juce::jlimit (20.0f, (float) sampleRate * 0.47f, band.freq);
                const float q = juce::jlimit (0.1f, 18.0f, band.q);
                const float g = dbToGain (band.gain);

                auto coeffs = makeCoefficients (band.type, f, q, g);

                for (auto& ch : chans)
                {
                    ch.filters[(size_t) b].coefficients = coeffs;

                    if (band.dynamic && isBell (band.type))
                        ch.detectors[(size_t) b].coefficients =
                            juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, f, q);
                }
            }
        }

        /** Magnitude of the whole section at one frequency, for drawing the curve. */
        float getResponseDb (float hz) const
        {
            if (! enabled) return 0.0f;

            double total = 1.0;
            for (int b = 0; b < kNumEqBands; ++b)
            {
                const auto& band = bands[(size_t) b];
                if (! band.on) continue;

                const float f = juce::jlimit (20.0f, (float) sampleRate * 0.47f, band.freq);
                auto c = makeCoefficients (band.type, f, juce::jlimit (0.1f, 18.0f, band.q), dbToGain (band.gain));
                if (c != nullptr)
                    total *= c->getMagnitudeForFrequency ((double) hz, sampleRate);
            }

            return gainToDb ((float) total);
        }

        void process (juce::AudioBuffer<float>& buffer)
        {
            if (! enabled) return;

            const int numCh = juce::jmin (2, buffer.getNumChannels());
            const int n = buffer.getNumSamples();

            for (int c = 0; c < numCh; ++c)
            {
                auto& ch = chans[(size_t) c];
                auto* d = buffer.getWritePointer (c);

                for (int i = 0; i < n; ++i)
                {
                    float x = d[i];

                    for (int b = 0; b < kNumEqBands; ++b)
                    {
                        const auto& band = bands[(size_t) b];
                        if (! band.on) continue;

                        if (band.dynamic && isBell (band.type))
                        {
                            const float e = ch.env[(size_t) b].process (std::abs (ch.detectors[(size_t) b].processSample (x)));
                            const float over = juce::jlimit (0.0f, 1.0f, (gainToDb (e) + 36.0f) / 30.0f);
                            const float wet = ch.filters[(size_t) b].processSample (x);
                            x = lerp (x, wet, over);
                        }
                        else
                        {
                            x = ch.filters[(size_t) b].processSample (x);
                        }
                    }

                    d[i] = x;
                }
            }
        }

        static bool isBell (int type) { return type == EqBell || type == EqNotch; }

    private:
        juce::dsp::IIR::Coefficients<float>::Ptr makeCoefficients (int type, float f, float q, float g) const
        {
            using C = juce::dsp::IIR::Coefficients<float>;
            switch (type)
            {
                case EqHighPass:  return C::makeHighPass  (sampleRate, f, q);
                case EqLowShelf:  return C::makeLowShelf   (sampleRate, f, q, g);
                case EqHighShelf: return C::makeHighShelf  (sampleRate, f, q, g);
                case EqLowPass:   return C::makeLowPass    (sampleRate, f, q);
                case EqNotch:     return C::makeNotch      (sampleRate, f, q);
                case EqBell:
                default:          return C::makePeakFilter (sampleRate, f, q, g);
            }
        }

        struct Channel
        {
            std::array<juce::dsp::IIR::Filter<float>, kNumEqBands> filters;
            std::array<juce::dsp::IIR::Filter<float>, kNumEqBands> detectors;
            std::array<EnvFollower, kNumEqBands> env;
        };

        std::array<Channel, 2> chans;
        std::array<Band, kNumEqBands> bands;
        double sampleRate { 44100.0 };
        bool enabled { true };
    };
}
