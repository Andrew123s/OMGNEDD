#pragma once
#include "Utils.h"

namespace omg::dsp
{
    /** The submerged character engine.

        Signal path: murk shelving -> modulated resonant low pass (1 to 4 cascaded
        one-pole-pair stages) -> pressure compression -> bubble shimmer -> engine mix.
    */
    class UnderwaterEngine
    {
    public:
        struct Params
        {
            float depth = 45, water = 50, murk = 35, pressure = 40;
            float ripple = 25, wave = 30, bubble = 20, mix = 100;
            float resonance = 1.2f, modPhaseDeg = 90.0f, pressResponse = 50.0f;
            int   slope = 1;       // 0..3 -> 12/24/36/48 dB
            int   modShape = 0;
            bool  chaos = false;
        };

        void prepare (const juce::dsp::ProcessSpec& spec)
        {
            sampleRate = spec.sampleRate;

            for (auto& ch : chans)
            {
                for (auto& f : ch.water) { f.prepare (spec); f.setType (juce::dsp::StateVariableTPTFilterType::lowpass); }
                ch.murkLow.prepare (spec);
                ch.murkHigh.prepare (spec);
                ch.bubble.prepare (spec);
                ch.bubble.setType (juce::dsp::StateVariableTPTFilterType::bandpass);
                ch.env.prepare (spec.sampleRate);
                ch.rippleLfo.prepare (spec.sampleRate);
                ch.waveLfo.prepare (spec.sampleRate);
                ch.cutoffSmooth.reset (spec.sampleRate, 0.004);
            }

            cutoffSmoothInit = false;
        }

        void reset()
        {
            for (auto& ch : chans)
            {
                for (auto& f : ch.water) f.reset();
                ch.murkLow.reset(); ch.murkHigh.reset(); ch.bubble.reset();
                ch.env.env = 0.0f;
            }
        }

        void setParams (const Params& newParams)
        {
            p = newParams;

            const float depth = pct (p.depth);
            const float water = pct (p.water);

            // deeper and wetter closes the filter down from 16 kHz to 260 Hz
            baseCutoff = juce::jlimit (180.0f, 18000.0f,
                                       16000.0f * std::pow (0.016f, depth * 0.65f + water * 0.35f));

            stages = juce::jlimit (1, 4, p.slope + 1);
            resonance = juce::jlimit (0.1f, 12.0f, p.resonance + pct (p.water) * 0.8f);

            const float murk = pct (p.murk);
            murkLowGain  = dbToGain (murk * 9.0f);
            murkHighGain = dbToGain (-murk * 14.0f);

            rippleDepth = pct (p.ripple) * 0.35f;
            waveDepth   = pct (p.wave) * 0.55f;
            bubbleAmt   = pct (p.bubble);

            const float resp = pct (p.pressResponse);
            pressAttack  = lerp (60.0f, 2.0f, resp);
            pressRelease = lerp (600.0f, 60.0f, resp);
            pressAmount  = pct (p.pressure);

            for (int c = 0; c < 2; ++c)
            {
                auto& ch = chans[(size_t) c];
                ch.rippleLfo.setFrequency (lerp (2.5, 11.0, pct (p.ripple)));
                ch.waveLfo.setFrequency   (lerp (0.05, 0.9, pct (p.wave)));
                ch.rippleLfo.setPhaseOffset (c == 1 ? p.modPhaseDeg / 360.0 : 0.0);
                ch.waveLfo.setPhaseOffset   (c == 1 ? p.modPhaseDeg / 360.0 : 0.0);
                ch.env.setTimes (pressAttack, pressRelease);

                ch.murkLow.coefficients  = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sampleRate, 220.0f, 0.7f, murkLowGain);
                ch.murkHigh.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, 3200.0f, 0.7f, murkHighGain);
                ch.bubble.setCutoffFrequency (juce::jlimit (1000.0f, 12000.0f, 5200.0f + bubbleAmt * 4000.0f));
                ch.bubble.setResonance (0.9f + bubbleAmt * 3.0f);
            }
        }

        void process (juce::AudioBuffer<float>& buffer)
        {
            const int numCh = juce::jmin (2, buffer.getNumChannels());
            const int n = buffer.getNumSamples();
            const float wet = pct (p.mix);

            for (int c = 0; c < numCh; ++c)
            {
                auto& ch = chans[(size_t) c];
                auto* d = buffer.getWritePointer (c);

                for (int i = 0; i < n; ++i)
                {
                    const float dry = d[i];
                    float x = ch.murkHigh.processSample (ch.murkLow.processSample (dry));

                    const float ripple = ch.rippleLfo.next (p.modShape) * rippleDepth;
                    const float wave   = ch.waveLfo.next (p.modShape) * waveDepth;
                    float wander = ripple + wave;

                    if (p.chaos)
                        wander += (ch.chaosRandom.nextFloat() - 0.5f) * 0.06f;

                    const float target = juce::jlimit (60.0f, 19000.0f,
                                                       baseCutoff * std::pow (2.0f, wander * 2.2f));
                    const float cutoff = ch.cutoffSmooth.next (target);

                    for (int s = 0; s < stages; ++s)
                    {
                        ch.water[(size_t) s].setCutoffFrequency (cutoff);
                        ch.water[(size_t) s].setResonance (s == 0 ? resonance : 0.707f);
                        x = ch.water[(size_t) s].processSample (c, x);
                    }

                    // pressure: downward compression with a soft knee, density on the way out
                    if (pressAmount > 0.0001f)
                    {
                        const float envDb = gainToDb (ch.env.process (std::abs (x)));
                        const float over = juce::jmax (0.0f, envDb - (-30.0f + (1.0f - pressAmount) * 24.0f));
                        const float gr = -over * (0.25f + pressAmount * 0.55f);
                        x *= dbToGain (gr);
                        x = std::tanh (x * (1.0f + pressAmount * 1.4f)) / (1.0f + pressAmount * 0.55f);
                    }

                    if (bubbleAmt > 0.0001f)
                    {
                        const float band = ch.bubble.processSample (c, x);
                        x += band * bubbleAmt * 0.6f * (0.6f + 0.4f * ripple);
                    }

                    d[i] = dry + (x - dry) * wet;
                }
            }
        }

    private:
        struct Channel
        {
            std::array<juce::dsp::StateVariableTPTFilter<float>, 4> water;
            juce::dsp::IIR::Filter<float> murkLow, murkHigh;
            juce::dsp::StateVariableTPTFilter<float> bubble;
            EnvFollower env;
            Lfo rippleLfo, waveLfo;
            Smooth cutoffSmooth;
            juce::Random chaosRandom;
        };

        std::array<Channel, 2> chans;
        Params p;
        double sampleRate { 44100.0 };
        float baseCutoff { 4000.0f }, resonance { 1.0f };
        float murkLowGain { 1.0f }, murkHighGain { 1.0f };
        float rippleDepth { 0.0f }, waveDepth { 0.0f }, bubbleAmt { 0.0f };
        float pressAmount { 0.0f }, pressAttack { 20.0f }, pressRelease { 200.0f };
        int stages { 2 };
        bool cutoffSmoothInit { false };
    };
}
