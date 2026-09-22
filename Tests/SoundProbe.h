#pragma once
/*  Measuring what a knob does to the sound.

    Used by the verification target (Tests/SoundTests.h). The first
    version of the verification target only checked that audio stayed finite
    and under the ceiling, which a plugin whose knobs do nothing passes easily.
    That is exactly what shipped. This measures the sound itself.
*/
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include <map>
#include <vector>
#include "PluginProcessor.h"

namespace probe
{
    inline void setParam (OmgnedProcessor& p, const juce::String& id, float plain)
    {
        if (auto* q = p.apvts.getParameter (id))
            q->setValueNotifyingHost (q->convertTo0to1 (plain));
    }

    /** A synthetic stereo vocal: a glottal pulse train through three moving
        formants, a pitch contour, syllable envelopes, sibilant bursts and
        plosive clicks. The right channel has different harmonic phases and
        its own breath noise, so stereo controls have side content to act on. */
    struct Voice
    {
        double sr, t = 0.0, phase = 0.0;
        juce::Random rng { 7 };
        std::array<omg::dsp::Svf, 3> fL, fR;
        omg::dsp::Biquad sibL, sibR;

        explicit Voice (double sampleRate) : sr (sampleRate)
        {
            sibL.set (omg::dsp::BiquadCoeffs::highPass (sr, 5200.0, 0.7));
            sibR.set (omg::dsp::BiquadCoeffs::highPass (sr, 5200.0, 0.7));
        }

        void fill (juce::AudioBuffer<float>& b, float level = 1.0f)
        {
            static const float vowels[5][3] = { { 730, 1090, 2440 }, { 530, 1840, 2480 }, { 270, 2290, 3010 },
                                                { 570, 840, 2410 }, { 300, 870, 2240 } };
            for (int i = 0; i < b.getNumSamples(); ++i)
            {
                const double tt = t + i / sr;
                const double f0 = 140.0 + 35.0 * std::sin (tt * 1.7) + 12.0 * std::sin (tt * 5.3);
                phase += f0 / sr; if (phase >= 1.0) phase -= 1.0;

                // a band-limited-ish glottal pulse: saw with a soft corner
                const float src = (float) (2.0 * phase - 1.0) - (float) std::pow (phase, 6.0) * 2.0f;

                // vowel changes every 220 ms
                const int v = (int) (tt / 0.22) % 5, v2 = (v + 1) % 5;
                const float fr = (float) std::fmod (tt / 0.22, 1.0);

                const double syl = std::fmod (tt, 0.44);
                const float env = (float) (syl < 0.02 ? syl / 0.02 : std::exp (-(syl - 0.02) * 2.5)) * 0.9f + 0.1f;

                float outL = 0.0f, outR = 0.0f;
                for (int k = 0; k < 3; ++k)
                {
                    const double hz = vowels[v][k] + (vowels[v2][k] - vowels[v][k]) * fr;
                    fL[(size_t) k].tune (sr, hz, 8.0); fL[(size_t) k].process (src);
                    fR[(size_t) k].tune (sr, hz * 1.01, 8.0); fR[(size_t) k].process (src * 0.97f);
                    const float g = k == 0 ? 1.0f : k == 1 ? 0.6f : 0.35f;
                    outL += (float) fL[(size_t) k].bandNorm() * g;
                    outR += (float) fR[(size_t) k].bandNorm() * g;
                }

                // an S every 0.88 s and a T at every syllable onset
                const double sPos = std::fmod (tt, 0.88);
                const float sib = (sPos > 0.60 && sPos < 0.70) ? 0.35f : 0.0f;
                const float plo = syl < 0.004 ? 0.6f : 0.0f;
                const float nL = rng.nextFloat() * 2.0f - 1.0f, nR = rng.nextFloat() * 2.0f - 1.0f;

                const float l = (outL * env * 0.35f + sibL.process (nL) * sib + nL * plo * 0.5f + nL * 0.004f) * level;
                const float r = (outR * env * 0.35f + sibR.process (nR) * sib + nR * plo * 0.5f + nR * 0.004f) * level;

                b.setSample (0, i, l);
                if (b.getNumChannels() > 1) b.setSample (1, i, r);
            }
            t += b.getNumSamples() / sr;
        }
    };

