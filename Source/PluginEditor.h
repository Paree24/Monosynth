#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "Parameters.h"
#include "PastelViews.h"

class MonoProcessor;
class PresetBrowser;

// v3 design (after Pastel EFX Grid; see mockup/redesign_v3.png): fully
// monochrome. Ivory on charcoal, dot + title panel headers, arc knobs with
// readouts, live views you drag.
namespace MonoColors
{
    static const juce::Colour bg       { 0xff141518 };
    static const juce::Colour panel    { 0xff1c1f24 };
    static const juce::Colour inner    { 0xff16181c };
    static const juce::Colour grid     { 0xff24272c };
    static const juce::Colour raised   { 0xff232529 };
    static const juce::Colour control  { 0xff232529 };
    static const juce::Colour border   { 0xff33363c };
    static const juce::Colour edge     { 0xff3a3d43 };
    static const juce::Colour text     { 0xffd9d5d0 };
    static const juce::Colour textDim  { 0xff8f8b85 };
    static const juce::Colour faint    { 0xff5c5a56 };
    static const juce::Colour track    { 0xff33363c };
    static const juce::Colour ivory    { 0xffe6dcc8 }; // the one accent
    static const juce::Colour handle   { 0xfff4f1ec };
    static const juce::Colour ink      { 0xff1b1d22 }; // text on a filled accent
}

class MonoLNF : public juce::LookAndFeel_V4
{
public:
    MonoLNF();
    juce::Font uiFont(float h) const;
    juce::Font titleFont(float h) const;
    juce::Font logoFont(float h) const;
    static juce::Colour accentFor(juce::Component& c) { return pastel::accentFor(c); }

    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getComboBoxFont(juce::ComboBox&) override;
    juce::Font getPopupMenuFont() override;
    juce::Font getTextButtonFont(juce::TextButton&, int) override;
    void positionComboBoxText(juce::ComboBox&, juce::Label&) override;
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
    juce::GroupComponent* addGroup(const juce::String& title, int x, int y, int w, int h);
    // knob centred on (cx, cy), radius r; name + live value underneath (or beside)
    juce::Slider* addKnob(const juce::String& param, const juce::String& label, juce::Component* parent,
                          int cx, int cy, int r, bool bipolar = false, bool labelsBeside = false);
    juce::Slider* addFader(const juce::String& param, const juce::String& label, juce::Component* parent,
                           int cx, int y, int h);
    juce::Slider* addBar(const juce::String& param, const juce::String& label, juce::Component* parent,
                         int x, int y, int w, bool bipolar = false);
    juce::Label* addCaption(const juce::String& text, juce::Component* parent, int x, int y, int w,
                            juce::Justification just = juce::Justification::centredLeft);
    juce::ComboBox* addChoice(const juce::String& param, juce::Component* parent, int x, int y, int w, int h = 22);
    juce::ToggleButton* addToggle(const juce::String& param, const juce::String& label, juce::Component* parent,
                                  int x, int y, int w, int h);
    juce::ToggleButton* addPower(const juce::String& param, juce::Component* parent, int x, int y);
    pastel::SegChoice* addSeg(const juce::String& param, juce::Component* parent, juce::Rectangle<int> b,
                          juce::StringArray labels = {}, std::vector<pastel::Wave> icons = {});
    template <typename V, typename... Args>
    V* addView(juce::Component* parent, juce::Rectangle<int> bounds, Args&&... args)
    {
        auto* v = new V(std::forward<Args>(args)...);
        views.emplace_back(v);
        parent->addAndMakeVisible(v);
        v->setBounds(bounds);
        return v;
    }
    MonoProcessor& proc;
    MonoLNF lnf;
    juce::TextButton prevBtn { "<" }, nextBtn { ">" }, nameBtn { "Init Patch" };
    juce::TextButton saveBtn { "SAVE" }, initBtn { "INIT" }, panicBtn { "PANIC" };
    juce::TextButton undoBtn { juce::String::fromUTF8("\xe2\x86\xb6") }, redoBtn { juce::String::fromUTF8("\xe2\x86\xb7") };
    juce::TextButton randBtn { "RANDOM" }, mutateBtn { "MUTATE" }, locksBtn { "LOCKS" };
    juce::ComboBox* dlDivBox = nullptr;   // shown while the delay is tempo-synced
    struct MatrixRow { std::vector<juce::Component*> parts; };
    std::vector<MatrixRow> matrixRows;    // dimmed while the row has no source
    std::vector<std::unique_ptr<juce::GroupComponent>> groups;
    std::vector<std::unique_ptr<juce::Slider>> knobs;
    std::vector<std::unique_ptr<juce::Label>> labels;
    std::vector<std::unique_ptr<juce::ComboBox>> boxes;
    std::vector<std::unique_ptr<juce::ToggleButton>> toggles;
    std::vector<std::unique_ptr<juce::Component>> views;
    std::unique_ptr<PresetBrowser> browser;
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
    bool keyPressed(const juce::KeyPress&) override; // Ctrl/Cmd+Z undo, +Shift redo
private:
    MonoProcessor& proc;
    MonoContent content;
    juce::ResizableCornerComponent corner;
    juce::TooltipWindow tooltips { this, 600 }; // hover help for every control with a tooltip
};
