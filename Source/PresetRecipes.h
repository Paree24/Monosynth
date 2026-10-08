#pragma once
#include <juce_core/juce_core.h>
#include <map>
#include <vector>
#include <string>

// Dump-time only: deterministic recipe builders used by the PresetDump tool to
// generate the Factory/*.xml data files. Never linked into the plugin — the
// plugin only reads data files at runtime.
struct RecipePreset
{
    std::string fileName; // e.g. "000 Deep Sub [Bass]"
    std::map<std::string, float> values; // paramID -> real-unit value
};

std::vector<RecipePreset> buildRecipePresets();
