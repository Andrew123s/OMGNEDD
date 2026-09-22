#pragma once
#include "Utils.h"
#include "Filters.h"
#include "Tempo.h"

namespace omg::dsp
{
    /** Reverb and delay, both able to duck under the vocal.

        This is most of what makes an underground vocal sound underground: the
        voice sits inside a dark wash that swells in the gaps between words
        and gets out of the way while the words are happening. Without it,
        every engine sounds dry and flat however it is set, which is exactly
        what the first version of this plugin sounded like.

        REVERB  An 8-line feedback delay network with a Householder matrix,
                slowly modulated line lengths so it does not ring metallically,
                damping inside the loop, four input diffusers and a pre-delay.
                DECAY sets the real RT60: each line's gain is computed from its
                own length so the whole network decays at the same rate.
        DELAY   Stereo, tempo-synced or free, with filtered and gently
                saturated feedback, ping-pong, and WARP for tape wow on the
                repeats. The repeats are also fed into the reverb, so echoes
                smear into the wash rather than sitting on top of it.
        DUCK    Both follow the dry vocal and pull their own level down by up
                to 20 dB while it is loud.

        MIX keeps the dry vocal at full level up to 50 % and adds the wet in;
        beyond 50 % the dry fades out, so 100 % is fully wet.
    */
    class SpaceEngine
    {
    public:
        struct Params
        {
            bool  revOn = false;
            float revMix = 25, revSize = 55, revDecay = 2.4f, revDamp = 50, revPre = 20, revDuck = 0;

            bool  dlyOn = false;
            float dlyMix = 20;
            bool  dlySync = true;
            int   dlyDiv = 8;                    // 1/8 dotted
            float dlyTimeMs = 375, dlyFeedback = 35, dlyTone = 50;
            bool  dlyPing = false;
            float dlyDuck = 0, dlyWarp = 0;
        };

        static constexpr int N = 8;

        void prepare (double fs, int /*maxBlock*/)
        {
            sampleRate = fs;

            for (auto& l : lines) l.allocate ((int) (0.20 * fs) + 16);
            for (auto& ch : chans)
            {
                ch.pre.allocate ((int) (0.26 * fs) + 8);
                ch.delay.allocate ((int) (3.1 * fs) + 8);
                for (auto& a : ch.diff) a.allocate ((int) (0.03 * fs) + 8);
                ch.outHp.set (BiquadCoeffs::highPass (fs, 140.0, 0.707));
                ch.duckEnv.prepare (fs);
                ch.duckEnv.setTimes (5.0f, 280.0f);
                ch.timeSmooth.setTime (fs, 0.12);
            }

            for (auto* s : { &dryGain, &revGain, &dlyGain, &revDuckGain, &dlyDuckGain })
                s->setTime (fs, 0.03);

            reset();
            setParams (p, {});
        }

        void reset()
        {
            for (auto& l : lines) l.clear();
            for (auto& d : damping) d.reset();
            inputTone.reset();
            for (auto& ch : chans)
            {
                ch.pre.clear(); ch.delay.clear();
                for (auto& a : ch.diff) a.clear();
                ch.outHp.reset(); ch.fbHp.reset(); ch.fbLp.reset();
                ch.duckEnv.env = 0.0f;
                ch.timeSmooth.reset ((float) delaySamplesTarget);
                ch.lastDelayOut = 0.0f;
            }
            dryGain.reset (1.0f); revGain.reset (0.0f); dlyGain.reset (0.0f);
            revDuckGain.reset (1.0f); dlyDuckGain.reset (1.0f);
            lfoT = warpT = flutterT = 0.0;
        }

        bool isActive() const noexcept
        {
            return (p.revOn && p.revMix > 0.01f) || (p.dlyOn && p.dlyMix > 0.01f)
                || revGain.z > 1.0e-4f || dlyGain.z > 1.0e-4f;
        }

