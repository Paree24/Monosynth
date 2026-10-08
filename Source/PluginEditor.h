#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Parameters.h"

class MonoProcessor;
class PresetBrowser;

namespace MonoColors
{
    static const juce::Colour bg        { 0xff151515 };
    static const juce::Colour panel     { 0xff202020 };
    static const juce::Colour raised    { 0xff292929 };
    static const juce::Colour control   { 0xff343434 };
    static const juce::Colour border    { 0xff4a4a4a };
    static const juce::Colour text      { 0xffe8e8e8 };
    static const juce::Colour textDim   { 0xffa8a8a8 };
    static const juce::Colour track     { 0xff3a3a3a };
    static const juce::Colour blue      { 0xffb4b4b4 };
    static const juce::Colour mint      { 0xffbdbdbd };
    static const juce::Colour yellow    { 0xffc6c6c6 };
    static const juce::Colour pink      { 0xffafafaf };
    static const juce::Colour lavender  { 0xffcfcfcf };
}

class MonoLNF : public juce::LookAndFeel_V4
{
public:
    MonoLNF();
    juce::Font uiFont(float h) const;
    juce::Font titleFont(float h) const;
    juce::Font logoFont(float h) const;
    static juce::Colour accentFor(juce::Component& c);

    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont(juce::TextButton&, int) override;
    void drawRotarySlider(juce::Graphics&, int x, int y, int w, int h, float pos,
                          float start, float end, juce::Slider&) override;
    void drawLinearSlider(juce::Graphics&, int x, int y, int w, int h, float pos,
                          float min, float max, juce::Slider::SliderStyle, juce::Slider&) override;
    void drawToggleButton(juce::Graphics&, juce::ToggleButton&, bool, bool) override;
    void drawComboBox(juce::Graphics&, int w, int h, bool, int, int, int, int, juce::ComboBox&) override;
    void drawGroupComponentOutline(juce::Graphics&, int w, int h, const juce::String&,
                                   const juce::Justification&, juce::GroupComponent&) override;
    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour&, bool, bool) override;
    void drawPopupMenuBackground(juce::Graphics&, int w, int h) override;
private:
    juce::Typeface::Ptr latoReg, latoBold, latoBlack;
};

// Small rotary amount knob used in the matrix (attached to -1..+1 params).
class AmountKnob : public juce::Component
{
public:
    AmountKnob(MonoProcessor& p, const juce::String& paramId, MonoLNF& lnf);
    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseDrag(const juce::MouseEvent& e) override;
    void mouseDoubleClick(const juce::MouseEvent&) override;
    float getValue() const { return value; }
    void setValue(float v, bool notify);
    void setQuiet(float v) { value = juce::jlimit(-1.0f, 1.0f, v); repaint(); }
    std::function<void(float)> onChange;
private:
    void syncFromParam();
    MonoProcessor& proc;
    juce::String pid;
    MonoLNF& lnf;
    float value = 0.0f;
    double dragStartY = 0.0;
    float dragStartV = 0.0f;
};

class MonoContent : public juce::Component, private juce::Timer
{
public:
    explicit MonoContent(MonoProcessor& p);
    ~MonoContent() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
    void refreshPresetName();
    static constexpr int baseW = 1280, baseH = 832;
private:
    void timerCallback() override;
    juce::GroupComponent* addGroup(const juce::String& title, int x, int y, int w, int h, juce::Colour accent);
    juce::Slider* addKnob(const juce::String& param, const juce::String& label, juce::Component* parent,
                          int x, int y, int w = 64, int h = 78);
    juce::Label* addCaption(const juce::String& text, juce::Component* parent, int x, int y, int w);
    juce::ComboBox* addChoice(const juce::String& param, juce::Component* parent, int x, int y, int w, int h = 24);
    juce::ToggleButton* addToggle(const juce::String& param, const juce::String& label, juce::Component* parent,
                                  int x, int y, int w, int h);
    void addWaveButtons(const juce::String& param, juce::Component* parent, int x, int y, int bw = 36);
    void addOctButtons(const juce::String& param, juce::Component* parent, int x, int y, int bw = 34);
    void openSaveDialog();
    MonoProcessor& proc;
    MonoLNF lnf;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, nameBtn { "Init Patch" };
    juce::TextButton saveBtn { "S" }, folderBtn { "F" }, initBtn { "INIT" };
    juce::Slider* volTop = nullptr;
    std::vector<std::unique_ptr<juce::GroupComponent>> groups;
    std::vector<std::unique_ptr<juce::Slider>> knobs;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::ComboBox>> boxes;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<juce::TextButton>> waveBtns;
    struct WaveBtnInfo { juce::TextButton* btn = nullptr; juce::String param; int index = 0; };
    std::vector<WaveBtnInfo> waveBtnInfos;
    std::vector<std::unique_ptr<AmountKnob>> amtKnobs;
    std::vector<std::unique_ptr<juce::Label>> amtLabels;
    std::unique_ptr<PresetBrowser> browser;
    juce::String customName; // user-preset display name (empty = follow processor)
    juce::OwnedArray<juce::AudioProcessorValueTreeState::SliderAttachment> sAtt;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ComboBoxAttachment> cAtt;
    juce::OwnedArray<juce::AudioProcessorValueTreeState::ButtonAttachment> bAtt;
};

class MonoEditor : public juce::AudioProcessorEditor
{
public:
    explicit MonoEditor(MonoProcessor& p);
    ~MonoEditor() override;
    void paint(juce::Graphics& g) override;
    void resized() override;
private:
    MonoContent content;
    juce::ResizableCornerComponent corner;
};
