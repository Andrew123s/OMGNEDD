#pragma once
#include "Utils.h"
#include "Filters.h"

namespace omg::dsp
{
    /** Stereo shaping, lookahead limiting and the last safety net.

        Order: DC blocker -> M/S encode -> width and side level ->
        mono-compatibility low narrowing -> decode -> air -> phase -> mono ->
        1.5 ms lookahead limiter (optionally true peak) -> safety clip.

        The DC blocker is first and is not optional. An asymmetric or rectifying
        distortion curve genuinely produces a DC component, and BIAS is there to
        be used; left in, it costs headroom the limiter then has to take back,
        and it is the kind of thing that shows up as a thump on a full-range
        system rather than as anything audible in the vocal. A one-pole at 10 Hz
        removes it without touching the bottom of the voice.
    */
    class OutputStage
    {
    public:
        struct Params
        {
            float width = 100, sideLevel = 0, monoComp = 0, ceiling = -0.3f;
            bool  ms = false, limiter = true, truePeak = false, safety = true;
            bool  phase = false, mono = false, air = false;
        };

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;

            // a one-pole DC blocker: y[n] = x[n] - x[n-1] + R*y[n-1]
            dcR = (float) std::exp (-juce::MathConstants<double>::twoPi * 10.0 / spec.sampleRate);
            for (auto& d : dc) { d.x1 = 0.0f; d.y1 = 0.0f; }

            sideHp.reset(); airShelf[0].reset(); airShelf[1].reset();
            gainSmooth.reset (spec.sampleRate, 0.0015, 1.0f);
            releaseCoef = (float) std::exp (-1.0 / (0.050 * spec.sampleRate));

            // 1.5 ms of lookahead: the limiter sees a peak coming and has
            // brought the gain down by the time it arrives. The audio is always
            // delayed by this much, limiter on or off, so the latency the host
            // compensates for never changes when the switch is toggled.
            lookahead = juce::jmax (1, (int) std::round (0.0015 * spec.sampleRate));
            for (auto& d : delayed) d.allocate (lookahead + 8);
            peakRing.assign ((size_t) lookahead + 1, 0.0f);
            peakPos = 0;
            attackCoef = (float) std::exp (-1.0 / juce::jmax (1.0, lookahead / 3.0));
            limiterEnv = 0.0f;
            gr.store (0.0f);
        }

        void reset()
        {
            sideHp.reset(); airShelf[0].reset(); airShelf[1].reset();
            for (auto& d : dc) { d.x1 = 0.0f; d.y1 = 0.0f; }
            limiterEnv = 0.0f; gainSmooth.setImmediate (1.0f);
            for (auto& d : delayed) d.clear();
            std::fill (peakRing.begin(), peakRing.end(), 0.0f);
            limGain = 1.0f;
            for (auto& h : hist) h = {};
        }

        void setParams (const Params& np)
        {
            p = np;
            widthAmt = juce::jlimit (0.0f, 2.0f, p.width * 0.01f);
            sideGain = dbToGain (p.sideLevel);
            ceilingGain = dbToGain (p.ceiling);
            sideHp.set (BiquadCoeffs::highPass (sampleRate, juce::jmap (pct (p.monoComp), 20.0f, 320.0f), 0.707));

            for (auto& f : airShelf)
                f.set (BiquadCoeffs::highShelf (sampleRate, 11000.0, 0.6, p.air ? 4.0 : 0.0));
        }

