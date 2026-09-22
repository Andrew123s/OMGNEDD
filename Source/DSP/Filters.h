#pragma once
#include <cmath>
#include <array>
#include <algorithm>
#include <vector>

namespace omg::dsp
{
    /*  The filter primitives every module in the plugin is built from.

        Two properties matter more than anything else here, and both were
        missing from the JUCE filters the modules used before:

        1. Every coefficient is computed from the sample rate the filter is
           *actually running at*, passed in on every design call. A module
           inside the oversampler is told the oversampled rate; a module outside
           it is told the session rate. Nothing caches a rate from prepare().

        2. Designing a filter never allocates. Coefficients live inside the
           filter as plain doubles and are overwritten in place, so a module can
           redesign its filters every block, or every few samples for a sweep,
           on the audio thread.

        Coefficients and state are double precision. At 8x oversampling a
        30 Hz shelf has its poles very close to the unit circle, which is where
        single precision starts to add audible noise.
    */

    constexpr double kPi = 3.14159265358979323846;

    inline double clampFreq (double f, double fs) { return std::clamp (f, 5.0, fs * 0.49); }

    // -------------------------------------------------------------------------
    // Biquad: transposed direct form II, RBJ cookbook designs
    // -------------------------------------------------------------------------
    struct BiquadCoeffs
    {
        double b0 { 1.0 }, b1 { 0.0 }, b2 { 0.0 }, a1 { 0.0 }, a2 { 0.0 };

        static BiquadCoeffs identity() { return {}; }

        static BiquadCoeffs normalise (double b0, double b1, double b2, double a0, double a1, double a2)
        {
            const double inv = 1.0 / a0;
            return { b0 * inv, b1 * inv, b2 * inv, a1 * inv, a2 * inv };
        }

