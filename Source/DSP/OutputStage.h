#pragma once
#include "Utils.h"

namespace omg::dsp
{
    /** Stereo shaping, brickwall limiting and the last safety net.

        Order: DC blocker -> M/S encode -> width and side level ->
        mono-compatibility low narrowing -> decode -> air -> phase -> mono ->
        limiter -> ceiling -> safety clip.

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

            sideHp.prepare (spec);
            airShelf[0].prepare (spec);
            airShelf[1].prepare (spec);
            gainSmooth.reset (spec.sampleRate, 0.0015, 1.0f);
            releaseCoef = (float) std::exp (-1.0 / (0.050 * spec.sampleRate));
            limiterEnv = 0.0f;
            gr.store (0.0f);
        }

        void reset()
        {
            sideHp.reset(); airShelf[0].reset(); airShelf[1].reset();
            for (auto& d : dc) { d.x1 = 0.0f; d.y1 = 0.0f; }
            limiterEnv = 0.0f; gainSmooth.setImmediate (1.0f);
        }

        void setParams (const Params& np)
        {
            p = np;
            widthAmt = juce::jlimit (0.0f, 2.0f, p.width * 0.01f);
            sideGain = dbToGain (p.sideLevel);
            ceilingGain = dbToGain (p.ceiling);
            sideHp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass (sampleRate, juce::jmap (pct (p.monoComp), 20.0f, 320.0f), 0.707f);

            for (auto& f : airShelf)
                f.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 11000.0f, 0.6f, dbToGain (p.air ? 4.0f : 0.0f));
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
                        side = sideHp.processSample (side);

                    l[i] = mid + side;
                    r[i] = mid - side;
                }
            }

            for (int c = 0; c < juce::jmin (2, numCh); ++c)
            {
                auto* d = buffer.getWritePointer (c);
                for (int i = 0; i < n; ++i)
                    d[i] = airShelf[(size_t) c].processSample (d[i]);
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

            if (p.limiter)
            {
                for (int i = 0; i < n; ++i)
                {
                    float peak = 0.0f;
                    for (int c = 0; c < numCh; ++c)
                        peak = juce::jmax (peak, std::abs (buffer.getSample (c, i)));

                    if (p.truePeak && i > 0)
                        for (int c = 0; c < numCh; ++c)
                            peak = juce::jmax (peak, std::abs ((buffer.getSample (c, i) + buffer.getSample (c, i - 1)) * 0.5f) * 1.06f);

                    limiterEnv = juce::jmax (peak, limiterEnv * releaseCoef);

                    const float target = limiterEnv > ceilingGain ? ceilingGain / juce::jmax (1.0e-6f, limiterEnv) : 1.0f;
                    const float g = gainSmooth.next (target);
                    worstGr = juce::jmax (worstGr, -gainToDb (g));

                    for (int c = 0; c < numCh; ++c)
                        buffer.setSample (c, i, buffer.getSample (c, i) * g);
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

    private:
        Params p;
        double sampleRate { 44100.0 };
        struct DcBlocker { float x1 { 0.0f }, y1 { 0.0f }; };
        std::array<DcBlocker, 2> dc;
        float dcR { 0.999f };

        juce::dsp::IIR::Filter<float> sideHp;
        std::array<juce::dsp::IIR::Filter<float>, 2> airShelf;
        Smooth gainSmooth;
        float widthAmt { 1.0f }, sideGain { 1.0f }, ceilingGain { 1.0f };
        float limiterEnv { 0.0f }, releaseCoef { 0.99f };
        std::atomic<float> gr { 0.0f };
    };
}
