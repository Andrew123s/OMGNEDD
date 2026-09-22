#pragma once
/*  Sound-level verification.

    Everything here measures what the plugin does to audio, because the first
    version's suite checked only that audio stayed finite and under the
    ceiling, and a plugin whose knobs do nothing passes that easily. That is
    what shipped, and it is what these checks exist to stop happening again:

      * every parameter audibly changes the sound in a context that engages it
      * the sound does not change with the oversampling setting or the
        session's sample rate (the fault that made the first version flat)
      * two renders with the same settings are identical, so a bounce is
        repeatable
      * tempo sync, delay timing and reverb decay land where they claim to
      * nothing runs away at maximum feedback or decay
      * the de-esser, the external sidechain, polarity, the safety clip and
        true-peak limiting each do their specific job
      * knobs respond to the mouse and the wheel
*/
#include "SoundProbe.h"
#include "../Source/UI/OmgKnob.h"

namespace soundtests
{
    using Check = std::function<void (bool, const juce::String&)>;

    inline std::unique_ptr<OmgnedProcessor> fresh (double sr = 48000.0, int block = 256)
    {
        auto p = std::make_unique<OmgnedProcessor>();
        p->setPlayConfigDetails (2, 2, sr, block);
        p->prepareToPlay (sr, block);
        p->presetManager.load (0);
        return p;
    }

    inline void applySettings (OmgnedProcessor& p, const std::map<juce::String, float>& settings)
    {
        for (auto& kv : settings) probe::setParam (p, kv.first, kv.second);
    }

    /** Largest band difference among bands within @p window dB of the loudest. */
    inline float bandAgreement (const std::vector<float>& a, const std::vector<float>& b, float window, size_t maxBand = 99)
    {
        float top = -300.0f;
        for (size_t i = 0; i < a.size(); ++i) top = std::max ({ top, a[i], b[i] });
        float d = 0.0f;
        for (size_t i = 0; i < a.size() && i < maxBand; ++i)
            if (std::max (a[i], b[i]) > top - window)
                d = std::max (d, std::abs (a[i] - b[i]));
        return d;
    }

    /** Runs @p seconds of a signal generator through @p p, collecting channel 0. */
    template <typename Gen>
    std::vector<float> run (OmgnedProcessor& p, double sr, double seconds, Gen&& gen, int block = 256)
    {
        juce::AudioBuffer<float> b (2, block);
        juce::MidiBuffer m;
        std::vector<float> out;
        int64_t n = 0;
        const int64_t total = (int64_t) (seconds * sr);
        while (n < total)
        {
            for (int i = 0; i < block; ++i) { const float x = gen (n + i); b.setSample (0, i, x); b.setSample (1, i, x); }
            p.processBlock (b, m);
            for (int i = 0; i < block; ++i) out.push_back (b.getSample (0, i));
            n += block;
        }
        return out;
    }

    inline float rmsDb (const std::vector<float>& x, size_t from, size_t to)
    {
        double s = 0.0;
        to = std::min (to, x.size());
        for (size_t i = from; i < to; ++i) s += x[i] * (double) x[i];
        return (float) (10.0 * std::log10 (s / std::max<size_t> (1, to - from) + 1e-20));
    }

