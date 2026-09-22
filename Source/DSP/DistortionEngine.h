#pragma once
#include "Utils.h"
#include "Filters.h"

namespace omg::dsp
{
    /** Eleven nonlinear algorithms in a multi-stage chain. Runs inside the
        oversampler, at whatever rate it is told.

        in -> BODY low shelf -> BITE presence peak -> pre-emphasis
           -> drive, bias, asymmetry -> curve -> bias removal
           -> CRUSH (bits and rate) -> de-emphasis -> BODY restore
           -> SMOOTH / post low pass -> auto gain -> mix

        Each tone control sits in front of the curve, because that is where it
        changes what distorts rather than only what comes out. BODY is then
        partly restored after the curve, so turning it up makes the low end
        thicker and more saturated rather than simply louder.

        Auto gain matches the output level to the input, so DRIVE, EDGE and
        the algorithm change the character and not the loudness. A fixed
        makeup formula cannot do that across eleven curves with very different
        gain; a slow follower can.
    */
    class DistortionEngine
    {
    public:
        enum Algo { Soft = 0, Hard, Tube, Tape, Fuzz, Fold, Digital, Rectify, Shaper, BitCrush, Asym };

        struct Params
        {
            float drive = 35, bite = 40, body = 50, crush = 0, edge = 30, smooth = 25, mix = 100;
            float bias = 0, asym = 0, preEmph = 20, postFilter = 16000;
            int   algo = Soft;
            bool  chaos = false;
        };

        void prepare (double maxRate, int /*maxBlock*/) { setSampleRate (maxRate); }

        void setSampleRate (double fs)
        {
            sampleRate = fs;
            for (auto& ch : chans)
            {
                ch.autoGain.prepare (fs, 0.35, 12.0f, 24.0f);
                ch.dc.set (BiquadCoeffs::highPass1 (fs, 20.0));
                ch.chaosSmooth.setTime (fs, 0.05);
            }
            reset();
            setParams (p);
        }

        void reset()
        {
            for (auto& ch : chans)
            {
                for (auto* b : { &ch.body, &ch.bite, &ch.pre, &ch.post, &ch.bodyRestore, &ch.dc })
                    b->reset();
                ch.smooth.reset();
                ch.autoGain.reset();
                ch.holdCounter = 0.0f; ch.held = 0.0f;
            }
            for (size_t c = 0; c < chans.size(); ++c) chans[c].random.setSeed ((juce::int64) (0xD157 + c));
        }

        void setParams (const Params& np)
        {
            p = np;
            const double fs = sampleRate;

            driveGain = dbToGain (pct (p.drive) * 40.0f);
            edgeAmt   = pct (p.edge);
            crushAmt  = pct (p.crush);
            mixAmt    = pct (p.mix);
            biasAmt   = bipolar (p.bias) * 0.45f;
            asymAmt   = bipolar (p.asym);

            // bits 16 down to 3; the hold is in session-rate samples, so the
            // decimation sounds the same at any oversampling setting
            bits     = juce::jlimit (3.0f, 16.0f, 16.0f - crushAmt * 13.0f);
            bitStep  = std::pow (2.0f, bits - 1.0f);
            decimate = (1.0f + crushAmt * 15.0f) * (float) (fs / 48000.0);

            const double preDb  = pct (p.preEmph) * 12.0;
            const double biteDb = pct (p.bite) * 15.0 - 3.0;        // -3 .. +12 dB
            const double bodyDb = pct (p.body) * 15.0 - 6.0;        // -6 .. +9 dB

            // SMOOTH closes a low pass from the post-filter setting down to
            // 1.8 kHz, exponentially, so the whole travel of the knob is heard
            const double smoothHz = std::max (1800.0, (double) p.postFilter * std::pow (1800.0 / std::max (1800.0, (double) p.postFilter), (double) pct (p.smooth)));

            for (auto& ch : chans)
            {
                ch.body.set (BiquadCoeffs::lowShelf (fs, 200.0, 0.7, bodyDb));
                ch.bite.set (BiquadCoeffs::peak (fs, 2800.0, 0.9, biteDb));
                ch.pre.set  (BiquadCoeffs::highShelf (fs, 2600.0, 0.7,  preDb));
                ch.post.set (BiquadCoeffs::highShelf (fs, 2600.0, 0.7, -preDb));
                ch.bodyRestore.set (BiquadCoeffs::lowShelf (fs, 200.0, 0.7, -bodyDb * 0.45));
                ch.smooth.design (false, fs, smoothHz, 1, 0.707);
            }
        }