        void setParams (const Params& np, const TransportInfo& t)
        {
            p = np;
            const double fs = sampleRate;

            // ---- reverb
            const double sizeScale = 0.45 + pct (p.revSize) * 1.1;
            static const double baseMs[N] = { 37.3, 42.7, 49.1, 55.7, 61.3, 67.9, 73.1, 80.9 };
            const double rt60 = juce::jlimit (0.2, 20.0, (double) p.revDecay);

            for (int i = 0; i < N; ++i)
            {
                lineLen[i] = (float) (baseMs[i] * sizeScale * 0.001 * fs);
                lineGain[i] = (float) std::pow (10.0, -3.0 * (lineLen[i] / fs) / rt60);
            }

            const double dampHz = 16000.0 * std::pow (1400.0 / 16000.0, (double) pct (p.revDamp));
            for (auto& d : damping) d.set (BiquadCoeffs::lowPass1 (fs, dampHz));
            // DAMP darkens what goes in as well as what circulates, so a dark
            // setting is dark from the first reflection, not only in the tail
            inputTone.set (BiquadCoeffs::lowPass (fs, 18000.0 * std::pow (2600.0 / 18000.0, (double) pct (p.revDamp)), 0.6));

            preSamples = (float) (juce::jlimit (0.0f, 250.0f, p.revPre) * 0.001 * fs);
            modDepth = (float) (0.00035 * fs);

            static const double diffMs[4] = { 3.2, 2.4, 8.6, 6.3 };
            for (int k = 0; k < 4; ++k)
                diffLen[k] = (float) (diffMs[k] * (0.6 + 0.4 * sizeScale) * 0.001 * fs);

            // ---- delay
            const double secs = p.dlySync ? Divisions::seconds (p.dlyDiv, t.bpm)
                                          : juce::jlimit (1.0, 3000.0, (double) p.dlyTimeMs) * 0.001;
            delaySamplesTarget = juce::jlimit (1.0, 2.95 * fs, secs * fs);

            const double tone = pct (p.dlyTone);
            for (auto& ch : chans)
            {
                ch.fbLp.set (BiquadCoeffs::lowPass (fs, 1200.0 * std::pow (14000.0 / 1200.0, tone), 0.6));
                ch.fbHp.set (BiquadCoeffs::highPass (fs, 240.0 - tone * 150.0, 0.6));
            }
            feedback = juce::jlimit (0.0f, 0.95f, pct (p.dlyFeedback) * 0.95f);
            warpSwing = (float) (pct (p.dlyWarp) * 0.009 * fs);
        }