    // ------------------------------------------------------------------------
    inline void invariance (Check check)
    {
        juce::Logger::writeToLog ("\nSound: invariance and repeatability");
        const double sr = 48000.0;

        for (const bool drive : { false, true })
        {
            std::map<juce::String, float> s { { "engine", 0 }, { "uwMix", 100 }, { "uwDepth", 55 }, { "uwWater", 0 },
                                              { "uwPressure", drive ? 60.0f : 0.0f }, { "uwRipple", 0 }, { "uwBubble", 0 }, { "uwWave", 0 } };
            auto ref = fresh (sr); applySettings (*ref, s); probe::setParam (*ref, "oversampling", 3);
            const auto r = probe::render (*ref, sr);

            float worst = 0.0f;
            for (int os = 0; os < 3; ++os)
            {
                auto p = fresh (sr); applySettings (*p, s); probe::setParam (*p, "oversampling", (float) os);
                worst = std::max (worst, bandAgreement (probe::render (*p, sr).average, r.average, 40.0f));
            }
            check (worst < 1.0f, "the sound does not change with oversampling, " + juce::String (drive ? "drive on" : "linear path")
                                 + " (worst " + juce::String (worst, 2) + " dB in bands within 40 dB of the loudest)");
        }

        // the same settings at two session rates. The test voice itself is not
        // identical at two rates (white noise spreads its energy over a wider
        // band at 96 kHz), so what is compared is the plugin's transfer: its
        // output against its own bypassed input, band by band
        {
            std::map<juce::String, float> s { { "engine", 0 }, { "uwMix", 100 }, { "uwDepth", 55 }, { "uwWater", 0 },
                                              { "uwPressure", 0 }, { "uwRipple", 0 }, { "uwBubble", 0 }, { "uwWave", 0 },
                                              { "oversampling", 1 } };
            auto transfer = [&s] (double rate)
            {
                auto on = fresh (rate);  applySettings (*on, s);
                auto off = fresh (rate); applySettings (*off, s); probe::setParam (*off, "power", 0);
                const auto a = probe::render (*on, rate), b = probe::render (*off, rate);
                std::vector<float> t;
                for (size_t i = 0; i < a.average.size(); ++i) t.push_back (a.average[i] - b.average[i]);
                return std::make_pair (t, a.average);
            };
            const auto [t44, lvl44] = transfer (44100.0);
            const auto [t96, lvl96] = transfer (96000.0);
            // the engine's auto gain matches total input level, and the test
            // voice's noise carries more energy at 96 kHz, so a uniform offset
            // is expected; the shape of the response is what must match
            float top = -300.0f; for (auto v : lvl44) top = std::max (top, v);
            std::vector<size_t> used;
            for (size_t i = 0; i < 8 && i < t44.size(); ++i) if (lvl44[i] > top - 40.0f) used.push_back (i);
            float offset = 0.0f; for (auto i : used) offset += t96[i] - t44[i];
            offset /= (float) std::max<size_t> (1, used.size());
            float d = 0.0f; for (auto i : used) d = std::max (d, std::abs (t96[i] - t44[i] - offset));
            check (d < 1.0f, "44.1 kHz and 96 kHz sessions shape the vocal the same way (worst " + juce::String (d, 2)
                             + " dB after a uniform " + juce::String (offset, 2) + " dB level offset from auto gain)");
        }

        // determinism: a bounce is the same every time
        {
            std::map<juce::String, float> s { { "engine", 0 }, { "uwBubble", 60 }, { "sigChaos", 1 }, { "modOn", 1 },
                                              { "modMode", 4 }, { "fxOn", 1 }, { "fxShape", 5 }, { "revOn", 1 }, { "dlyOn", 1 } };
            auto a = fresh (sr); applySettings (*a, s);
            auto b = fresh (sr); applySettings (*b, s);
            probe::Voice va (sr), vb (sr);
            juce::AudioBuffer<float> ba (2, 256), bb (2, 256);
            juce::MidiBuffer m;
            float maxDiff = 0.0f;
            for (int k = 0; k < 300; ++k)
            {
                va.fill (ba); vb.fill (bb);
                a->processBlock (ba, m); b->processBlock (bb, m);
                for (int c = 0; c < 2; ++c)
                    for (int i = 0; i < 256; ++i)
                        maxDiff = std::max (maxDiff, std::abs (ba.getSample (c, i) - bb.getSample (c, i)));
            }
            check (maxDiff < 1.0e-6f, "two instances with the same settings, random modulation included, render identically");
        }
    }

    // ------------------------------------------------------------------------
    inline void sensitivity (Check check, bool report = false)
    {
        juce::Logger::writeToLog ("\nSound: every parameter changes the sound");
        const double sr = 48000.0;
        auto make = [sr] { return fresh (sr); };

        int tested = 0, failed = 0, exempt = 0;
        for (int i = 0; i < omg::numParams(); ++i)
        {
            const auto& d = omg::params()[i];
            const juce::String id (d.id);
            if (probe::exemptionFor (id).isNotEmpty()) { ++exempt; continue; }

            const auto r = probe::sweep (d, sr, make);
            const float need = probe::thresholdFor (id);
            ++tested;
            if (report)
                juce::Logger::writeToLog ("  " + id.paddedRight (' ', 16) + juce::String (r.diffDb, 2).paddedLeft (' ', 6)
                                          + " dB  (needs " + juce::String (need, 1) + ")");
            if (r.diffDb < need)
            {
                ++failed;
                juce::Logger::writeToLog ("  FAIL  " + id + " changes the sound by only " + juce::String (r.diffDb, 2)
                                          + " dB (needs " + juce::String (need, 1) + ")");
            }
        }
        check (failed == 0, juce::String (tested) + " parameters each audibly change the sound ("
                            + juce::String (exempt) + " conditional ones are checked by their own tests below)");
    }

