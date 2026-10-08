#pragma once
#include <juce_audio_processors/juce_audio_processors.h>

// Monosynth parameter IDs. Stable, never generated from display names.
namespace P
{
    // VCOs (wave: 0 Saw 1 Triangle 2 Square 3 Pulse 4 Sine)
    static constexpr const char* VCO1_WAVE = "vco1wave";
    static constexpr const char* VCO1_OCT  = "vco1oct";   // -2..+2 choice
    static constexpr const char* VCO1_PITCH= "vco1pitch"; // -12..+12 st
    static constexpr const char* VCO1_TUNE = "vco1tune";  // -100..+100 c
    static constexpr const char* VCO1_PW   = "vco1pw";    // 0.05..0.95
    // VCO2
    static constexpr const char* VCO2_WAVE = "vco2wave";
    static constexpr const char* VCO2_OCT  = "vco2oct";
    static constexpr const char* VCO2_PITCH= "vco2pitch";
    static constexpr const char* VCO2_TUNE = "vco2tune";
    static constexpr const char* VCO2_PW   = "vco2pw";
    // Mixer
    static constexpr const char* MIX_VCO1 = "mixvco1";
    static constexpr const char* MIX_VCO2 = "mixvco2";
    static constexpr const char* MIX_RING = "mixring";
    static constexpr const char* MIX_NOISE= "mixnoise";
    static constexpr const char* THICK     = "thick";     // 0..1 osc-bus soft saturation
    static constexpr const char* NOISE_TYPE= "noisetype"; // 0 White 1 Pink
    // Filter (cascade MIX -> HPF -> LPF). Model: 0 Early 1 Late.
    static constexpr const char* HPF_CUT  = "hpfcut";   // Hz
    static constexpr const char* HPF_PEAK = "hpfpeak";  // 0..1
    static constexpr const char* LPF_CUT  = "lpfcut";   // Hz
    static constexpr const char* LPF_RES  = "lpfres";   // 0..1
    static constexpr const char* DRIVE    = "drive";    // 0..1
    static constexpr const char* FMODEL   = "fmodel";   // 0 Early 1 Late
    static constexpr const char* SAT_AMT  = "satamt";   // 0..1 saturation amount
    static constexpr const char* SAT_TYPE = "sattype";  // 0 Tape 1 Tube 2 Fold 3 Fuzz
    // VCA / output
    static constexpr const char* VCA_LEVEL = "vcalevel";
    static constexpr const char* VOLUME    = "volume";
    static constexpr const char* LIMITER   = "limiter";   // master limiter on/off
    static constexpr const char* PORTA     = "porta";     // 0..1 mapped to time
    static constexpr const char* PORTA_ON  = "portaon";   // bool
    static constexpr const char* GLIDE_MODE= "glidemode"; // 0 Legato 1 Always
    // Modulation generator (LFO)
    static constexpr const char* MG_WAVE = "mgwave";  // 0 Tri 1 Saw 2 Square 3 Sine 4 S&H
    static constexpr const char* MG_RATE = "mgrate";
    static constexpr const char* MG_SYNC = "mgsync";  // bool tempo sync
    // Envelopes
    static constexpr const char* EG1_A = "eg1a"; static constexpr const char* EG1_D = "eg1d";
    static constexpr const char* EG1_S = "eg1s"; static constexpr const char* EG1_R = "eg1r";
    static constexpr const char* EG2_A = "eg2a"; static constexpr const char* EG2_D = "eg2d";
    static constexpr const char* EG2_S = "eg2s"; static constexpr const char* EG2_R = "eg2r";
    // Global
    static constexpr const char* BEND_RANGE = "bendrange"; // 0..12 semitones
    static constexpr const char* VELSENS    = "velsens";   // 0..1
    // Master FX (simple controls; all off by default)
    static constexpr const char* CH_ON    = "chon";
    static constexpr const char* CH_RATE  = "chrate";   // 0.05..8 Hz
    static constexpr const char* CH_DEPTH = "chdepth";  // 0..1
    static constexpr const char* DL_ON    = "dlon";
    static constexpr const char* DL_TIME  = "dltime";   // ms
    static constexpr const char* DL_FB    = "dlfb";     // 0..0.85
    static constexpr const char* DL_MIX   = "dlmix";    // 0..1
    static constexpr const char* RV_ON    = "rvon";
    static constexpr const char* RV_SIZE  = "rvsize";   // 0..1
    static constexpr const char* RV_MIX   = "rvmix";     // 0..1

    // Mod matrix: 8 slots x (source, amount, dest, type)
    // source: 0 None 1 MG 2 EG1 3 EG2 4 Velocity 5 Aftertouch 6 KeyTrack 7 ModWheel 8 PitchWheel 9 Random 10 NoteOn
    // dest: 0 None 1 VCO1Pitch 2 VCO2Pitch 3 VCO1PW 4 VCO2PW 5 HPFCut 6 LPFCut 7 HPFPeak 8 LPFRes 9 VCALevel
    // type: 0 Bipolar 1 Unipolar
    static constexpr int NMOD = 8;
    inline juce::String modSrcId (int i) { return "mod" + juce::String(i) + "src"; }
    inline juce::String modAmtId (int i) { return "mod" + juce::String(i) + "amt"; }
    inline juce::String modDstId (int i) { return "mod" + juce::String(i) + "dst"; }
    inline juce::String modTypeId(int i) { return "mod" + juce::String(i) + "typ"; }
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