        static float shape (int algo, float x, float edge)
        {
            switch (algo)
            {
                case Hard:    return juce::jlimit (-1.0f, 1.0f, x * (1.0f + edge * 2.0f));
                case Tube:    return x >= 0.0f ? std::tanh (x * (1.0f + edge))
                                               : std::tanh (x * (1.0f + edge) * 0.72f) * 0.86f;
                case Tape:    { const float t = std::tanh (x * 1.4f); return t - 0.16f * t * t * t; }
                case Fuzz:    { const float s = x < 0.0f ? -1.0f : 1.0f;
                                return s * (1.0f - std::exp (-std::abs (x) * (3.0f + edge * 7.0f))); }
                case Fold:    { float y = x * (1.0f + edge * 3.0f);
                                for (int i = 0; i < 6; ++i)
                                {
                                    if (y > 1.0f)  y = 2.0f - y;
                                    if (y < -1.0f) y = -2.0f - y;
                                }
                                return juce::jlimit (-1.0f, 1.0f, y); }
                case Digital: { const float q = juce::jmax (2.0f, 40.0f - edge * 36.0f);
                                return juce::jlimit (-1.0f, 1.0f, std::round (juce::jlimit (-1.0f, 1.0f, x) * q) / q); }
                case Rectify: { const float r = std::abs (std::tanh (x)); return (r * 2.0f - 1.0f) * 0.7f + std::tanh (x) * 0.3f; }
                case Shaper:  { const float c = juce::jlimit (-1.0f, 1.0f, x);
                                const float t3 = 4.0f * c * c * c - 3.0f * c;
                                const float t5 = 16.0f * c * c * c * c * c - 20.0f * c * c * c + 5.0f * c;
                                return juce::jlimit (-1.0f, 1.0f, c + (0.25f + edge) * (0.45f * t3 + 0.22f * t5)); }
                case BitCrush: { const float levels = juce::jmax (2.0f, std::round (juce::jmap (edge, 24.0f, 2.0f)));
                                 return juce::jlimit (-1.0f, 1.0f, std::round (std::tanh (x) * levels) / levels); }
                case Asym:    return x >= 0.0f ? std::tanh (x * (1.0f + edge * 2.2f))
                                               : (x / (1.0f + std::abs (x) * (1.4f + edge))) * 0.78f;
                case Soft:
                default:      return std::tanh (x * (1.0f + edge * 1.6f));
            }
        }

        void process (juce::AudioBuffer<float>& buffer)
        {
            const int numCh = juce::jmin (2, buffer.getNumChannels());
            const int n = buffer.getNumSamples();

            for (int c = 0; c < numCh; ++c)
            {
                auto& ch = chans[(size_t) c];
                auto* d = buffer.getWritePointer (c);

                for (int i = 0; i < n; ++i)
                {
                    const float dry = d[i];
                    float x = ch.pre.process (ch.bite.process (ch.body.process (dry)));

                    float g = driveGain;
                    if (p.chaos)
                        g *= 1.0f + ch.chaosSmooth.process ((ch.random.nextFloat() - 0.5f) * 0.6f);

                    x = x * g + biasAmt;

                    if (std::abs (asymAmt) > 0.001f)
                        x = x >= 0.0f ? x * (1.0f + asymAmt * 0.8f) : x * (1.0f - asymAmt * 0.8f);

                    x = shape (p.algo, x, edgeAmt) - shape (p.algo, biasAmt, edgeAmt);

                    if (crushAmt > 0.0001f)
                    {
                        ch.holdCounter += 1.0f;
                        if (ch.holdCounter >= decimate)
                        {
                            ch.holdCounter -= decimate;
                            ch.held = std::round (x * bitStep) / bitStep;
                        }
                        x = lerp (x, ch.held, juce::jmin (1.0f, crushAmt * 1.5f));
                    }

                    x = ch.bodyRestore.process (ch.post.process (x));
                    x = ch.smooth.process (x);
                    x = ch.dc.process (x);
                    x = ch.autoGain.process (dry, x);

                    d[i] = dry + (x - dry) * mixAmt;
                }
            }
        }

    private:
        struct Channel
        {
            Biquad body, bite, pre, post, bodyRestore, dc;
            SlopeFilter smooth;
            AutoGain autoGain;
            OnePole chaosSmooth;
            juce::Random random;
            float holdCounter { 0.0f }, held { 0.0f };
        };

        std::array<Channel, 2> chans;
        Params p;
        double sampleRate { 96000.0 };
        float driveGain { 1.0f }, edgeAmt { 0.0f }, crushAmt { 0.0f }, mixAmt { 1.0f };
        float biasAmt { 0.0f }, asymAmt { 0.0f }, bits { 16.0f }, bitStep { 32768.0f }, decimate { 1.0f };
    };
}