    // ------------------------------------------------------------------------
    inline void timing (Check check)
    {
        juce::Logger::writeToLog ("\nSound: tempo, delay and reverb timing");
        const double sr = 48000.0;

        // wobble locked to the host: the output's envelope repeats every beat
        for (const double bpm : { 120.0, 90.0 })
        {
            auto p = fresh (sr);
            applySettings (*p, { { "engine", 2 }, { "satMix", 0 }, { "fxOn", 1 }, { "fxMode", 1 }, { "fxSync", 1 }, { "fxDiv", 4 },
                         { "fxFreq", 200 }, { "fxDepth", 80 }, { "fxReso", 40 }, { "oversampling", 0 } });
            omg::dsp::TransportInfo t; t.bpm = bpm; t.playing = true; t.ppqAtBlockStart = 0.0;
            p->setTransportOverride (t);

            juce::Random rng (3);
            const auto y = run (*p, sr, 4.0, [&rng] (int64_t) { return (rng.nextFloat() * 2.0f - 1.0f) * 0.3f; });

            // 10 ms RMS envelope, then the autocorrelation peak between 0.2 and 1.2 s
            const int hop = (int) (0.01 * sr);
            std::vector<float> env;
            for (size_t i = (size_t) sr; i + (size_t) hop < y.size(); i += (size_t) hop)
                env.push_back (rmsDb (y, i, i + (size_t) hop));
            float mean = 0; for (auto v : env) mean += v; mean /= (float) env.size();
            for (auto& v : env) v -= mean;

            int bestLag = 0; double best = -1e30;
            for (int lag = 20; lag < 120 && lag < (int) env.size(); ++lag)
            {
                double s = 0; for (size_t i = 0; i + (size_t) lag < env.size(); ++i) s += env[i] * env[i + (size_t) lag];
                if (s > best) { best = s; bestLag = lag; }
            }
            const double period = bestLag * 0.01, expected = 60.0 / bpm;
            check (std::abs (period - expected) / expected < 0.04,
                   "a synced 1/4 wobble at " + juce::String ((int) bpm) + " BPM repeats every " + juce::String (period, 3)
                   + " s (expected " + juce::String (expected, 3) + ")");
        }

        // a synced 1/8 dotted delay puts the first echo at 0.375 s at 120 BPM
        {
            auto p = fresh (sr);
            applySettings (*p, { { "engine", 2 }, { "satMix", 0 }, { "dlyOn", 1 }, { "dlyMix", 100 }, { "dlySync", 1 }, { "dlyDiv", 8 },
                         { "dlyFeedback", 0 }, { "dlyTone", 100 }, { "oversampling", 0 }, { "limiter", 0 } });
            omg::dsp::TransportInfo t; t.bpm = 120.0; p->setTransportOverride (t);
            const auto y = run (*p, sr, 1.0, [sr] (int64_t n) { return n == (int64_t) (0.1 * sr) ? 0.8f : 0.0f; });
            size_t peak = 0; float mx = 0;
            for (size_t i = (size_t) (0.15 * sr); i < y.size(); ++i) if (std::abs (y[i]) > mx) { mx = std::abs (y[i]); peak = i; }
            const double echo = (double) peak / sr - 0.1;
            check (std::abs (echo - 0.375) < 0.003, "a 1/8 dotted delay at 120 BPM echoes after " + juce::String (echo * 1000.0, 1) + " ms (expected 375)");
        }

        // DECAY is real: the tail a second after an impulse is far louder at 6 s than at 1 s
        {
            auto tail = [sr] (float decay)
            {
                auto p = fresh (sr);
                applySettings (*p, { { "engine", 2 }, { "satMix", 0 }, { "revOn", 1 }, { "revMix", 100 }, { "revDecay", decay },
                             { "revDuck", 0 }, { "oversampling", 0 } });
                const auto y = run (*p, sr, 2.5, [sr] (int64_t n) { return n < 64 ? 0.8f : 0.0f; });
                return rmsDb (y, (size_t) (1.2 * sr), (size_t) (1.7 * sr));
            };
            const float shortTail = tail (1.0f), longTail = tail (6.0f);
            check (longTail - shortTail > 15.0f, "REVERB DECAY sets the tail: 1.2 s after an impulse the 6 s setting is "
                                                + juce::String (longTail - shortTail, 1) + " dB louder than the 1 s setting");
        }

        // nothing runs away: maximum feedback and decay die away after the input stops
        {
            auto p = fresh (sr);
            applySettings (*p, { { "engine", 2 }, { "satMix", 0 }, { "revOn", 1 }, { "revMix", 100 }, { "revDecay", 12 },
                         { "dlyOn", 1 }, { "dlyMix", 100 }, { "dlyFeedback", 100 }, { "dlySync", 0 }, { "dlyTime", 250 },
                         { "limiter", 0 } });
            juce::Random rng (5);
            const auto y = run (*p, sr, 40.0, [&rng, sr] (int64_t n) { return n < (int64_t) (2.0 * sr) ? (rng.nextFloat() * 2.0f - 1.0f) * 0.5f : 0.0f; });
            bool finite = true; for (auto v : y) finite = finite && std::isfinite (v);
            const float early = rmsDb (y, (size_t) (2.0 * sr), (size_t) (3.0 * sr));
            const float late  = rmsDb (y, (size_t) (38.0 * sr), (size_t) (40.0 * sr));
            check (finite && late < early - 30.0f, "maximum delay feedback and 12 s decay die away rather than run away ("
                                                  + juce::String (early - late, 1) + " dB down after 36 s)");
        }
    }

