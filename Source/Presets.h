#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

// A factory preset = name + data file. Presets live as data (XML files), never
// as compiled code: shipped files load from the install, per-user overwrites in
// Documents shadow them. Filenames: "NNN Name [Bank].xml".
struct FactoryPreset
{
    juce::String name;
    juce::File file;
    int index = -1;
    bool shadowed = false; // user-overwritten copy shadows the shipped file
    int bank = 0;          // effective bank 1..6 (tag or index fallback)
};

class MonoProcessor;

const std::vector<FactoryPreset>& getFactoryPresets(); // sorted by index
void rescanFactoryPresets();
void setFactoryDirsForTest(const juce::File& bundleDir, const juce::File& overrideDir);
juce::File factoryOverrideDir();
juce::String presetName(int index);
juce::String bankName(int bank); // 1..6 -> "Bass".."FX", else empty
bool applyPreset(MonoProcessor& proc, int index); // factory by index
bool overwriteFactoryPreset(int index, MonoProcessor& proc);
bool deleteFactoryShadow(int index); // restores the shipped file (true if one existed)

// ---- loading / saving state files (factory + user share the format) ----
bool loadPresetFile(MonoProcessor& proc, const juce::File& file);
bool savePresetFile(MonoProcessor& proc, const juce::File& file);

// ---- user presets (XML files on disk) ----
struct UserPreset { juce::String name; juce::File file; };
void setUserPresetDirForTest(const juce::File& dir); // harness override, else Documents
juce::File getUserPresetDir();
std::vector<UserPreset> scanUserPresets();
juce::String sanitisePresetName(juce::String name);
bool saveUserPreset(const juce::String& name, MonoProcessor& proc);
bool loadUserPreset(const juce::File& file, MonoProcessor& proc);
bool deleteUserPreset(const juce::File& file);
