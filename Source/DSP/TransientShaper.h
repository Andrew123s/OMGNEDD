#pragma once
#include "Utils.h"

namespace omg::dsp
{
    /** Vocal attack and body.

        Two envelope followers watch the same signal: one fast, one slow. Where
        the fast one is above the slow one the vocal is starting a word, and
        ATTACK lifts or softens that; where the fast one has fallen back to the
        slow one the vocal is holding a note, and BODY lifts or thins that.

        It is deliberately gentler than a drum transient designer. The gain is
        limited to +/- 9 dB, both envelopes are level relative rather than
        absolute so it behaves the same on a quiet ad-lib and a shouted hook,
        and the applied gain is smoothed over 3 ms so consonants do not click.

        The detector is shared across the pair, which keeps the stereo image
        intact on a doubled or widened vocal.
    */
    class TransientShaper
    {
    public:
        struct Params
        {
            float attack = 0, body = 0;         // -100 .. +100
            bool  enabled = false;
        };

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;

            fast.prepare (sampleRate);
            slow.prepare (sampleRate);
            fast.setTimes (0.5f, 45.0f);
            slow.setTimes (28.0f, 320.0f);

            gainSmooth.reset (sampleRate, 0.003, 1.0f);
            reset();
        }

        void reset()
        {
            fast.env = 0.0f;
            slow.env = 0.0f;
            gainSmooth.setImmediate (1.0f);
        }

        void setParams (const Params& np)
        {
            p = np;
            attackAmt = bipolar (p.attack);
            bodyAmt   = bipolar (p.body);
        }

        bool isActive() const noexcept
        {
            return p.enabled && (std::abs (attackAmt) > 0.001f || std::abs (bodyAmt) > 0.001f);
        }

        void process (juce::AudioBuffer<float>& buffer)
        {
            if (! isActive()) return;

            const int numCh = juce::jmin (2, buffer.getNumChannels());
            const int n = buffer.getNumSamples();

            for (int i = 0; i < n; ++i)
            {
                float key = 0.0f;
                for (int c = 0; c < numCh; ++c)
                    key = juce::jmax (key, std::abs (buffer.getSample (c, i)));

                const float f = fast.process (key);
                const float s = slow.process (key);

                // how far the fast envelope is above the slow one, in dB, which
                // is positive on an onset and around zero on a held note
                const float diffDb = juce::jlimit (-24.0f, 24.0f,
                                                   gainToDb (f + 1.0e-7f) - gainToDb (s + 1.0e-7f));

                const float onset   = juce::jlimit (0.0f, 1.0f, diffDb / 9.0f);
                const float holding = 1.0f - onset;

                const float db = juce::jlimit (-9.0f, 9.0f,
                                               attackAmt * onset * 9.0f + bodyAmt * holding * 6.0f);

                const float g = gainSmooth.next (dbToGain (db));

                for (int c = 0; c < numCh; ++c)
                    buffer.setSample (c, i, buffer.getSample (c, i) * g);
            }
        }

    private:
        Params p;
        double sampleRate { 44100.0 };
        EnvFollower fast, slow;
        Smooth gainSmooth;
        float attackAmt { 0.0f }, bodyAmt { 0.0f };
    };
}
