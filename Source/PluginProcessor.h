#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "Parameters.h"
#include "StateManager.h"
#include "Presets/PresetManager.h"
#include "DSP/Utils.h"
#include "DSP/Filters.h"
#include "DSP/Tempo.h"
#include "DSP/UnderwaterEngine.h"
#include "DSP/DistortionEngine.h"
#include "DSP/SaturationEngine.h"
#include "DSP/EqSection.h"
#include "DSP/CompressorSection.h"
#include "DSP/DeEsser.h"
#include "DSP/OutputStage.h"
#include "DSP/MacroEngine.h"
#include "DSP/SpectrumSource.h"
#include "DSP/ModulationEngine.h"
#include "DSP/MultibandDrive.h"
#include "DSP/TransientShaper.h"
#include "DSP/FilterFx.h"
#include "DSP/SpaceEngine.h"
#include "Randomizer.h"


class OmgnedProcessor : public juce::AudioProcessor
{
public:
    OmgnedProcessor();
    ~OmgnedProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    /** The host's own bypass. The plugin reports latency, so bypassed audio
        is delayed by exactly that much: toggling bypass never shifts the
        vocal in time against the rest of the mix. */
    void processBlockBypassed (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "OMGNEDD"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }

    /** The reverb and delay ring on after the input stops. */
    double getTailLengthSeconds() const override { return 8.0; }

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
    omg::Randomizer    randomizer    { apvts };

    omg::dsp::MeterSource     inputMeter, outputMeter;
    omg::dsp::SpectrumSource  spectrum;

    float getCompressorReductionDb() const { return compressor.getReductionDb(); }
    float getDeEsserReductionDb()   const { return deEsser.getReductionDb(); }
    float getLimiterReductionDb()   const { return outputStage.getLimiterReductionDb(); }

    /** Peak level measured going into the character engine, in dB. */
    float getDriveDb() const { return driveMeterDb.load(); }

    bool isSidechainConnected() const { return sidechainConnected.load(); }

    /** Summed EQ magnitude at one frequency; safe from the message thread. */
    float getEqResponseDb (float hz) { return eq.getResponseDb (hz); }

    /** The tempo the synced controls are following, for the panel. */
    double getCurrentBpm() const { return currentBpm.load(); }

    void setEditorSize (int w, int h);
    juce::Rectangle<int> getSavedEditorSize() const;

    /** For the verification target: lets a test supply the transport a host
        would, so tempo sync can be measured without a host. */
    void setTransportOverride (const omg::dsp::TransportInfo& t) { transportOverride = t; hasTransportOverride = true; }

private:
    void pullParameters (const omg::dsp::TransportInfo&);
    void runCharacterChain (juce::AudioBuffer<float>& buffer, int osIndex);
    void applyEngineRate (int osIndex);
    void processEngine (int engine, juce::AudioBuffer<float>& b);
    void resetEngine (int engine);
    omg::dsp::TransportInfo readTransport();

    omg::dsp::UnderwaterEngine  underwater;
    omg::dsp::DistortionEngine  distortion;
    omg::dsp::SaturationEngine  saturation;
    omg::dsp::MultibandDrive    multiband;
    omg::dsp::ModulationEngine  modulation;
    omg::dsp::TransientShaper   transient;
    omg::dsp::FilterFx          filterFx;
    omg::dsp::SpaceEngine       space;
    omg::dsp::EqSection         eq;
    omg::dsp::CompressorSection compressor;
    omg::dsp::DeEsser           deEsser;
    omg::dsp::OutputStage       outputStage;

    std::array<std::unique_ptr<juce::dsp::Oversampling<float>>, 3> oversamplers;

    juce::AudioBuffer<float> dryBuffer, osScratch, xfadeScratch, sidechainCopy, sideStash;

    /** The oversampler delays the engine path. Anything mixed back against it
        (the global dry signal, and the side channel in M/S mode) is delayed by
        the same amount, or partial MIX settings comb-filter. */
    std::array<omg::dsp::DelayLine, 2> dryAlign, bypassAlign;
    omg::dsp::DelayLine sideAlign;

    /** Passes the main bus through, delayed by the reported latency. */
    void passThroughAligned (juce::AudioBuffer<float>& work);

    omg::dsp::Smooth inGainSmooth, outGainSmooth, mixSmooth, dryLevelSmooth, wetLevelSmooth;
    std::atomic<float> driveMeterDb { -100.0f };
    std::atomic<bool>  sidechainConnected { false };
    std::atomic<double> currentBpm { 120.0 };

    double currentSampleRate { 44100.0 };
    int    currentBlockSize { 512 };
    int    lastOsIndex { -1 };
    float  engineLatency { 0.0f };        // exact, fractional; the host gets it rounded

    // engine switching: both run for a short equal-power crossfade
    int currentEngine { -1 }, previousEngine { -1 };
    int xfadeLeft { 0 }, xfadeTotal { 1 };

    // how much of the underwater wash is live, following engine switches
    float underwaterWeight { 0.0f };
    float washAmount { 0.0f };
    bool  midSideEngine { false };

    omg::dsp::TransportInfo transportOverride;
    bool hasTransportOverride { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (OmgnedProcessor)
};
