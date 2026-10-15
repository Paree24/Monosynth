#include "Presets.h"
#include "PluginProcessor.h"

// File read/write operations that need the live processor (kept separate so
// the factory scanning code stays processor-free).

bool loadPresetFile(MonoProcessor& proc, const juce::File& file)
{
    auto xml = juce::parseXML(file);
    if (xml == nullptr) return false;
    auto vt = juce::ValueTree::fromXml(*xml);
    if (! vt.isValid() || ! vt.hasType(proc.apvts.state.getType())) return false;
    return proc.loadPresetState(vt);
}

bool savePresetFile(MonoProcessor& proc, const juce::File& file)
{
    if (auto xml = proc.presetState().createXml())
        return xml->writeTo(file);
    return false;
}

bool applyPreset(MonoProcessor& proc, int index)
{
    for (auto& pr : getFactoryPresets())
        if (pr.index == index)
        {
            if (! loadPresetFile(proc, pr.file)) return false;
            proc.setPresetIdentity(index, pr.name);
            return true;
        }
    return false;
}

bool overwriteFactoryPreset(int index, MonoProcessor& proc)
{
    const FactoryPreset* row = nullptr;
    for (auto& pr : getFactoryPresets())
        if (pr.index == index) row = &pr;
    if (row == nullptr) return false;
    auto dir = factoryOverrideDir();
    if (! dir.isDirectory() && ! dir.createDirectory()) return false;
    auto file = dir.getChildFile(row->file.getFileName());
    if (file.exists() && ! file.deleteFile()) return false;
    proc.setPresetIdentity(index, row->name); // keep the slotted name/bank on overwrite
    bool ok = savePresetFile(proc, file);
    if (ok) rescanFactoryPresets();
    return ok;
}

bool saveUserPreset(const juce::String& name, MonoProcessor& proc)
{
    auto clean = sanitisePresetName(name);
    auto dir = getUserPresetDir();
    if (! dir.isDirectory() && ! dir.createDirectory()) return false;
    auto file = dir.getChildFile(clean + ".xml");
    if (file.exists() && ! file.deleteFile()) return false;
    proc.setPresetIdentity(-1, clean); // now a user sound, not a factory slot
    return savePresetFile(proc, file);
}

bool loadUserPreset(const juce::File& file, MonoProcessor& proc)
{
    if (! loadPresetFile(proc, file)) return false;
    proc.setPresetIdentity(-1, file.getFileNameWithoutExtension());
    return true;
}
