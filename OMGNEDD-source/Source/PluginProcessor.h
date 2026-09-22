#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "StateManager.h"
#include "Presets/PresetManager.h"
#include "DSP/Utils.h"
#include "DSP/UnderwaterEngine.h"
#include "DSP/DistortionEngine.h"
#include "DSP/SaturationEngine.h"
#include "DSP/EqSection.h"
#include "DSP/CompressorSection.h"
#include "DSP/DeEsser.h"
#include "DSP/OutputStage.h"
#include "DSP/MacroEngine.h"
#include "DSP/SpectrumSource.h"


class OmgnedProcessor : public juce::AudioProcessor
{
public:
    OmgnedProcessor();
    ~OmgnedProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "OMGNEDD"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // ---- shared with the editor -------------------------------------------
    juce::AudioProcessorValueTreeState apvts;
    omg::StateManager  stateManager  { apvts };
    omg::PresetManager presetManager { apvts };

    omg::dsp::MeterSource     inputMeter, outputMeter;
    omg::dsp::SpectrumSource  spectrum;

    float getCompressorReductionDb() const { return compressor.getReductionDb(); }
    float getDeEsserReductionDb()   const { return deEsser.getReductionDb(); }
    float getLimiterReductionDb()   const { return outputStage.getLimiterReductionDb(); }

    /** Summed EQ magnitude at one frequency, including the macro offsets the
        processor actually applied on the last block. Used to draw the curve. */
    float getEqResponseDb (float hz) { return eq.getResponseDb (hz); }

    /** Editor size, persisted with the session. */
    void setEditorSize (int w, int h);
    juce::Rectangle<int> getSavedEditorSize() const;

private:
    void pullParameters();
    int  currentOversamplingFactor() const;

    omg::dsp::UnderwaterEngine  underwater;
    omg::dsp::DistortionEngine  distortion;
    omg::dsp::SaturationEngine  saturation;
    omg::dsp::EqSection         eq;
    omg::dsp::CompressorSection compressor;
    omg::dsp::DeEsser           deEsser;
    omg::dsp::OutputStage       outputStage;

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;

    juce::AudioBuffer<float> dryBuffer;
    omg::dsp::Smooth inGainSmooth, outGainSmooth, mixSmooth;

    double currentSampleRate { 44100.0 };
    int    currentBlockSize { 512 };
    int    lastOversamplingIndex { -1 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmgnedProcessor)
};
