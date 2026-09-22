#pragma once
#include "Utils.h"
#include "Filters.h"
#include "../Parameters.h"

namespace omg::dsp
{
    /** Seven band equaliser.

        Every band can be any of six shapes. High and low pass bands can be
        12, 24, 36 or 48 dB per octave, built as Butterworth cascades, with the
        band's Q shaping the resonance of the sharpest section.

        DYNAMIC makes a band's gain follow the energy in its own region: a
        band set to cut 6 dB at 350 Hz only cuts while the vocal is actually
        pushing 350 Hz, which is what mud, harshness and sibilance control
        need. The band's gain is recomputed every 16 samples from its own
        detector, so it is a real dynamic EQ rather than a crossfade between a
        filtered and an unfiltered signal, which comb-filters.

        Coefficients are designed in place: no allocation on the audio thread.
        The editor draws the curve from an atomic snapshot of the band
        settings, never from the audio thread's own state.
    */
    class EqSection
    {
    public:
        struct Band
        {
            float freq = 1000.0f, gain = 0.0f, q = 1.0f;
            int   type = EqBell;
            bool  on = true, dynamic = false;
            int   slope = 0;                     // 0..3 -> 12..48 dB, pass types only
        };

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;
            for (auto& ch : chans)
                for (int b = 0; b < kNumEqBands; ++b)
                {
                    ch.env[(size_t) b].prepare (spec.sampleRate);
                    ch.env[(size_t) b].setTimes (4.0f, 90.0f);
                }
            for (auto& g : dynGain) g.setTime (spec.sampleRate, 0.01);
            reset();
            updateCoefficients();
        }

        void reset()
        {
            for (auto& ch : chans)
                for (int b = 0; b < kNumEqBands; ++b)
                {
                    ch.filters[(size_t) b].reset();
                    ch.detectors[(size_t) b].reset();
                    ch.env[(size_t) b].env = 0.0f;
                }
            for (auto& g : dynGain) g.reset (0.0f);
            counter = 0;
        }

        void setBand (int index, const Band& b)
        {
            if (juce::isPositiveAndBelow (index, kNumEqBands))
                bands[(size_t) index] = b;
        }

        void setEnabled (bool shouldBeOn) { enabled = shouldBeOn; snapshotEnabled.store (shouldBeOn); }

        static bool isPass (int type)  { return type == EqHighPass || type == EqLowPass; }
        static bool isDynamicCapable (int type) { return type == EqBell || type == EqLowShelf || type == EqHighShelf; }

        void updateCoefficients()
        {
            for (int b = 0; b < kNumEqBands; ++b)
            {
                const auto& band = bands[(size_t) b];
                for (auto& ch : chans)
                {
                    design (ch.filters[(size_t) b], band, band.gain);
                    ch.detectors[(size_t) b].set (BiquadCoeffs::bandPass (sampleRate, clampF (band.freq), juce::jmax (0.5f, band.q)));
                }

                auto& s = snapshot[(size_t) b];
                s.freq.store (band.freq); s.gain.store (band.gain); s.q.store (band.q);
                s.type.store (band.type); s.on.store (band.on); s.slope.store (band.slope);
            }
        }

        /** Magnitude of the whole section at one frequency, for drawing the
            curve. Safe to call from the message thread. */
        float getResponseDb (float hz) const
        {
            if (! snapshotEnabled.load()) return 0.0f;

            double total = 0.0;
            for (int b = 0; b < kNumEqBands; ++b)
            {
                const auto& s = snapshot[(size_t) b];
                if (! s.on.load()) continue;

                Band band;
                band.freq = s.freq.load(); band.gain = s.gain.load(); band.q = s.q.load();
                band.type = s.type.load(); band.slope = s.slope.load();

                BandFilter f;
                design (f, band, band.gain);
                total += f.magnitudeDb (hz, sampleRate);
            }
            return (float) total;
        }

