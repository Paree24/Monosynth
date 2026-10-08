cmake_minimum_required(VERSION 3.22)
project(Monosynth VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Prefer the local JUCE checkout (fast, pinned). Override with -DJUCE_DIR=...
if(NOT DEFINED JUCE_DIR)
  set(JUCE_DIR "/home/paree/ai_experiments/Nanoberg/JUCE")
endif()
if(EXISTS "${JUCE_DIR}/CMakeLists.txt")
  add_subdirectory(${JUCE_DIR} ${CMAKE_CURRENT_BINARY_DIR}/JUCE)
else()
  include(FetchContent)
  FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.6
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(JUCE)
endif()

juce_add_binary_data(MonoAssets
  HEADER_NAME MonoAssets.h
  NAMESPACE MonoAssets
  SOURCES
    Assets/Lato-Regular.ttf
    Assets/Lato-Bold.ttf
    Assets/Lato-Black.ttf
)

juce_add_plugin(Monosynth
  COMPANY_NAME "Paree"
  BUNDLE_ID "com.paree.monosynth"
  PLUGIN_MANUFACTURER_CODE Pare
  PLUGIN_CODE Mono
  FORMATS VST3 Standalone
  PRODUCT_NAME "Monosynth"
  DESCRIPTION "Monophonic retro analog synthesizer: dual VCO, cascade HPF-LPF filter, mod matrix"
  NEEDS_MIDI_INPUT TRUE
  NEEDS_MIDI_OUTPUT FALSE
  IS_SYNTH TRUE
  COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(Monosynth)

target_sources(Monosynth PRIVATE
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/PresetBrowser.cpp
  Source/Presets.cpp
)

target_include_directories(Monosynth PRIVATE Source)

target_compile_definitions(Monosynth PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
  JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Monosynth PRIVATE
  juce::juce_audio_utils
  juce::juce_dsp
  juce_audio_plugin_client
  MonoAssets
)

# Headless DSP self-test harness (links the real DSP + preset code, no GUI)
juce_add_console_app(MonoHarness PRODUCT_NAME "MonoHarness")
target_sources(MonoHarness PRIVATE
  Test/Harness.cpp
  Source/PluginProcessor.cpp
  Source/PluginEditor.cpp
  Source/PresetBrowser.cpp
  Source/Presets.cpp
)
target_include_directories(MonoHarness PRIVATE Source)
target_compile_definitions(MonoHarness PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
)
target_link_libraries(MonoHarness PRIVATE
  juce::juce_audio_utils
  juce::juce_dsp
  MonoAssets
)

# PresetDump: recipe -> Factory/*.xml data-file generator (never shipped)
juce_add_console_app(PresetDump PRODUCT_NAME "PresetDump")
target_sources(PresetDump PRIVATE
  Test/Dump.cpp
  Source/PresetRecipes.cpp
)
target_include_directories(PresetDump PRIVATE Source)
target_compile_definitions(PresetDump PRIVATE
  JUCE_WEB_BROWSER=0
  JUCE_USE_CURL=0
)
target_link_libraries(PresetDump PRIVATE
  juce::juce_core
  juce::juce_audio_processors
  juce::juce_data_structures
)

# Ship factory presets inside the VST3 bundle (runtime resolves them
# bundle-relative, with repo/dev fallbacks when run uninstalled).
# Always-target (not POST_BUILD): data-only preset changes refresh the bundle
# even when no source relinks.
add_custom_target(MonoFactory ALL
  COMMAND ${CMAKE_COMMAND} -E remove_directory
    "${CMAKE_CURRENT_BINARY_DIR}/Monosynth_artefacts/$<CONFIG>/VST3/Monosynth.vst3/Contents/Resources/Factory"
  COMMAND ${CMAKE_COMMAND} -E make_directory
    "${CMAKE_CURRENT_BINARY_DIR}/Monosynth_artefacts/$<CONFIG>/VST3/Monosynth.vst3/Contents/Resources/Factory"
  COMMAND ${CMAKE_COMMAND} -E copy_directory
    "${CMAKE_CURRENT_SOURCE_DIR}/Factory"
    "${CMAKE_CURRENT_BINARY_DIR}/Monosynth_artefacts/$<CONFIG>/VST3/Monosynth.vst3/Contents/Resources/Factory"
)