        void process (juce::AudioBuffer<float>& buffer)
        {
            const int n = buffer.getNumSamples();
            const int numCh = buffer.getNumChannels();
            float worstGr = 0.0f;

            for (int c = 0; c < juce::jmin (2, numCh); ++c)
            {
                auto& d = dc[(size_t) c];
                auto* p2 = buffer.getWritePointer (c);

                for (int i = 0; i < n; ++i)
                {
                    const float x = p2[i];
                    d.y1 = x - d.x1 + dcR * d.y1;
                    d.x1 = x;
                    p2[i] = d.y1;
                }
            }

            if (numCh >= 2)
            {
                auto* l = buffer.getWritePointer (0);
                auto* r = buffer.getWritePointer (1);

                for (int i = 0; i < n; ++i)
                {
                    float mid  = (l[i] + r[i]) * 0.5f;
                    float side = (l[i] - r[i]) * 0.5f;

                    side *= widthAmt * sideGain;

                    if (p.monoComp > 0.001f)
                        side = sideHp.process (side);

                    l[i] = mid + side;
                    r[i] = mid - side;
                }
            }

            for (int c = 0; c < juce::jmin (2, numCh); ++c)
            {
                auto* d = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                    d[i] = airShelf[(size_t) c].process (d[i]);
            }

            if (p.mono && numCh >= 2)
            {
                auto* l = buffer.getWritePointer (0);
                auto* r = buffer.getWritePointer (1);
                for (int i = 0; i < n; ++i) { const float m = (l[i] + r[i]) * 0.5f; l[i] = m; r[i] = m; }
            }

            if (p.phase)
                for (int c = 0; c < numCh; ++c)
                    juce::FloatVectorOperations::multiply (buffer.getWritePointer (c), -1.0f, n);

            // ---- lookahead peak limiter ------------------------------------
            for (int i = 0; i < n; ++i)
            {
                float peak = 0.0f;
                for (int c = 0; c < numCh; ++c)
                    peak = juce::jmax (peak, std::abs (buffer.getSample (c, i)));

                // TRUE PEAK: a cubic (Catmull-Rom) reconstruction at three
                // points between the last two samples, which catches the
                // inter-sample overs a lossy encode or a DAC would produce
                for (int c = 0; c < juce::jmin (2, numCh); ++c)
                {
                    auto& h = hist[(size_t) c];
                    const float x = buffer.getSample (c, i);
                    if (p.truePeak)
                    {
                        const float p0 = h[0], p1 = h[1], p2 = h[2], p3 = x;
                        for (const float t : { 0.25f, 0.5f, 0.75f })
                        {
                            const float t2 = t * t, t3 = t2 * t;
                            const float v = 0.5f * ((2.0f * p1) + (-p0 + p2) * t
                                          + (2.0f * p0 - 5.0f * p1 + 4.0f * p2 - p3) * t2
                                          + (-p0 + 3.0f * p1 - 3.0f * p2 + p3) * t3);
                            peak = juce::jmax (peak, std::abs (v));
                        }
                    }
                    h[0] = h[1]; h[1] = h[2]; h[2] = x;
                }

                // the loudest peak anywhere in the lookahead window
                peakRing[(size_t) peakPos] = peak;
                if (++peakPos >= (int) peakRing.size()) peakPos = 0;
                float ahead = 0.0f;
                for (auto v : peakRing) ahead = juce::jmax (ahead, v);

                const float target = (p.limiter && ahead > ceilingGain) ? ceilingGain / ahead : 1.0f;
                limGain = target < limGain ? target + attackCoef * (limGain - target)
                                           : target + releaseCoef * (limGain - target);
                worstGr = juce::jmax (worstGr, -gainToDb (limGain));

                for (int c = 0; c < juce::jmin (2, numCh); ++c)
                {
                    auto& line = delayed[(size_t) c];
                    line.push (buffer.getSample (c, i));
                    buffer.setSample (c, i, line.read ((float) lookahead) * limGain);
                }
            }

            if (p.safety)
                for (int c = 0; c < numCh; ++c)
                {
                    auto* d = buffer.getWritePointer (c);
                    for (int i = 0; i < n; ++i)
                        d[i] = juce::jlimit (-1.0f, 1.0f, d[i]);
                }

            gr.store (worstGr);
        }

        float getLimiterReductionDb() const { return gr.load(); }

        /** The constant lookahead delay, in samples, for the host's latency. */
        int getLatencySamples() const noexcept { return lookahead; }

    private:
        Params p;
        double sampleRate { 44100.0 };
        struct DcBlocker { float x1 { 0.0f }, y1 { 0.0f }; };
        std::array<DcBlocker, 2> dc;
        float dcR { 0.999f };

        Biquad sideHp;
        std::array<Biquad, 2> airShelf;
        Smooth gainSmooth;
        std::array<std::array<float, 3>, 2> hist {};
        std::array<DelayLine, 2> delayed;
        std::vector<float> peakRing;
        int lookahead { 64 }, peakPos { 0 };
        float limGain { 1.0f }, attackCoef { 0.9f };
        float widthAmt { 1.0f }, sideGain { 1.0f }, ceilingGain { 1.0f };
        float limiterEnv { 0.0f }, releaseCoef { 0.99f };
        std::atomic<float> gr { 0.0f };
    };
}