        static BiquadCoeffs lowPass (double fs, double f, double q)
        {
            const double w = 2.0 * kPi * clampFreq (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            return normalise ((1 - c) * 0.5, 1 - c, (1 - c) * 0.5, 1 + al, -2 * c, 1 - al);
        }

        static BiquadCoeffs highPass (double fs, double f, double q)
        {
            const double w = 2.0 * kPi * clampFreq (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            return normalise ((1 + c) * 0.5, -(1 + c), (1 + c) * 0.5, 1 + al, -2 * c, 1 - al);
        }

        /** Band pass with 0 dB gain at the centre, whatever the Q. */
        static BiquadCoeffs bandPass (double fs, double f, double q)
        {
            const double w = 2.0 * kPi * clampFreq (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            return normalise (al, 0, -al, 1 + al, -2 * c, 1 - al);
        }

        static BiquadCoeffs notch (double fs, double f, double q)
        {
            const double w = 2.0 * kPi * clampFreq (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            return normalise (1, -2 * c, 1, 1 + al, -2 * c, 1 - al);
        }

        static BiquadCoeffs allPass (double fs, double f, double q)
        {
            const double w = 2.0 * kPi * clampFreq (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            return normalise (1 - al, -2 * c, 1 + al, 1 + al, -2 * c, 1 - al);
        }

        static BiquadCoeffs peak (double fs, double f, double q, double gainDb)
        {
            const double A = std::pow (10.0, gainDb / 40.0);
            const double w = 2.0 * kPi * clampFreq (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            return normalise (1 + al * A, -2 * c, 1 - al * A, 1 + al / A, -2 * c, 1 - al / A);
        }

        static BiquadCoeffs lowShelf (double fs, double f, double q, double gainDb)
        {
            const double A = std::pow (10.0, gainDb / 40.0), sA = std::sqrt (A);
            const double w = 2.0 * kPi * clampFreq (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            return normalise (A * ((A + 1) - (A - 1) * c + 2 * sA * al),
                              2 * A * ((A - 1) - (A + 1) * c),
                              A * ((A + 1) - (A - 1) * c - 2 * sA * al),
                              (A + 1) + (A - 1) * c + 2 * sA * al,
                              -2 * ((A - 1) + (A + 1) * c),
                              (A + 1) + (A - 1) * c - 2 * sA * al);
        }

        static BiquadCoeffs highShelf (double fs, double f, double q, double gainDb)
        {
            const double A = std::pow (10.0, gainDb / 40.0), sA = std::sqrt (A);
            const double w = 2.0 * kPi * clampFreq (f, fs) / fs, c = std::cos (w), al = std::sin (w) / (2.0 * q);
            return normalise (A * ((A + 1) + (A - 1) * c + 2 * sA * al),
                              -2 * A * ((A - 1) + (A + 1) * c),
                              A * ((A + 1) + (A - 1) * c - 2 * sA * al),
                              (A + 1) - (A - 1) * c + 2 * sA * al,
                              2 * ((A - 1) - (A + 1) * c),
                              (A + 1) - (A - 1) * c - 2 * sA * al);
        }

        /** First order low pass, 6 dB per octave, as a degenerate biquad. */
        static BiquadCoeffs lowPass1 (double fs, double f)
        {
            const double k = std::tan (kPi * clampFreq (f, fs) / fs);
            return { k / (k + 1), k / (k + 1), 0.0, (k - 1) / (k + 1), 0.0 };
        }

        static BiquadCoeffs highPass1 (double fs, double f)
        {
            const double k = std::tan (kPi * clampFreq (f, fs) / fs);
            return { 1 / (k + 1), -1 / (k + 1), 0.0, (k - 1) / (k + 1), 0.0 };
        }

        /** |H| at a frequency, in dB. Used to draw the EQ curve. */
        double magnitudeDb (double f, double fs) const
        {
            const double w = 2.0 * kPi * f / fs;
            const double cr = std::cos (w), ci = -std::sin (w), c2r = std::cos (2 * w), c2i = -std::sin (2 * w);
            const double nr = b0 + b1 * cr + b2 * c2r, ni = b1 * ci + b2 * c2i;
            const double dr = 1 + a1 * cr + a2 * c2r, di = a1 * ci + a2 * c2i;
            const double mag = std::sqrt ((nr * nr + ni * ni) / std::max (1e-30, dr * dr + di * di));
            return 20.0 * std::log10 (std::max (1e-12, mag));
        }
    };

    struct Biquad
    {
        BiquadCoeffs c;
        double s1 { 0.0 }, s2 { 0.0 };

        void set (const BiquadCoeffs& coeffs) noexcept { c = coeffs; }
        void reset() noexcept { s1 = s2 = 0.0; }

        inline float process (float xf) noexcept
        {
            const double x = xf;
            const double y = c.b0 * x + s1;
            s1 = c.b1 * x - c.a1 * y + s2;
            s2 = c.b2 * x - c.a2 * y;
            return (float) y;
        }
    };

    // -------------------------------------------------------------------------
    // Butterworth cascades, for 12 / 24 / 36 / 48 dB per octave slopes
    // -------------------------------------------------------------------------
    struct ButterworthQs
    {
        /** Section Qs for an even-order Butterworth of 2, 4, 6 or 8 poles. */
        static const double* forStages (int stages)
        {
            static const double q1[] = { 0.70710678 };
            static const double q2[] = { 0.54119610, 1.30656296 };
            static const double q3[] = { 0.51763809, 0.70710678, 1.93185165 };
            static const double q4[] = { 0.50979558, 0.60134489, 0.89997622, 2.56291545 };
            switch (stages) { case 1: return q1; case 2: return q2; case 3: return q3; default: return q4; }
        }
    };

    /** High or low pass at 12, 24, 36 or 48 dB per octave. @p resonance scales
        the sharpest section's Q, so the user's Q still does something on a
        steep slope. */
    struct SlopeFilter
    {
        std::array<Biquad, 4> stages;
        int active { 1 };

        void design (bool highPass, double fs, double f, int slopeIndex, double resonance = 0.70710678)
        {
            active = std::clamp (slopeIndex, 0, 3) + 1;
            const double* qs = ButterworthQs::forStages (active);
            const double scale = resonance / 0.70710678;

            for (int i = 0; i < active; ++i)
            {
                const double q = (i == active - 1) ? qs[i] * scale : qs[i];
                stages[(size_t) i].set (highPass ? BiquadCoeffs::highPass (fs, f, std::max (0.1, q))
                                                 : BiquadCoeffs::lowPass  (fs, f, std::max (0.1, q)));
            }
        }

        void reset() { for (auto& s : stages) s.reset(); }

        inline float process (float x) noexcept
        {
            for (int i = 0; i < active; ++i) x = stages[(size_t) i].process (x);
            return x;
        }

        double magnitudeDb (double f, double fs) const
        {
            double db = 0;
            for (int i = 0; i < active; ++i) db += stages[(size_t) i].c.magnitudeDb (f, fs);
            return db;
        }
    };

    // -------------------------------------------------------------------------
    // TPT state variable filter (Zavalishin). Cheap to retune every sample,
    // which is what the swept filters in the underwater engine and the filter
    // FX need, and it gives low, band and high pass from one structure.
    // -------------------------------------------------------------------------
    struct Svf
    {
        double ic1 { 0.0 }, ic2 { 0.0 };
        double a1 { 0.0 }, a2 { 0.0 }, a3 { 0.0 }, k { 1.41421356 };
        double lp { 0.0 }, bp { 0.0 }, hp { 0.0 };

        void reset() noexcept { ic1 = ic2 = lp = bp = hp = 0.0; }

        /** The prewarped integrator gain for a cutoff. Computed once and shared
            when several stages sit at the same frequency, which saves a tan()
            per stage per sample on a swept cascade. */
        static inline double gFor (double fs, double f) noexcept
        {
            return std::tan (kPi * std::clamp (f, 10.0, fs * 0.45) / fs);
        }

        inline void tuneG (double g, double q) noexcept
        {
            k = 1.0 / std::max (0.3, q);
            a1 = 1.0 / (1.0 + g * (g + k));
            a2 = g * a1;
            a3 = g * a2;
        }

        /** @param q  0.5 upwards; about 0.707 is flat, 10 is a strong whistle. */
        inline void tune (double fs, double f, double q) noexcept { tuneG (gFor (fs, f), q); }

        inline void process (float xf) noexcept
        {
            const double x = xf;
            const double v3 = x - ic2;
            const double v1 = a1 * ic1 + a2 * v3;
            const double v2 = ic2 + a2 * ic1 + a3 * v3;
            ic1 = 2.0 * v1 - ic1;
            ic2 = 2.0 * v2 - ic2;
            lp = v2; bp = v1; hp = x - k * v1 - v2;
        }

        /** Band pass normalised to 0 dB at the centre. */
        inline double bandNorm() const noexcept { return bp * k; }
    };

    // -------------------------------------------------------------------------
    // Linkwitz-Riley 4th order crossover: two Butterworth sections per side.
    // Low + high sums to a second order all pass, so the split is transparent.
    // -------------------------------------------------------------------------
    struct Lr4Crossover
    {
        Biquad lp1, lp2, hp1, hp2;

        void design (double fs, double f)
        {
            const auto l = BiquadCoeffs::lowPass (fs, f, 0.70710678);
            const auto h = BiquadCoeffs::highPass (fs, f, 0.70710678);
            lp1.set (l); lp2.set (l); hp1.set (h); hp2.set (h);
        }

        void reset() { lp1.reset(); lp2.reset(); hp1.reset(); hp2.reset(); }

        inline void process (float x, float& low, float& high) noexcept
        {
            low  = lp2.process (lp1.process (x));
            high = hp2.process (hp1.process (x));
        }
    };

    // -------------------------------------------------------------------------
    // Small building blocks
    // -------------------------------------------------------------------------

    /** One pole low pass on a control or audio signal, set by a time constant. */
    struct OnePole
    {
        float a { 0.0f }, z { 0.0f };
        void setTime (double fs, double seconds) { a = (float) std::exp (-1.0 / std::max (1.0, seconds * fs)); }
        void setCutoff (double fs, double hz)   { a = (float) std::exp (-2.0 * kPi * std::max (1.0, hz) / fs); }
        inline float process (float x) noexcept { z = x + a * (z - x); return z; }
        void reset (float v = 0.0f) { z = v; }
    };

    /** Fractional delay line with linear interpolation, sized once. */
    struct DelayLine
    {
        std::vector<float> buf;
        int w { 0 }, size { 1 };

        void allocate (int samples) { size = std::max (4, samples); buf.assign ((size_t) size, 0.0f); w = 0; }
        void clear() { std::fill (buf.begin(), buf.end(), 0.0f); w = 0; }

        inline void push (float x) noexcept { buf[(size_t) w] = x; if (++w >= size) w = 0; }

        /** Read @p d samples behind the most recently pushed sample. */
        inline float read (float d) const noexcept
        {
            // a NaN or infinite delay would otherwise survive std::clamp and
            // turn into an out-of-range index; treat it as no delay at all
            if (! (d >= 0.0f)) d = 0.0f;
            if (size < 4 || buf.empty()) return 0.0f;
            d = std::min (d, (float) (size - 2));

            // split into whole and fractional samples and wrap the index as an
            // integer. Wrapping a float position instead can round a position
            // just below zero up to exactly `size`, one past the end.
            const int whole = (int) d;
            const float fr = d - (float) whole;
            int i0 = w - 1 - whole;                     // `whole` samples back
            if (i0 < 0) i0 += size;
            const int i1 = i0 == 0 ? size - 1 : i0 - 1; // one sample further back
            return buf[(size_t) i0] + fr * (buf[(size_t) i1] - buf[(size_t) i0]);
        }
    };

    /** A gain follower that brings a processed signal back to the level of
        the signal that fed it. Moving a filter or a drive then changes the
        character, not the loudness, which is what makes an A/B honest. Below
        the gate it holds its gain rather than climbing into the noise floor. */
    struct AutoGain
    {
        double inE { 1e-6 }, outE { 1e-6 }, coef { 0.999 };
        float gain { 1.0f }, maxUp { 4.0f }, maxDown { 0.25f };

        void prepare (double fs, double seconds = 0.35, float maxUpDb = 12.0f, float maxDownDb = 12.0f)
        {
            coef = std::exp (-1.0 / std::max (1.0, seconds * fs));
            maxUp = std::pow (10.0f, maxUpDb / 20.0f);
            maxDown = std::pow (10.0f, -maxDownDb / 20.0f);
            reset();
        }

        void reset() { inE = outE = 1e-6; gain = 1.0f; }

        inline float process (float in, float out) noexcept
        {
            inE  = in  * (double) in  + coef * (inE  - in  * (double) in);
            outE = out * (double) out + coef * (outE - out * (double) out);

            if (inE > 1e-6)   // about -60 dBFS: below that, hold
            {
                const float target = (float) std::sqrt (inE / std::max (1e-12, outE));
                gain = std::clamp (target, maxDown, maxUp);
            }
            return out * gain;
        }
    };
}
