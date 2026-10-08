#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"
#include "DSP.h"
#include "FX.h"
#include "Presets.h"

class MonoProcessor : public juce::AudioProcessor
{
public:
    MonoProcessor();
    ~MonoProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Monosynth"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }

    int getNumPrograms() override { return juce::jmax(1, (int)getFactoryPresets().size()); }
    int getCurrentProgram() override { return currentPreset; }
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    juce::AudioProcessorValueTreeState apvts;
    void loadPreset(int index);
    void loadInit();
    void panic();
    juce::String getPresetName(int i) const;
    bool showInit = false; // true after loadInit: UI shows "Init Patch"

private:
    MonoDSP::SynthParams collectParams();
    void handleMidi(const juce::MidiMessage& msg);
    void updateHeld(int note, bool on);

    MonoDSP::MonoVoice voice;
    MonoDSP::ModGen mg;
    MonoFX::ChorusFX chorus;
    MonoFX::DelayFX delay;
    MonoFX::ModVerb verb;
    juce::Random rng;

    juce::dsp::IIR::Filter<float> dcBlock;
    juce::dsp::ProcessSpec spec {};
    double cachedSr = 44100.0;

    std::vector<int> heldNotes;
    float bendSemis = 0.0f, bendSm = 0.0f;
    float modWheel = 0.0f, aftertouch = 0.0f, pitchWheelSrc = 0.0f;
    float mgSmooth = 0.0f;
    double hostBpm = 120.0;

    // smoother states (base params only; matrix adds on top per-sample)
    float smHpf = 30.0f, smHpfPk = 0.1f, smLpf = 2500.0f, smLpfRs = 0.2f;
    float smDrv = 0.2f, smSat = 0.0f, smThick = 0.3f, smM1 = 0.8f, smM2 = 0.0f, smRing = 0.0f, smNz = 0.0f;
    float smVol = 0.5f, smVca = 0.8f;
    float limGain = 1.0f, limPeak = 0.0f;
    int idleSamples = 0; float gateG = 1.0f;

    int currentPreset = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MonoProcessor)
};
