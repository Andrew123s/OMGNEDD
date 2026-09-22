#pragma once
#include "Utils.h"
#include <array>

namespace omg::dsp
{
    /** Real FFT feeding the analyser behind the EQ curve.

        The audio thread pushes samples into a lock-free fifo; the editor pulls a
        smoothed, log-spaced magnitude set on its own timer. Nothing is drawn that
        the processor did not actually measure.
    */
    class SpectrumSource
    {
    public:
        static constexpr int fftOrder = 11;              // 2048
        static constexpr int fftSize  = 1 << fftOrder;
        static constexpr int numBins  = fftSize / 2;
        static constexpr int numPoints = 128;            // what the editor draws

        SpectrumSource() : fft (fftOrder),
                           window (fftSize, juce::dsp::WindowingFunction<float>::hann)
        {
            fifo.fill (0.0f);
            fftData.fill (0.0f);
            points.fill (0.0f);
        }

        void prepare (double sr) { sampleRate = sr; fifoIndex = 0; ready.store (false); }

        void push (const float* data, int numSamples)
        {
            for (int i = 0; i < numSamples; ++i)
            {
                fifo[(size_t) fifoIndex++] = data[i];

                if (fifoIndex == fftSize)
                {
                    if (! ready.load())
                    {
                        std::copy (fifo.begin(), fifo.end(), fftData.begin());
                        std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
                        ready.store (true);
                    }
                    fifoIndex = 0;
                }
            }
        }

        /** Called from the editor's timer. Returns true when the points changed. */
        bool updatePoints()
        {
            if (! ready.load())
                return false;

            window.multiplyWithWindowingTable (fftData.data(), fftSize);
            fft.performFrequencyOnlyForwardTransform (fftData.data());

            const float minHz = 20.0f, maxHz = 20000.0f;
            const float binHz = (float) sampleRate / (float) fftSize;

            for (int i = 0; i < numPoints; ++i)
            {
                const float t  = (float) i / (float) (numPoints - 1);
                const float hz = minHz * std::pow (maxHz / minHz, t);
                const int   b  = juce::jlimit (1, numBins - 1, (int) std::round (hz / binHz));

                const float db  = gainToDb (fftData[(size_t) b] / (float) numBins * 2.0f);
                const float norm = juce::jlimit (0.0f, 1.0f, (db + 90.0f) / 90.0f);

                points[(size_t) i] = juce::jmax (norm, points[(size_t) i] * 0.82f);   // slow visual fall
            }

            ready.store (false);
            return true;
        }

        const std::array<float, numPoints>& getPoints() const noexcept { return points; }

    private:
        juce::dsp::FFT fft;
        juce::dsp::WindowingFunction<float> window;
        std::array<float, fftSize>      fifo {};
        std::array<float, fftSize * 2>  fftData {};
        std::array<float, numPoints>    points {};
        int fifoIndex { 0 };
        double sampleRate { 44100.0 };
        std::atomic<bool> ready { false };
    };
}