    // ------------------------------------------------------------------------
    inline void specifics (Check check)
    {
        juce::Logger::writeToLog ("\nSound: specific jobs");
        const double sr = 48000.0;

        // de-esser: pulls the top of an S down, and says so
        {
            auto on = fresh (sr), off = fresh (sr);
            applySettings (*on,  { { "deOn", 1 }, { "deThresh", -40 }, { "deAmount", 80 }, { "deRange", 14 } });
            applySettings (*off, { { "deOn", 0 } });
            const auto a = probe::render (*on, sr), b = probe::render (*off, sr);
            const float hf = (b.average[7] + b.average[8]) * 0.5f - (a.average[7] + a.average[8]) * 0.5f;
            check (hf > 3.0f && on->getDeEsserReductionDb() > 3.0f,
                   "the de-esser takes " + juce::String (hf, 1) + " dB off the sibilance above 5.6 kHz");
        }

        // external sidechain: a loud key compresses a quiet vocal only when EXTERNAL SC is on
        for (const bool ext : { true, false })
        {
            OmgnedProcessor p;
            auto layout = p.getBusesLayout();
            layout.inputBuses.getReference (1) = juce::AudioChannelSet::stereo();
            const bool ok = p.setBusesLayout (layout);
            p.setRateAndBufferSizeDetails (sr, 256);
            p.prepareToPlay (sr, 256);
            p.presetManager.load (0);
            applySettings (p, { { "compOn", 1 }, { "compThresh", -30 }, { "compRatio", 8 }, { "compScExt", ext ? 1.0f : 0.0f },
                        { "compScAmount", 100 } });

            juce::AudioBuffer<float> b (4, 256);
            juce::MidiBuffer m;
            float gr = 0.0f;
            for (int k = 0; k < 100; ++k)
            {
                for (int i = 0; i < 256; ++i)
                {
                    const float quiet = 0.003f * std::sin ((float) (k * 256 + i) * 0.05f);
                    const float key = 0.7f * std::sin ((float) (k * 256 + i) * 0.03f);
                    b.setSample (0, i, quiet); b.setSample (1, i, quiet);
                    b.setSample (2, i, key);   b.setSample (3, i, key);
                }
                p.processBlock (b, m);
                gr = p.getCompressorReductionDb();
            }
            if (ext)
                check (ok && p.isSidechainConnected() && gr > 6.0f,
                       "EXTERNAL SC: a loud sidechain compresses a quiet vocal by " + juce::String (gr, 1) + " dB");
            else
                check (gr < 0.5f, "with EXTERNAL SC off the same key is ignored (" + juce::String (gr, 2) + " dB)");
        }

        // polarity: an exact inversion, nothing else
        {
            auto a = fresh (sr), b = fresh (sr);
            applySettings (*a, { { "engine", 1 } });
            applySettings (*b, { { "engine", 1 }, { "phase", 1 } });
            probe::Voice va (sr), vb (sr);
            juce::AudioBuffer<float> ba (2, 256), bb (2, 256);
            juce::MidiBuffer m;
            float worst = 0.0f, level = 0.0f;
            for (int k = 0; k < 100; ++k)
            {
                va.fill (ba); vb.fill (bb);
                a->processBlock (ba, m); b->processBlock (bb, m);
                for (int c = 0; c < 2; ++c) for (int i = 0; i < 256; ++i)
                { worst = std::max (worst, std::abs (ba.getSample (c, i) + bb.getSample (c, i))); level = std::max (level, std::abs (ba.getSample (c, i))); }
            }
            check (level > 0.01f && worst < 1.0e-5f, "PHASE inverts the output exactly");
        }

        // safety: a hard ceiling even with the limiter off and 24 dB of input gain
        {
            auto p = fresh (sr);
            applySettings (*p, { { "engine", 1 }, { "dsDrive", 90 }, { "inGain", 24 }, { "limiter", 0 }, { "safety", 1 } });
            probe::Voice v (sr);
            juce::AudioBuffer<float> b (2, 256);
            juce::MidiBuffer m;
            float peak = 0.0f;
            for (int k = 0; k < 200; ++k) { v.fill (b, 2.0f); p->processBlock (b, m); peak = std::max (peak, b.getMagnitude (0, 256)); }
            check (peak <= 1.0f, "SAFETY holds the output at 0 dBFS with the limiter off and +24 dB going in");

            auto q = fresh (sr);
            applySettings (*q, { { "engine", 1 }, { "outGain", 24 }, { "limiter", 0 }, { "safety", 1 } });
            float peak2 = 0.0f;
            for (int k = 0; k < 200; ++k) { v.fill (b, 1.0f); q->processBlock (b, m); peak2 = std::max (peak2, b.getMagnitude (0, 256)); }
            check (peak2 <= 1.0f, "SAFETY also holds with +24 dB of OUTPUT GAIN, which now feeds the limiter rather than following it");
        }

        // true peak: the reconstructed peak stays near the ceiling
        {
            auto measure = [sr] (bool tp)
            {
                auto p = fresh (sr);
                applySettings (*p, { { "engine", 1 }, { "dsDrive", 70 }, { "compOn", 0 }, { "inGain", 6 }, { "outGain", 14 },
                                     { "truePeak", tp ? 1.0f : 0.0f }, { "ceiling", -1.0f }, { "safety", 0 } });
                probe::Voice v (sr);
                juce::AudioBuffer<float> b (2, 256);
                juce::MidiBuffer m;
                juce::dsp::Oversampling<float> up (1, 3, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true, false);
                up.initProcessing (256);
                juce::AudioBuffer<float> mono (1, 256);
                float peak = 0.0f;
                for (int k = 0; k < 300; ++k)
                {
                    v.fill (b, 1.5f); p->processBlock (b, m);
                    mono.copyFrom (0, 0, b, 0, 0, 256);
                    juce::dsp::AudioBlock<float> blk (mono);
                    auto hi = up.processSamplesUp (blk);
                    if (k > 20)
                        for (size_t i = 0; i < hi.getNumSamples(); ++i) peak = std::max (peak, std::abs (hi.getSample (0, (int) i)));
                }
                return omg::dsp::gainToDb (peak);
            };
            const float with = measure (true), without = measure (false);
            check (with <= without && with < -0.9f, "with TRUE PEAK the reconstructed peak holds the -1 dB ceiling ("
                                                   + juce::String (with, 2) + " dBFS; " + juce::String (without, 2) + " without)");
        }

        // switching engines mid-phrase is crossfaded, not stepped
        {
            auto measure = [sr] (bool switching)
            {
                auto p = fresh (sr);
                applySettings (*p, { { "engine", 0 }, { "uwMix", 100 }, { "character", 70 }, { "oversampling", 1 } });
                probe::Voice v (sr);
                juce::AudioBuffer<float> b (2, 256);
                juce::MidiBuffer m;
                float worst = 0.0f, prev = 0.0f;
                for (int k = 0; k < 240; ++k)
                {
                    if (switching && k % 20 == 10) probe::setParam (*p, "engine", (float) ((k / 20) % 3));
                    v.fill (b); p->processBlock (b, m);
                    for (int i = 0; i < 256; ++i) { const float x = b.getSample (0, i); if (k > 8) worst = std::max (worst, std::abs (x - prev)); prev = x; }
                }
                return worst;
            };
            const float plain = measure (false), switched = measure (true);
            check (switched < plain * 1.6f + 0.02f, "switching engines mid-phrase adds no step larger than the vocal's own ("
                                                    + juce::String (switched, 3) + " vs " + juce::String (plain, 3) + ")");
        }
    }