        void process (juce::AudioBuffer<float>& buffer)
        {
            if (! enabled) return;

            const int numCh = juce::jmin (2, buffer.getNumChannels());
            const int n = buffer.getNumSamples();

            for (int i = 0; i < n; ++i)
            {
                // dynamic bands: recompute the gain at control rate from the
                // louder of the two channels' detectors
                if (++counter >= 16)
                {
                    counter = 0;
                    for (int b = 0; b < kNumEqBands; ++b)
                    {
                        const auto& band = bands[(size_t) b];
                        if (! (band.on && band.dynamic && isDynamicCapable (band.type))) continue;

                        float e = 0.0f;
                        for (int c = 0; c < numCh; ++c) e = juce::jmax (e, chans[(size_t) c].env[(size_t) b].env);
                        const float amount = juce::jlimit (0.0f, 1.0f, (gainToDb (e + 1.0e-7f) + 42.0f) / 20.0f);
                        const float g = dynGain[(size_t) b].process (band.gain * amount);
                        for (int c = 0; c < numCh; ++c)
                            design (chans[(size_t) c].filters[(size_t) b], band, g);
                    }
                }

                for (int c = 0; c < numCh; ++c)
                {
                    auto& ch = chans[(size_t) c];
                    auto* d = buffer.getWritePointer (c);
                    float x = d[i];

                    for (int b = 0; b < kNumEqBands; ++b)
                    {
                        const auto& band = bands[(size_t) b];
                        if (! band.on) continue;

                        if (band.dynamic && isDynamicCapable (band.type))
                            ch.env[(size_t) b].process (std::abs (ch.detectors[(size_t) b].process (x)));

                        x = ch.filters[(size_t) b].process (x);
                    }

                    d[i] = x;
                }
            }
        }

    private:
        /** One band's filter: a cascade for the pass shapes, a single
            biquad for everything else. */
        struct BandFilter
        {
            SlopeFilter cascade;
            Biquad single;
            bool useCascade { false };

            void reset() { cascade.reset(); single.reset(); }
            inline float process (float x) noexcept { return useCascade ? cascade.process (x) : single.process (x); }
            double magnitudeDb (double f, double fs) const
            {
                return useCascade ? cascade.magnitudeDb (f, fs) : single.c.magnitudeDb (f, fs);
            }
        };

        float clampF (float f) const { return juce::jlimit (20.0f, (float) sampleRate * 0.47f, f); }

        void design (BandFilter& f, const Band& band, float gainDb) const
        {
            const double fq = clampF (band.freq);
            const double q = juce::jlimit (0.1f, 18.0f, band.q);

            f.useCascade = isPass (band.type);
            switch (band.type)
            {
                case EqHighPass:  f.cascade.design (true,  sampleRate, fq, band.slope, q); break;
                case EqLowPass:   f.cascade.design (false, sampleRate, fq, band.slope, q); break;
                case EqLowShelf:  f.single.set (BiquadCoeffs::lowShelf  (sampleRate, fq, q, gainDb)); break;
                case EqHighShelf: f.single.set (BiquadCoeffs::highShelf (sampleRate, fq, q, gainDb)); break;
                case EqNotch:     f.single.set (BiquadCoeffs::notch     (sampleRate, fq, q)); break;
                case EqBell:
                default:          f.single.set (BiquadCoeffs::peak      (sampleRate, fq, q, gainDb)); break;
            }
        }

        struct Channel
        {
            std::array<BandFilter, kNumEqBands> filters;
            std::array<Biquad, kNumEqBands> detectors;
            std::array<EnvFollower, kNumEqBands> env;
        };

        struct Snapshot
        {
            std::atomic<float> freq { 1000.0f }, gain { 0.0f }, q { 1.0f };
            std::atomic<int> type { EqBell }, slope { 0 };
            std::atomic<bool> on { true };
        };

        std::array<Channel, 2> chans;
        std::array<Band, kNumEqBands> bands;
        std::array<Snapshot, kNumEqBands> snapshot;
        std::array<OnePole, kNumEqBands> dynGain;
        std::atomic<bool> snapshotEnabled { true };
        double sampleRate { 44100.0 };
        bool enabled { true };
        int counter { 0 };
    };
}