    /** A rendered result: octave-band energies averaged, and per frame. */
    struct Measure
    {
        std::vector<float> average;                  // dB per octave band
        std::vector<std::vector<float>> frames;      // dB per band per frame
        std::vector<float> sideAverage;              // the side channel's bands
        std::vector<std::vector<float>> fine;        // dB per FFT bin, 100 Hz - 8 kHz, per frame
        float rmsDb { -100.0f };
    };

    inline const std::vector<float>& bandEdges()
    {
        static const std::vector<float> e { 45, 90, 180, 355, 710, 1400, 2800, 5600, 11200, 20000 };
        return e;
    }

    inline std::vector<float> bandsOf (const std::vector<float>& power, double sr, int n)
    {
        const auto& e = bandEdges();
        std::vector<float> out;
        for (size_t b = 0; b + 1 < e.size(); ++b)
        {
            double s = 1e-14;
            for (int k = 1; k < n / 2; ++k)
            {
                const double f = k * sr / n;
                if (f >= e[b] && f < e[b + 1]) s += power[(size_t) k];
            }
            out.push_back ((float) (10.0 * std::log10 (s)));
        }
        return out;
    }

    /** Renders @p seconds of the voice through @p p and measures it. */
    inline Measure render (OmgnedProcessor& p, double sr, float seconds = 1.2f, float level = 1.0f, int block = 256)
    {
        juce::AudioBuffer<float> b (2, block);
        juce::MidiBuffer m;
        Voice voice (sr);

        for (int k = 0; k < (int) (0.4 * sr) / block; ++k) { voice.fill (b, level); p.processBlock (b, m); }

        std::vector<float> mid, side;
        const int total = (int) (seconds * sr);
        double sumSq = 0.0;
        for (int done = 0; done < total; done += block)
        {
            voice.fill (b, level);
            p.processBlock (b, m);
            for (int i = 0; i < block; ++i)
            {
                const float l = b.getSample (0, i), r = b.getSample (1, i);
                mid.push_back (0.5f * (l + r));
                side.push_back (0.5f * (l - r));
                sumSq += 0.5 * (l * (double) l + r * (double) r);
            }
        }

        constexpr int order = 12, n = 1 << order;
        juce::dsp::FFT fft (order);
        std::vector<float> w (2 * n), acc ((size_t) n / 2, 0.0f), accSide ((size_t) n / 2, 0.0f);
        Measure out;
        int frames = 0;

        for (size_t start = 0; start + n <= mid.size(); start += n / 2, ++frames)
        {
            std::vector<float> frame ((size_t) n / 2);
            for (int pass = 0; pass < 2; ++pass)
            {
                const auto& src = pass == 0 ? mid : side;
                std::fill (w.begin(), w.end(), 0.0f);
                for (int i = 0; i < n; ++i)
                    w[(size_t) i] = src[start + (size_t) i] * (0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (n - 1)));
                fft.performFrequencyOnlyForwardTransform (w.data());
                for (int k = 0; k < n / 2; ++k)
                {
                    const float pw = w[(size_t) k] * w[(size_t) k];
                    if (pass == 0) { acc[(size_t) k] += pw; frame[(size_t) k] = pw; }
                    else           accSide[(size_t) k] += pw;
                }
            }
            out.frames.push_back (bandsOf (frame, sr, n));

            std::vector<float> fineFrame;
            for (int k = (int) (100.0 * n / sr); k < (int) (8000.0 * n / sr); ++k)
                fineFrame.push_back ((float) (10.0 * std::log10 (frame[(size_t) k] + 1e-14)));
            out.fine.push_back (std::move (fineFrame));
        }