    // ------------------------------------------------------------------------
    /** Knobs respond to the mouse: a real drag and a real wheel move, sent
        through the component the way the host's window would. */
    inline void knobInteraction (juce::AudioProcessorEditor& editor, OmgnedProcessor& processor, Check check)
    {
        juce::Logger::writeToLog ("\nUI: knobs respond to the mouse");

        std::vector<omg::ui::OmgKnob*> knobs;
        std::function<void (juce::Component&)> collect = [&] (juce::Component& c)
        {
            if (auto* k = dynamic_cast<omg::ui::OmgKnob*> (&c))
                if (k->isShowing() || k->isVisible()) knobs.push_back (k);
            for (int i = 0; i < c.getNumChildComponents(); ++i) collect (*c.getChildComponent (i));
        };
        collect (editor);

        int moved = 0, wheeled = 0, tried = 0, wrote = 0;
        auto src = juce::Desktop::getInstance().getMainMouseSource();

        for (auto* k : knobs)
        {
            if (! k->isVisible() || k->getWidth() < 10) continue;
            ++tried;
            k->setValue (k->getMinimum() + (k->getMaximum() - k->getMinimum()) * 0.3, juce::sendNotificationSync);
            const double before = k->getValue();

            const juce::Point<float> down (k->getWidth() * 0.5f, k->getHeight() * 0.4f);
            const auto now = juce::Time::getCurrentTime();
            auto ev = [&] (juce::Point<float> pos, bool dragged)
            {
                return juce::MouseEvent (src, pos, juce::ModifierKeys (juce::ModifierKeys::leftButtonModifier),
                                         juce::MouseInputSource::defaultPressure, 0.0f, 0.0f, 0.0f, 0.0f,
                                         k, k, now, down, now, 1, dragged);
            };

            auto* param = processor.apvts.getParameter (k->getParameterID());
            const float paramBefore = param != nullptr ? param->getValue() : 0.0f;

            k->mouseDown (ev (down, false));
            k->mouseDrag (ev (down.translated (0.0f, -60.0f), true));
            k->mouseUp   (ev (down.translated (0.0f, -60.0f), true));
            if (k->getValue() > before) ++moved;
            if (param != nullptr && param->getValue() > paramBefore) ++wrote;

            const double beforeWheel = k->getValue();
            const bool nearTop = k->getValue() > (k->getMinimum() + k->getMaximum()) * 0.5;
            juce::MouseWheelDetails w { 0.0f, nearTop ? -0.5f : 0.5f, false, false, false };
            const juce::MouseEvent wheelEvent (src, down, juce::ModifierKeys(), juce::MouseInputSource::defaultPressure,
                                               0.0f, 0.0f, 0.0f, 0.0f, k, k, now + juce::RelativeTime::milliseconds (50),
                                               down, now, 0, false);
            k->mouseWheelMove (wheelEvent, w);
            if (std::abs (k->getValue() - beforeWheel) > 0.0) ++wheeled;
            else juce::Logger::writeToLog ("  wheel did not move " + k->getParameterID() + " at " + juce::String (beforeWheel));
        }

        check (tried > 20 && moved == tried, juce::String (moved) + " of " + juce::String (tried) + " visible knobs follow a mouse drag");
        check (tried > 20 && wheeled == tried, juce::String (wheeled) + " of " + juce::String (tried) + " visible knobs follow the mouse wheel");

        // and the parameter behind each knob really moved, which is what the DSP reads
        check (tried > 20 && wrote == tried, juce::String (wrote) + " of " + juce::String (tried)
                                             + " drags wrote through to the parameter the DSP reads");
    }
}