        void process (juce::AudioBuffer<float>& buffer)
        {
            const int numCh = buffer.getNumChannels();
            const int n = buffer.getNumSamples();
            if (numCh == 0) return;

            auto mixLaw = [] (bool on, float mixPct, float& dry, float& wet)
            {
                const float m = on ? pct (mixPct) : 0.0f;
                dry = juce::jmin (1.0f, 2.0f * (1.0f - m));
                wet = juce::jmin (1.0f, 2.0f * m);
            };

            float revDry, revWet, dlyDry, dlyWet;
            mixLaw (p.revOn, p.revMix, revDry, revWet);
            mixLaw (p.dlyOn, p.dlyMix, dlyDry, dlyWet);
            const float dryTarget = juce::jmin (revDry, dlyDry);

            const double fs = sampleRate;
            auto* L = buffer.getWritePointer (0);
            auto* R = buffer.getWritePointer (numCh > 1 ? 1 : 0);

            for (int i = 0; i < n; ++i)
            {
                const float inL = L[i], inR = numCh > 1 ? R[i] : L[i];

                // ---- ducking detector on the dry vocal
                const float env = chans[0].duckEnv.process (0.5f * (std::abs (inL) + std::abs (inR)));
                const float loud = juce::jlimit (0.0f, 1.0f, (gainToDb (env + 1.0e-7f) + 42.0f) / 30.0f);
                const float rDuck = revDuckGain.process (dbToGain (-pct (p.revDuck) * 20.0f * loud));
                const float dDuck = dlyDuckGain.process (dbToGain (-pct (p.dlyDuck) * 20.0f * loud));

                const float dg = dryGain.process (dryTarget);
                const float rg = revGain.process (revWet);
                const float lg = dlyGain.process (dlyWet);

                // ---- delay
                float dOutL = 0.0f, dOutR = 0.0f;
                if (lg > 1.0e-5f || p.dlyOn)
                {
                    warpT += 0.55 / fs;    if (warpT >= 1.0) warpT -= 1.0;
                    flutterT += 5.7 / fs;  if (flutterT >= 1.0) flutterT -= 1.0;
                    const float wob = warpSwing * (float) (std::sin (2.0 * kPi * warpT) + 0.25 * std::sin (2.0 * kPi * flutterT));

                    // TONE shapes every repeat, the first one included
                    for (int c = 0; c < 2; ++c)
                    {
                        auto& ch = chans[(size_t) c];
                        const float t = ch.timeSmooth.process ((float) delaySamplesTarget) + wob + warpSwing;
                        ch.lastDelayOut = ch.fbHp.process (ch.fbLp.process (ch.delay.read (juce::jmax (1.0f, t - 1.0f))));
                    }

                    const float fbL = p.dlyPing ? chans[1].lastDelayOut : chans[0].lastDelayOut;
                    const float fbR = p.dlyPing ? chans[0].lastDelayOut : chans[1].lastDelayOut;
                    const float feedL = p.dlyPing ? 0.5f * (inL + inR) : inL;
                    const float feedR = p.dlyPing ? 0.0f : inR;

                    auto sat = [] (float x) { return std::tanh (x * 1.2f) / 1.2f; };
                    chans[0].delay.push (feedL + sat (fbL) * feedback);
                    chans[1].delay.push (feedR + sat (fbR) * feedback);

                    dOutL = chans[0].lastDelayOut * dDuck;
                    dOutR = chans[1].lastDelayOut * dDuck;
                }

                // ---- reverb
                float rOutL = 0.0f, rOutR = 0.0f;
                if (rg > 1.0e-5f || p.revOn)
                {
                    // pre-delay, with the echoes fed in as well
                    const float feed = 0.5f * (inL + inR) + 0.5f * (dOutL + dOutR) * lg;
                    chans[0].pre.push (inputTone.process (feed));
                    float x = chans[0].pre.read (preSamples);

                    // input diffusion: four Schroeder all passes
                    for (int k = 0; k < 4; ++k)
                    {
                        auto& a = chans[0].diff[(size_t) k];
                        const float delayed = a.read (diffLen[k] - 1.0f);
                        const float v = x + 0.62f * delayed;
                        a.push (v);
                        x = delayed - 0.62f * v;
                    }

                    lfoT += 1.0 / fs;
                    float y[N];
                    for (int j = 0; j < N; ++j)
                    {
                        const float mod = modDepth * (float) std::sin (2.0 * kPi * (lfoT * (0.21 + 0.09 * j) + j * 0.13));
                        y[j] = lines[(size_t) j].read (lineLen[j] + mod + modDepth);
                    }

                    // Householder feedback: y - (2/N) * sum(y)
                    float sum = 0.0f;
                    for (int j = 0; j < N; ++j) sum += y[j];
                    const float h = sum * (2.0f / (float) N);

                    static const float inSign[N] = { 1, -1, 1, -1, 1, -1, 1, -1 };
                    for (int j = 0; j < N; ++j)
                    {
                        const float fb = damping[(size_t) j].process ((y[j] - h) * lineGain[j]);
                        lines[(size_t) j].push (fb + x * inSign[j] * 0.35f);
                    }

                    static const float oL[N] = { 1, 1, -1, 1, -1, -1, 1, -1 };
                    static const float oR[N] = { 1, -1, 1, 1, -1, 1, -1, -1 };
                    for (int j = 0; j < N; ++j) { rOutL += y[j] * oL[j]; rOutR += y[j] * oR[j]; }

                    rOutL = chans[0].outHp.process (rOutL * 0.42f) * rDuck;
                    rOutR = chans[1].outHp.process (rOutR * 0.42f) * rDuck;
                }

                L[i] = inL * dg + rOutL * rg + dOutL * lg;
                if (numCh > 1) R[i] = inR * dg + rOutR * rg + dOutR * lg;
                else           L[i] = inL * dg + 0.5f * (rOutL + rOutR) * rg + 0.5f * (dOutL + dOutR) * lg;
            }
        }

    private:
        struct Channel
        {
            DelayLine pre, delay;
            std::array<DelayLine, 4> diff;
            Biquad outHp, fbHp, fbLp;
            EnvFollower duckEnv;
            OnePole timeSmooth;
            float lastDelayOut { 0.0f };
        };

        std::array<Channel, 2> chans;
        std::array<DelayLine, N> lines;
        std::array<Biquad, N> damping;
        Biquad inputTone;
        Params p;
        double sampleRate { 48000.0 };
        float lineLen[N] {}, lineGain[N] {}, diffLen[4] {};
        float preSamples { 0.0f }, modDepth { 10.0f }, feedback { 0.3f }, warpSwing { 0.0f };
        double delaySamplesTarget { 12000.0 };
        double lfoT { 0.0 }, warpT { 0.0 }, flutterT { 0.0 };
        OnePole dryGain, revGain, dlyGain, revDuckGain, dlyDuckGain;
    };
}