        for (auto& v : acc) v /= (float) std::max (1, frames);
        for (auto& v : accSide) v /= (float) std::max (1, frames);
        out.average = bandsOf (acc, sr, n);
        out.sideAverage = bandsOf (accSide, sr, n);
        out.rmsDb = (float) (10.0 * std::log10 (sumSq / std::max (1, total) + 1e-20));
        return out;
    }

    /** How different two renders sound, in dB: the largest of the average
        spectrum change, the side-channel spectrum change, and the mean
        change across the spectrogram (which catches rates, delay times and
        anything else that moves the sound in time without moving its
        average spectrum). Bands more than 60 dB below the loudest are
        ignored on both sides: those differences are real but inaudible. */
    inline float difference (const Measure& a, const Measure& b)
    {
        auto bandDiff = [] (const std::vector<float>& x, const std::vector<float>& y)
        {
            float top = -300.0f;
            for (size_t i = 0; i < x.size(); ++i) top = std::max ({ top, x[i], y[i] });
            float d = 0.0f;
            for (size_t i = 0; i < x.size() && i < y.size(); ++i)
                if (std::max (x[i], y[i]) > top - 60.0f)
                    d = std::max (d, std::min (40.0f, std::abs (x[i] - y[i])));
            return d;
        };

        float d = std::max (bandDiff (a.average, b.average), bandDiff (a.sideAverage, b.sideAverage));

        const size_t nf = std::min (a.frames.size(), b.frames.size());
        if (nf > 0)
        {
            double total = 0.0;
            int count = 0;
            for (size_t f = 0; f < nf; ++f)
            {
                float top = -300.0f;
                for (size_t k = 0; k < a.frames[f].size(); ++k) top = std::max ({ top, a.frames[f][k], b.frames[f][k] });
                for (size_t k = 0; k < a.frames[f].size(); ++k)
                    if (std::max (a.frames[f][k], b.frames[f][k]) > top - 50.0f)
                    { total += std::min (40.0f, std::abs (a.frames[f][k] - b.frames[f][k])); ++count; }
            }
            d = std::max (d, (float) (total / std::max (1, count)));
        }
        return d;
    }

    /** The same comparison at FFT-bin resolution between 100 Hz and 8 kHz.
        Octave bands cannot see a pitch wobble of a few tens of cents, because
        the energy stays in the same band; a pitch or time control is judged
        on this instead. Bins more than 45 dB below the frame's loudest are
        ignored on both sides. */
    inline float fineDifference (const Measure& a, const Measure& b)
    {
        const size_t nf = std::min (a.fine.size(), b.fine.size());
        double total = 0.0;
        int count = 0;
        for (size_t f = 0; f < nf; ++f)
        {
            float top = -300.0f;
            for (size_t k = 0; k < a.fine[f].size(); ++k) top = std::max ({ top, a.fine[f][k], b.fine[f][k] });
            for (size_t k = 0; k < a.fine[f].size(); ++k)
                if (std::max (a.fine[f][k], b.fine[f][k]) > top - 45.0f)
                { total += std::min (30.0f, std::abs (a.fine[f][k] - b.fine[f][k])); ++count; }
        }
        return (float) (total / std::max (1, count));
    }

    /** Pitch and time controls: they move the sound in pitch or in time
        without necessarily moving its energy between octave bands. */
    inline bool isPitchOrTime (const juce::String& id)
    {
        static const juce::StringArray ids { "modRate", "modMotion", "modDetune", "modDepth", "modWidth", "dlyWarp",
                                             "uwRipple", "pitShift", "fxRate", "fxDiv", "fxStereo", "dlyTime",
                                             "dlyDiv", "revPre", "uwWave", "uwBubble", "uwModPhase", "uwModShape" };
        return ids.contains (id);
    }

    /** Engineering controls: legitimately subtle, because they refine how a
        process behaves rather than what it does. They must still be audible
        in a context built to exercise them, at a lower bar. */
    inline bool isEngineering (const juce::String& id)
    {
        static const juce::StringArray ids { "compScHpf", "compAutoRel", "compMode", "compKnee", "compRatio", "compAttack",
                                             "compRelease", "compDetect", "compPlace", "deAttack", "deRelease", "limiter",
                                             "uwPressResp", "mbCrossLow", "mbCrossHigh", "sigChaos" };
        return ids.contains (id) || (id.startsWith ("eq") && id.endsWith ("Q"));
    }

    inline float thresholdFor (const juce::String& id) { return isEngineering (id) ? 1.0f : 2.0f; }

    /** The settings each parameter needs around it before it can be heard:
        its section switched on, its engine selected, its band made a bell. */
    inline std::map<juce::String, float> contextFor (const juce::String& id)
    {
        std::map<juce::String, float> c;

        if (id.startsWith ("uw") || id == "character" || id.startsWith ("mac") || id.startsWith ("sig"))
        { c["engine"] = 0; c["uwMix"] = 100; }
        if (id == "macDamage")                { c["engine"] = 1; }
        if (id == "macColor" || id == "macBody") { c["engine"] = 2; }
        if (id.startsWith ("ds"))             { c["engine"] = 1; }
        if (id.startsWith ("sat"))            { c["engine"] = 2; }
        if (id.startsWith ("mod"))            { c["modOn"] = 1; c["modMix"] = 70; c["modDepth"] = 50; }
        if (id == "modMix")                   { c.erase ("modMix"); }
        if (id == "modDepth")                 { c.erase ("modDepth"); }
        if (id == "modDetune")                { c["modMode"] = 1; }                 // micro pitch
        if (id == "modRate")                  { c["modMode"] = 2; }                 // vibrato
        if (id == "pitShift")                 { c["pitMix"] = 50; }
        if (id == "pitMix")                   { c["pitShift"] = -12; }
        if (id.startsWith ("fx"))             { c["fxOn"] = 1; c["fxMode"] = 1; c["fxSync"] = 0; c["fxRate"] = 3; }
        if (id == "fxSens")                   { c["fxMode"] = 0; }                  // auto wah
        if (id == "fxDiv")                    { c["fxSync"] = 1; }
        if (id == "fxDrive")                  { c["fxMode"] = 1; c["fxReso"] = 80; }
        if (id.startsWith ("rev"))            { c["revOn"] = 1; c["revMix"] = 100; c["engine"] = 2; c["satMix"] = 0; }
        if (id == "revMix")                   { c.erase ("revMix"); }
        if (id.startsWith ("dly"))            { c["dlyOn"] = 1; c["dlyMix"] = 100; c["dlySync"] = 0; c["engine"] = 2; c["satMix"] = 0; }
        if (id == "dlyMix")                   { c.erase ("dlyMix"); }
        if (id == "dlyDiv")                   { c["dlySync"] = 1; }
        if (id.startsWith ("tr"))             { c["trOn"] = 1; }
        if (id.startsWith ("mb"))             { c["mbOn"] = 1; c["engine"] = 1; c["dsDrive"] = 70; c["mbMid"] = 9; c["mbLow"] = -9; }
        if (id == "mbMid")                    { c.erase ("mbMid"); }
        if (id == "mbLow")                    { c.erase ("mbLow"); }
        if (id.startsWith ("comp"))           { c["compOn"] = 1; c["compThresh"] = -30; c["compRatio"] = 6; }
        if (id == "compThresh")               { c.erase ("compThresh"); }
        if (id == "compRatio")                { c.erase ("compRatio"); }
        if (id == "compKnee")                 { c["compThresh"] = -22; c["compRatio"] = 12; }
        if (id == "compPlace")                { c["engine"] = 1; c["dsDrive"] = 75; }
        if (id == "compScHpf")                { c["eq2Gain"] = 15; c["eq2Freq"] = 160; }
        if (id == "compAutoRel")              { c["compRelease"] = 1000; }
        if (id.startsWith ("de"))             { c["deOn"] = 1; c["deThresh"] = -45; c["deAmount"] = 90; c["deRange"] = 18; }
        if (id == "deThresh")                 { c.erase ("deThresh"); }
        if (id == "deAmount")                 { c.erase ("deAmount"); }
        if (id == "deRange")                  { c.erase ("deRange"); }
        if (id == "ceiling")                  { c["inGain"] = 12; }
        if (id == "dryLevel")                 { c["mix"] = 50; c["engine"] = 1; }
        if (id == "wetLevel" || id == "mix")  { c["engine"] = 1; }
        if (id.startsWith ("st"))             { c["engine"] = 2; c["satMix"] = 0; }

        if (id == "limiter" || id == "truePeak") { c["inGain"] = 14; }
        if (id == "uwPressResp")              { c["uwPressure"] = 75; }
        if (id == "uwModShape" || id == "uwModPhase") { c["uwWave"] = 70; }
        if (id == "safety")                   { c["inGain"] = 14; c["limiter"] = 0; }
        if (id == "stMS")                     { c["engine"] = 0; c["uwMix"] = 100; c["uwDepth"] = 70; }
        if (id == "trOn")                     { c.erase ("trOn"); c["trAttack"] = 70; c["trBody"] = -50; }
        if (id == "mbOn")                     { c.erase ("mbOn"); }
        if (id == "modOn")                    { c.erase ("modOn"); }
        if (id == "fxOn")                     { c.erase ("fxOn"); }
        if (id == "revOn")                    { c.erase ("revOn"); }
        if (id == "dlyOn")                    { c.erase ("dlyOn"); }
        if (id == "dlySync")                  { c["dlyTime"] = 180; }
        if (id == "compOn")                   { c.erase ("compOn"); }
        if (id == "deOn")                     { c.erase ("deOn"); }
        if (id == "eqOn")                     { c["eq4Gain"] = 9; }

        if (id.startsWith ("eq") && id.endsWith ("On") && id != "eqOn")
        {
            const auto band = id.substring (2, 3);
            c["eq" + band + "Type"] = 2; c["eq" + band + "Gain"] = 9; c["eq" + band + "Freq"] = 1000;
        }
        if (id.startsWith ("eq") && id.endsWith ("Type"))
        {
            const auto band = id.substring (2, 3);
            c["eq" + band + "Gain"] = 9; c["eq" + band + "Freq"] = 1000;
        }
        if (id.startsWith ("eq") && id.endsWith ("Dyn"))
        {
            const auto band = id.substring (2, 3);
            c["eq" + band + "Type"] = 2; c["eq" + band + "Gain"] = -15; c["eq" + band + "Freq"] = 1000; c["eq" + band + "Q"] = 0.8f;
        }

        if (id.startsWith ("eq") && ! id.endsWith ("On") && ! id.endsWith ("Type") && ! id.endsWith ("Slope") && ! id.endsWith ("Dyn"))
        {
            const auto band = id.substring (2, 3);
            c["eq" + band + "Type"] = 2;                   // bell
            if (! id.endsWith ("Gain")) c["eq" + band + "Gain"] = 9;
            if (! id.endsWith ("Freq")) c["eq" + band + "Freq"] = 1500;
        }
        if (id.endsWith ("Slope"))
        {
            const auto band = id.substring (2, 3);
            c["eq" + band + "Type"] = 0;                   // high pass
            c["eq" + band + "Freq"] = 900;
        }
        return c;
    }

    /** Parameters whose effect is conditional on something a single render
        cannot supply, with the reason. Each of these is checked by its own
        dedicated test instead. */
    inline juce::String exemptionFor (const juce::String& id)
    {
        if (id == "compScAmount") return "needs an external sidechain signal; tested with one";
        if (id == "power")        return "bypass; tested sample for sample";
        if (id == "deListen")     return "monitoring switch; tested separately";
        if (id == "phase")        return "polarity only, inaudible alone; tested as an exact inversion";
        if (id == "compScExt")    return "needs an external sidechain signal; tested with one";
        if (id == "oversampling") return "must NOT change the sound; tested for invariance instead";
        if (id == "truePeak")     return "only acts on inter-sample overs; tested by measuring reconstructed peaks";
        if (id == "safety")       return "only acts above 0 dBFS; tested as a hard ceiling";
        return {};
    }

    struct Sweep { juce::String id; float diffDb; float lo, hi; float bandDb, fineDb; };

    /** Renders the parameter near both ends of its range, in its context. */
    inline Sweep sweep (const omg::ParamDesc& d, double sr, std::function<std::unique_ptr<OmgnedProcessor>()> fresh)
    {
        const juce::String id (d.id);
        const auto ctx = contextFor (id);

        auto renderAt = [&] (float plain)
        {
            auto p = fresh();
            for (auto& kv : ctx) setParam (*p, kv.first, kv.second);
            setParam (*p, id, plain);
            return render (*p, sr);
        };

        float lo, hi;
        if (d.type == omg::PType::Float)
        {
            const bool symmetric = d.min < 0.0f && std::abs (d.min + d.max) < 1.0e-3f;
            lo = symmetric ? 0.0f : d.min + (d.max - d.min) * 0.15f;
            hi = d.min + (d.max - d.min) * 0.85f;
        }
        else { lo = d.min; hi = d.max; }

        const auto a = renderAt (lo), b = renderAt (hi);
        const float band = difference (a, b);
        const float fine = isPitchOrTime (id) ? fineDifference (a, b) : 0.0f;
        return { id, std::max (band, fine), lo, hi, band, fine };
    }
}
