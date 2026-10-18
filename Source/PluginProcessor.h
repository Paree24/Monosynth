#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Parameters.h"
#include "DSP.h"
#include "FX.h"
#include "Presets.h"
#include "MidiArp.h"
#include "SnapshotUndo.h"

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
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override
    {
        return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
    }

    int getNumPrograms() override { return juce::jmax(1, (int)getFactoryPresets().size()); }
    int getCurrentProgram() override { return juce::jmax(0, currentPreset.load()); }
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

    // Current sound identity (message thread). Factory slot index, or -1 for
    // a user/init sound; the name is what the header shows and what gets
    // saved with the session.
    int getFactorySlot() const { return currentPreset.load(); }
    juce::String getCurrentPresetName() const { return currentName; }
    void setPresetIdentity(int factoryIndex, const juce::String& name);

    // Preset file content = APVTS state + name (factory + user share it).
    juce::ValueTree presetState();
    bool loadPresetState(const juce::ValueTree& vt);

    // ---- undo / redo (message thread) ----
    // Knob / switch gestures are one step each; preset loads, Init and
    // randomize / mutate wrap their parameter writes in undoStep().
    std::unique_ptr<SnapshotUndo::Transaction> undoStep()
    {
        return undo != nullptr ? std::make_unique<SnapshotUndo::Transaction>(*undo) : nullptr;
    }
    bool canUndo() const { return undo != nullptr && undo->canUndo(); }
    bool canRedo() const { return undo != nullptr && undo->canRedo(); }
    void undoEdit() { if (undo != nullptr) undo->undo(); }
    void redoEdit() { if (undo != nullptr) undo->redo(); }

    // ---- randomize / mutate with group locks (one undo step each) ----
    // Never touched: volume, VCA level / initial gain, limiter, glide and
    // bend / velocity settings (the playing feel). Arp is locked by default.
    enum RandGroup { RG_VCO, RG_MIXER, RG_FILTER, RG_ENV, RG_MG, RG_MATRIX, RG_FX, RG_ARP, RG_COUNT };
    static juce::StringArray randGroupNames() { return { "VCOs", "Mixer", "Filter", "Envelopes", "MG", "Matrix", "Effects", "Arp" }; }
    static int paramGroup(const juce::String& id); // -1 = never randomized
    bool isGroupLocked(int g) const { return (getRandLocks() >> g) & 1; }
    void setGroupLocked(int g, bool locked);
    void randomize();
    void mutate(float amount = 0.04f); // a small nudge (normalised range)

private:
    MonoDSP::SynthParams collectParams();
    void handleMidi(const juce::MidiMessage& msg, const MonoDSP::SynthParams& p);
    void updateHeld(int note, bool on);
    void allNotesOff();
    void resetParamsToDefaults();

    // raw parameter pointers, resolved once (no string lookups per block)
    struct ParamPtrs
    {
        std::atomic<float>* vco1wave, *vco1oct, *vco1pitch, *vco1tune, *vco1pw;
        std::atomic<float>* vco2wave, *vco2oct, *vco2pitch, *vco2tune, *vco2pw;
        std::atomic<float>* mix1, *mix2, *ring, *noise, *thick, *noiseType;
        std::atomic<float>* hpfCut, *hpfPeak, *lpfCut, *lpfRes, *drive, *fmodel, *satAmt, *satType;
        std::atomic<float>* vca, *volume, *limiter, *porta, *portaOn, *glideMode;
        std::atomic<float>* mgWave, *mgRate, *mgSync;
        std::atomic<float>* eg1a, *eg1d, *eg1s, *eg1r, *eg2a, *eg2d, *eg2s, *eg2r;
        std::atomic<float>* bendRange, *velSens;
        std::atomic<float>* eg1Delay, *eg2Hold, *vcaInit;
        std::atomic<float>* arpOn, *arpMode, *arpDiv, *arpOct, *arpGate, *arpSwing, *arpLatch;
        std::atomic<float>* chOn, *chRate, *chDepth, *dlOn, *dlTime, *dlFb, *dlMix, *dlSync, *dlDiv, *rvOn, *rvSize, *rvMix;
        std::atomic<float>* modSrc[P::NMOD], *modAmt[P::NMOD], *modDst[P::NMOD], *modTyp[P::NMOD];
    } pp {};

    int getRandLocks() const { return (int) apvts.state.getProperty("randLocks", 1 << RG_ARP); }
    std::unique_ptr<SnapshotUndo> undo;

    MonoDSP::MonoVoice voice;
    MidiArp arp;
    MonoDSP::ModGen mg;
    MonoFX::ChorusFX chorus;
    MonoFX::DelayFX delay;
    MonoFX::ModVerb verb;
    juce::Random rng;

    juce::dsp::IIR::Filter<float> dcBlock[2]; // IIR::Filter is mono: one per channel
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
    bool fxWasOn[3] = { false, false, false }; // chorus, delay, reverb (clear stale tails on enable)

    // GUI-thread requests serviced at the top of processBlock
    std::atomic<bool> pendingAllOff { false }, pendingPanic { false };

    std::atomic<int> currentPreset { 0 };
    juce::String currentName { "Init Patch" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MonoProcessor)
};
