#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "PresetBrowser.h"
#include "MonoAssets.h"
#include <cmath>

MonoLNF::MonoLNF()
{
    latoReg   = juce::Typeface::createSystemTypefaceFor(MonoAssets::LatoRegular_ttf, MonoAssets::LatoRegular_ttfSize);
    latoBold  = juce::Typeface::createSystemTypefaceFor(MonoAssets::LatoBold_ttf, MonoAssets::LatoBold_ttfSize);
    latoBlack = juce::Typeface::createSystemTypefaceFor(MonoAssets::LatoBlack_ttf, MonoAssets::LatoBlack_ttfSize);
}
juce::Font MonoLNF::uiFont(float h) const
{
    if (latoReg != nullptr) return juce::Font(latoReg).withHeight(h);
    return juce::Font(h);
}
juce::Font MonoLNF::titleFont(float h) const
{
    if (latoBold != nullptr) return juce::Font(latoBold).withHeight(h);
    return juce::Font(juce::FontOptions(h, juce::Font::bold));
}
juce::Font MonoLNF::logoFont(float h) const
{
    if (latoBlack != nullptr) return juce::Font(latoBlack).withHeight(h);
    return titleFont(h);
}
juce::Colour MonoLNF::accentFor(juce::Component& c)
{
    juce::Component* start = dynamic_cast<juce::GroupComponent*>(&c) != nullptr ? &c : c.getParentComponent();
    for (juce::Component* p = start; p != nullptr; p = p->getParentComponent())
        if (auto* g = dynamic_cast<juce::GroupComponent*>(p))
        {
            auto v = g->getProperties()["accent"];
            if (v.isInt()) return juce::Colour((juce::uint32)(int)v);
            return MonoColors::textDim;
        }
    return MonoColors::textDim;
}
juce::Font MonoLNF::getLabelFont(juce::Label&) { return uiFont(12.0f); }
juce::Font MonoLNF::getComboBoxFont(juce::ComboBox&) { return uiFont(12.5f); }
juce::Font MonoLNF::getPopupMenuFont() { return uiFont(13.0f); }
juce::Font MonoLNF::getTextButtonFont(juce::TextButton&, int) { return titleFont(12.0f); }

void MonoLNF::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                               float pos, float start, float end, juce::Slider& slider)
{
    float cx = x + w * 0.5f, cy = y + h * 0.5f;
    float r = juce::jmin(w, h) * 0.5f - 1.0f;
    float a = start + pos * (end - start);
    float br = r - 5.0f;
    g.setColour(MonoColors::raised);
    g.fillEllipse(cx - br, cy - br, br * 2, br * 2);
    g.setColour(MonoColors::border);
    g.drawEllipse(cx - br + 0.5f, cy - br + 0.5f, br * 2 - 1, br * 2 - 1, 1.0f);
    float arcR = r - 1.5f;
    g.setColour(MonoColors::track);
    juce::Path track;
    track.addArc(cx - arcR, cy - arcR, arcR * 2, arcR * 2, start, end, true);
    g.strokePath(track, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    if (pos > 0.001f)
    {
        g.setColour(MonoLNF::accentFor(slider));
        juce::Path val;
        val.addArc(cx - arcR, cy - arcR, arcR * 2, arcR * 2, start, a, true);
        g.strokePath(val, juce::PathStrokeType(2.5f, juce::PathStrokeType::curved, juce::PathStrokeType::butt));
    }
    float dx = std::cos(a - juce::MathConstants<float>::halfPi);
    float dy = std::sin(a - juce::MathConstants<float>::halfPi);
    g.setColour(juce::Colours::white.withAlpha(0.9f));
    g.drawLine(cx + dx * 2.0f, cy + dy * 2.0f, cx + dx * (br - 2.0f), cy + dy * (br - 2.0f), 2.5f);
}

void MonoLNF::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    float cy = y + h * 0.5f;
    float l = (float)(x + 4), rr = (float)(x + w - 4);
    g.setColour(MonoColors::track);
    g.fillRoundedRectangle(l, cy - 1.5f, rr - l, 3.0f, 1.5f);
    float fx = l + pos * (rr - l);
    g.setColour(MonoColors::blue);
    g.fillRoundedRectangle(l, cy - 1.5f, fx - l, 3.0f, 1.5f);
    g.setColour(MonoColors::text);
    g.fillRoundedRectangle(fx - 5.0f, cy - 8.0f, 10.0f, 16.0f, 2.0f);
}

void MonoLNF::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool, bool)
{
    bool on = b.getToggleState();
    float w = (float)b.getWidth(), h = (float)b.getHeight();
    float pw = juce::jmin(w - 12.0f, 40.0f), ph = 15.0f;
    float px = (w - pw) * 0.5f, py = 3.0f;
    g.setColour(on ? MonoLNF::accentFor(b).withAlpha(0.4f) : MonoColors::track);
    g.fillRoundedRectangle(px, py, pw, ph, ph * 0.5f);
    float kr = ph - 5.0f;
    float kx = on ? px + pw - kr - 2.5f : px + 2.5f;
    g.setColour(on ? MonoLNF::accentFor(b) : MonoColors::textDim);
    g.fillEllipse(kx, py + 2.5f, kr, kr);
    g.setColour(on ? MonoColors::text : MonoColors::textDim);
    g.setFont(uiFont(11.5f));
    g.drawText(b.getButtonText(), 0, py + ph + 1.0f, (int)w, (int)(h - py - ph - 1.0f),
               juce::Justification::centred, true);
}

void MonoLNF::drawComboBox(juce::Graphics& g, int w, int h, bool,
                           int, int, int, int, juce::ComboBox& box)
{
    // Background + arrow only; JUCE paints the selected text itself.
    g.setColour(MonoColors::control);
    g.fillRoundedRectangle(0, 0, w, h, 4.0f);
    g.setColour(box.hasKeyboardFocus(true) ? MonoLNF::accentFor(box) : MonoColors::border);
    g.drawRoundedRectangle(0.5f, 0.5f, w - 1, h - 1, 4.0f, 1.0f);
    juce::Path arrow;
    arrow.addTriangle((float)w - 16.0f, h * 0.5f - 3, (float)w - 8.0f, h * 0.5f - 3, (float)w - 12.0f, h * 0.5f + 3);
    g.setColour(MonoColors::textDim);
    g.fillPath(arrow);
    juce::ignoreUnused(box);
}

void MonoLNF::drawGroupComponentOutline(juce::Graphics& g, int w, int h, const juce::String& text,
                                        const juce::Justification&, juce::GroupComponent& group)
{
    g.setColour(MonoColors::panel);
    g.fillRoundedRectangle(0, 0, w, h, 5.0f);
    juce::Colour accent = MonoLNF::accentFor(group);
    g.setColour(MonoColors::border);
    g.drawRoundedRectangle(0.5f, 0.5f, w - 1, h - 1, 5.0f, 1.0f);
    const float barH = 24.0f;
    g.setColour(MonoColors::raised);
    g.fillRoundedRectangle(1, 1, w - 2, barH, 4.0f);
    g.fillRect(1, (int)(barH - 5), w - 2, 5);
    g.setColour(accent);
    g.fillRect(10, (int)(barH - 3), 26, 2);
    g.setColour(MonoColors::text);
    g.setFont(titleFont(12.5f));
    g.drawText(text.toUpperCase(), 42, 0, w - 48, (int)barH, juce::Justification::centredLeft, false);
}

void MonoLNF::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down)
{
    float w = (float)b.getWidth(), h = (float)b.getHeight();
    bool on = b.getToggleState();
    g.setColour(down ? MonoColors::raised : (on ? MonoColors::control.brighter(0.15f) : MonoColors::control));
    g.fillRoundedRectangle(0, 0, w, h, 4.0f);
    g.setColour(hover || on ? MonoColors::text : MonoColors::border);
    g.drawRoundedRectangle(0.5f, 0.5f, w - 1, h - 1, 4.0f, 1.0f);
}

void MonoLNF::drawPopupMenuBackground(juce::Graphics& g, int w, int h)
{
    g.setColour(MonoColors::panel);
    g.fillRoundedRectangle(0, 0, w, h, 5.0f);
    g.setColour(MonoColors::border);
    g.drawRoundedRectangle(0.5f, 0.5f, w - 1, h - 1, 5.0f, 1.0f);
}

// ---------------- AmountKnob ----------------
AmountKnob::AmountKnob(MonoProcessor& p, const juce::String& paramId, MonoLNF& l)
    : proc(p), pid(paramId), lnf(l) { syncFromParam(); }

void AmountKnob::syncFromParam()
{
    if (auto* par = proc.apvts.getRawParameterValue(pid))
        value = juce::jlimit(-1.0f, 1.0f, par->load());
}
void AmountKnob::setValue(float v, bool notify)
{
    value = juce::jlimit(-1.0f, 1.0f, v);
    if (auto* par = dynamic_cast<juce::AudioParameterFloat*>(proc.apvts.getParameter(pid)))
        par->setValueNotifyingHost(par->convertTo0to1(value));
    if (notify && onChange) onChange(value);
    repaint();
}
void AmountKnob::paint(juce::Graphics& g)
{
    float w = (float)getWidth(), h = (float)getHeight();
    float cx = w * 0.5f, cy = h * 0.5f, r = juce::jmin(w, h) * 0.5f - 2.0f;
    g.setColour(MonoColors::control);
    g.fillEllipse(cx - r, cy - r, r * 2, r * 2);
    g.setColour(MonoColors::border);
    g.drawEllipse(cx - r + 0.5f, cy - r + 0.5f, r * 2 - 1, r * 2 - 1, 1.0f);
    float range = juce::MathConstants<float>::pi * 1.35f;
    float a = value * range;
    g.setColour(MonoColors::pink);
    g.drawLine(cx, cy, cx + std::sin(a) * (r - 3.0f), cy - std::cos(a) * (r - 3.0f), 2.0f);
    g.setColour(MonoColors::textDim);
    g.fillEllipse(cx - 1.5f, cy - r - 1.0f, 3.0f, 3.0f);
}
void AmountKnob::mouseDown(const juce::MouseEvent& e) { dragStartY = e.getScreenY(); dragStartV = value; }
void AmountKnob::mouseDrag(const juce::MouseEvent& e)
{
    double dy = dragStartY - (double)e.getScreenY();
    float step = e.mods.isShiftDown() ? 0.002f : 0.008f;
    setValue(dragStartV + (float)(dy * step), true);
}
void AmountKnob::mouseDoubleClick(const juce::MouseEvent&) { setValue(0.0f, true); }

// ---------------- MonoContent ----------------
static const char* waveNames[5] = { "Saw", "Tri", "Sqr", "Pls", "Sin" };

MonoContent::MonoContent(MonoProcessor& p) : proc(p)
{
    setLookAndFeel(&lnf);

    addAndMakeVisible(prevBtn); addAndMakeVisible(nextBtn);
    addAndMakeVisible(nameBtn); addAndMakeVisible(saveBtn);
    addAndMakeVisible(initBtn);
    for (auto* b : { &prevBtn, &nextBtn, &nameBtn, &saveBtn, &initBtn })
    {
        b->setLookAndFeel(&lnf);
        b->setColour(juce::TextButton::buttonColourId, MonoColors::raised);
        b->setColour(juce::TextButton::textColourOnId, MonoColors::text);
        b->setColour(juce::TextButton::textColourOffId, MonoColors::text);
    }
    // step through the factory list in file order (from a user/init sound,
    // > starts at the first slot and < at the last)
    auto stepPreset = [this](int dir) {
        const auto& all = getFactoryPresets();
        if (all.empty()) return;
        int n = (int)all.size(), pos = -1;
        for (int i = 0; i < n; ++i)
            if (all[(size_t)i].index == proc.getFactorySlot()) { pos = i; break; }
        int npos = pos < 0 ? (dir > 0 ? 0 : n - 1) : (pos + dir + n) % n;
        proc.loadPreset(all[(size_t)npos].index);
        refreshPresetName();
    };
    prevBtn.onClick = [stepPreset] { stepPreset(-1); };
    nextBtn.onClick = [stepPreset] { stepPreset(+1); };
    nameBtn.onClick = [&] { if (browser != nullptr) browser->setVisible(true); };
    saveBtn.onClick = [&] { if (browser != nullptr) browser->openForSave(); };
    initBtn.onClick = [&] { proc.loadInit(); refreshPresetName(); };
    nameBtn.setTooltip("Browse presets");
    refreshPresetName();

    volTop = addKnob(P::VOLUME, "", this, 0, 0, 40, 40);
    volTop->setVisible(true);

    // ---- Panel 1 row 1 ----
    auto* gVco1 = addGroup("VCO 1", 8, 64, 224, 252, MonoColors::blue);
    addCaption("WAVEFORM", gVco1, 10, 30, 204);
    addWaveButtons(P::VCO1_WAVE, gVco1, 18, 46);
    addKnob(P::VCO1_PITCH, "PITCH", gVco1, 6, 92);
    addKnob(P::VCO1_TUNE, "TUNE", gVco1, 80, 92);
    addKnob(P::VCO1_PW, "PW", gVco1, 154, 92);
    addCaption("OCTAVE", gVco1, 10, 176, 204);
    addOctButtons(P::VCO1_OCT, gVco1, 23, 192);

    auto* gVco2 = addGroup("VCO 2", 240, 64, 224, 252, MonoColors::blue);
    addCaption("WAVEFORM", gVco2, 10, 30, 204);
    addWaveButtons(P::VCO2_WAVE, gVco2, 18, 46);
    addKnob(P::VCO2_PITCH, "PITCH", gVco2, 6, 92);
    addKnob(P::VCO2_TUNE, "TUNE", gVco2, 80, 92);
    addKnob(P::VCO2_PW, "PW", gVco2, 154, 92);
    addCaption("OCTAVE", gVco2, 10, 176, 204);
    addOctButtons(P::VCO2_OCT, gVco2, 23, 192);

    auto* gMix = addGroup("MIXER", 472, 64, 140, 252, MonoColors::blue);
    addKnob(P::MIX_VCO1, "VCO 1", gMix, 6, 34, 64, 66);
    addKnob(P::MIX_VCO2, "VCO 2", gMix, 70, 34, 64, 66);
    addKnob(P::MIX_RING, "RING", gMix, 6, 104, 64, 66);
    addKnob(P::MIX_NOISE, "NOISE", gMix, 70, 104, 64, 66);
    addKnob(P::THICK, "THICKNESS", gMix, 38, 174, 64, 66);

    auto* gFilt = addGroup("FILTER", 620, 64, 336, 252, MonoColors::lavender);
    addCaption("MIX  >  HPF  >  LPF", gFilt, 58, 30, 220);
    addCaption("HPF (HIGH PASS)", gFilt, 10, 58, 150);
    addKnob(P::HPF_CUT, "CUTOFF", gFilt, 14, 74, 64, 78);
    addKnob(P::HPF_PEAK, "PEAK", gFilt, 92, 74, 64, 78);
    addCaption("LPF (LOW PASS)", gFilt, 176, 58, 150);
    addKnob(P::LPF_CUT, "CUTOFF", gFilt, 180, 74, 64, 78);
    addKnob(P::LPF_RES, "RESO", gFilt, 258, 74, 64, 78);
    // DRIVE centred under the HPF pair, MODEL under the LPF pair, both on
    // one centre line
    addKnob(P::DRIVE, "DRIVE", gFilt, 53, 160, 64, 72);
    addCaption("MODEL", gFilt, 191, 160, 120);
    addChoice(P::FMODEL, gFilt, 191, 176, 120);

    auto* gVca = addGroup("VCA", 964, 64, 120, 252, MonoColors::yellow);
    addKnob(P::VCA_LEVEL, "LEVEL", gVca, 28, 100, 64, 90);

    auto* gOut = addGroup("OUTPUT", 1092, 64, 180, 252, MonoColors::textDim);
    addKnob(P::VOLUME, "VOLUME", gOut, 58, 30, 64, 72);
    addKnob(P::PORTA, "PORTA", gOut, 58, 116, 64, 62);
    addToggle(P::PORTA_ON, "PORTA", gOut, 8, 196, 78, 44);
    addToggle(P::LIMITER, "LIMITER", gOut, 94, 196, 78, 44);

    // ---- Panel 1 row 2 ----
    auto* gMg = addGroup("MODULATION GENERATOR", 8, 324, 224, 160, MonoColors::mint);
    addCaption("WAVEFORM", gMg, 10, 30, 204);
    addWaveButtons(P::MG_WAVE, gMg, 18, 46);
    addKnob(P::MG_RATE, "RATE", gMg, 34, 80, 64, 72);
    addToggle(P::MG_SYNC, "SYNC", gMg, 118, 98, 80, 40);

    auto* gEg1 = addGroup("ENVELOPE GENERATOR 1 (FILTER)", 240, 324, 372, 160, MonoColors::yellow);
    addKnob(P::EG1_A, "ATTACK", gEg1, 12, 40, 72, 84);
    addKnob(P::EG1_D, "DECAY", gEg1, 104, 40, 72, 84);
    addKnob(P::EG1_S, "SUSTAIN", gEg1, 196, 40, 72, 84);
    addKnob(P::EG1_R, "RELEASE", gEg1, 288, 40, 72, 84);

    auto* gEg2 = addGroup("ENVELOPE GENERATOR 2 (AMP)", 620, 324, 372, 160, MonoColors::yellow);
    addKnob(P::EG2_A, "ATTACK", gEg2, 12, 40, 72, 84);
    addKnob(P::EG2_D, "DECAY", gEg2, 104, 40, 72, 84);
    addKnob(P::EG2_S, "SUSTAIN", gEg2, 196, 40, 72, 84);
    addKnob(P::EG2_R, "RELEASE", gEg2, 288, 40, 72, 84);

    auto* gVel = addGroup("EXPRESS", 1000, 324, 272, 160, MonoColors::pink);
    addKnob(P::VELSENS, "VELOCITY", gVel, 12, 40, 72, 84);
    addKnob(P::BEND_RANGE, "BEND", gVel, 100, 40, 72, 84);
    addCaption("GLIDE MODE", gVel, 180, 44, 84);
    addChoice(P::GLIDE_MODE, gVel, 184, 62, 76); // centred on the knob row
    // ---- Panel 2: mod matrix ----
    auto* gMat = addGroup("MODULATION MATRIX", 8, 492, 760, 332, MonoColors::pink);
    const char* heads[5] = { "#", "SOURCE", "AMOUNT", "DESTINATION", "TYPE" };
    const int hx[5] = { 8, 40, 252, 380, 590 };
    const int hw[5] = { 30, 200, 100, 200, 160 };
    for (int i = 0; i < 5; ++i) addCaption(heads[i], gMat, hx[i], 30, hw[i]);
    for (int r = 0; r < 8; ++r)
    {
        int y = 50 + r * 34;
        auto* num = new juce::Label("n" + juce::String(r), juce::String(r + 1));
        labels.emplace_back(num);
        gMat->addAndMakeVisible(num);
        num->setBounds(hx[0], y, hw[0], 24);
        num->setJustificationType(juce::Justification::centred);
        num->setColour(juce::Label::textColourId, MonoColors::textDim);
        num->setFont(lnf.uiFont(12.0f));

        auto* sc = new juce::ComboBox();
        boxes.emplace_back(sc);
        gMat->addAndMakeVisible(sc);
        sc->setBounds(hx[1], y, hw[1], 24);
        sc->setLookAndFeel(&lnf);
        sc->setColour(juce::ComboBox::textColourId, MonoColors::text);
        if (auto* par = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(P::modSrcId(r))))
            for (int k = 0; k < par->choices.size(); ++k) sc->addItem(par->choices[k], k + 1);
        cAtt.add(new juce::AudioProcessorValueTreeState::ComboBoxAttachment(proc.apvts, P::modSrcId(r), *sc));

        auto* ak = new AmountKnob(proc, P::modAmtId(r), lnf);
        amtKnobs.emplace_back(ak);
        gMat->addAndMakeVisible(ak);
        ak->setBounds(hx[2], y - 1, 26, 26);
        auto pct = [](float v) { return (v >= 0 ? "+" : "") + juce::String((int)std::round(v * 100)) + "%"; };
        auto* al = new juce::Label("a" + juce::String(r), pct(ak->getValue()));
        amtLabels.emplace_back(al);
        gMat->addAndMakeVisible(al);
        al->setBounds(hx[2] + 30, y, 60, 24);
        al->setColour(juce::Label::textColourId, MonoColors::text);
        al->setFont(lnf.uiFont(12.5f));
        ak->onChange = [al, pct](float v) { al->setText(pct(v), juce::dontSendNotification); };

        auto* dc = new juce::ComboBox();
        boxes.emplace_back(dc);
        gMat->addAndMakeVisible(dc);
        dc->setBounds(hx[3], y, hw[3], 24);
        dc->setLookAndFeel(&lnf);
        dc->setColour(juce::ComboBox::textColourId, MonoColors::text);
        if (auto* par = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(P::modDstId(r))))
            for (int k = 0; k < par->choices.size(); ++k) dc->addItem(par->choices[k], k + 1);
        cAtt.add(new juce::AudioProcessorValueTreeState::ComboBoxAttachment(proc.apvts, P::modDstId(r), *dc));

        auto* tc = new juce::ComboBox();
        boxes.emplace_back(tc);
        gMat->addAndMakeVisible(tc);
        tc->setBounds(hx[4], y, hw[4], 24);
        tc->setLookAndFeel(&lnf);
        tc->setColour(juce::ComboBox::textColourId, MonoColors::text);
        if (auto* par = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(P::modTypeId(r))))
            for (int k = 0; k < par->choices.size(); ++k) tc->addItem(par->choices[k], k + 1);
        cAtt.add(new juce::AudioProcessorValueTreeState::ComboBoxAttachment(proc.apvts, P::modTypeId(r), *tc));
    }

    auto* gGlob = addGroup("GLOBAL", 776, 492, 200, 332, MonoColors::textDim);
    // four evenly spaced blocks: caption + control, all on the centre line
    addCaption("BEND RANGE", gGlob, 10, 47, 180);
    addKnob(P::BEND_RANGE, "", gGlob, 68, 63, 64, 64);
    addCaption("GLIDE TIME", gGlob, 10, 128, 180);
    addKnob(P::PORTA, "", gGlob, 68, 144, 64, 64);
    addCaption("GLIDE MODE", gGlob, 10, 209, 180);
    addChoice(P::GLIDE_MODE, gGlob, 40, 225, 120);
    addCaption("NOISE TYPE", gGlob, 10, 266, 180);
    addChoice(P::NOISE_TYPE, gGlob, 40, 282, 120);

    auto* gFx = addGroup("EFFECTS", 984, 492, 288, 332, MonoColors::textDim);
    // four rows on a shared grid: knobs in columns 1-3, switch/selector in
    // the right column, everything centred on each row's knob line
    {
        const int kx[3] = { 12, 76, 140 }, rx = 204, rw = 72;
        auto rowCap = [&](const char* t, int y0) {
            addCaption(t, gFx, 12, y0, 264)->setJustificationType(juce::Justification::centredLeft);
        };
        const int step = 74, kh = 52; // knob slider 36px + label; 10px air above next caption
        auto knob = [&](const char* id, const char* lab, int col, int y0) { addKnob(id, lab, gFx, kx[col], y0 + 14, 60, kh); };
        int y0 = 30;
        rowCap("DISTORTION", y0);
        knob(P::SAT_AMT, "AMOUNT", 0, y0);
        addChoice(P::SAT_TYPE, gFx, rx, y0 + 20, rw);
        y0 += step;
        rowCap("CHORUS", y0);
        knob(P::CH_RATE, "RATE", 0, y0);
        knob(P::CH_DEPTH, "DEPTH", 1, y0);
        addToggle(P::CH_ON, "ON", gFx, rx, y0 + 22, rw, 36);
        y0 += step;
        rowCap("DELAY", y0);
        knob(P::DL_TIME, "TIME", 0, y0);
        knob(P::DL_FB, "FDBK", 1, y0);
        knob(P::DL_MIX, "MIX", 2, y0);
        addToggle(P::DL_ON, "ON", gFx, rx, y0 + 22, rw, 36);
        y0 += step;
        rowCap("REVERB", y0);
        knob(P::RV_SIZE, "SIZE", 0, y0);
        knob(P::RV_MIX, "MIX", 1, y0);
        addToggle(P::RV_ON, "ON", gFx, rx, y0 + 22, rw, 36);
    }

    // preset browser overlay (created last: paints above everything)
    browser = std::make_unique<PresetBrowser>(proc, lnf);
    addAndMakeVisible(browser.get());
    browser->setVisible(false);
    browser->onClose = [&] { browser->setVisible(false); };
    browser->onPresetChanged = [&] { refreshPresetName(); };

    startTimerHz(4);
}

MonoContent::~MonoContent() { setLookAndFeel(nullptr); }

void MonoContent::timerCallback()
{
    for (size_t i = 0; i < amtKnobs.size() && i < amtLabels.size(); ++i)
    {
        if (auto* par = proc.apvts.getRawParameterValue(P::modAmtId((int)i)))
        {
            float v = juce::jlimit(-1.0f, 1.0f, par->load());
            amtLabels[i]->setText((v >= 0 ? "+" : "") + juce::String((int)std::round(v * 100)) + "%",
                                  juce::dontSendNotification);
            amtKnobs[i]->setQuiet(v);
        }
    }
    for (auto& wi : waveBtnInfos)
    {
        if (auto* par = proc.apvts.getParameter(wi.param))
        {
            int cur = (int)std::round(par->getValue() * 4.0f);
            wi.btn->setToggleState(cur == wi.index, juce::dontSendNotification);
        }
    }
    refreshPresetName();
}

juce::GroupComponent* MonoContent::addGroup(const juce::String& title, int x, int y, int w, int h, juce::Colour accent)
{
    auto* g = new juce::GroupComponent(title, title);
    groups.emplace_back(g);
    addAndMakeVisible(g);
    g->setBounds(x, y, w, h);
    g->setLookAndFeel(&lnf);
    g->getProperties().set("accent", (int)accent.getARGB());
    return g;
}

juce::Label* MonoContent::addCaption(const juce::String& text, juce::Component* parent, int x, int y, int w)
{
    auto* l = new juce::Label(text, text);
    labels.emplace_back(l);
    parent->addAndMakeVisible(l);
    l->setBounds(x, y, w, 14);
    l->setJustificationType(juce::Justification::centred);
    l->setColour(juce::Label::textColourId, MonoColors::textDim);
    l->setFont(lnf.uiFont(11.5f));
    return l;
}

juce::Slider* MonoContent::addKnob(const juce::String& param, const juce::String& label,
                                   juce::Component* parent, int x, int y, int w, int h)
{
    auto* s = new juce::Slider();
    knobs.emplace_back(s);
    parent->addAndMakeVisible(s);
    s->setBounds(x, y, w, h - 16);
    s->setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s->setLookAndFeel(&lnf);
    s->setDoubleClickReturnValue(true, proc.apvts.getParameter(param) ?
        proc.apvts.getParameter(param)->convertFrom0to1(
            proc.apvts.getParameter(param)->getDefaultValue()) : 0.5);
    sAtt.add(new juce::AudioProcessorValueTreeState::SliderAttachment(proc.apvts, param, *s));
    if (label.isNotEmpty())
    {
        auto* l = new juce::Label(param + "_l", label);
        labels.emplace_back(l);
        parent->addAndMakeVisible(l);
        l->setBounds(x, y + h - 16, w, 14);
        l->setJustificationType(juce::Justification::centred);
        l->setColour(juce::Label::textColourId, MonoColors::textDim);
        l->setFont(lnf.uiFont(11.5f));
    }
    return s;
}

juce::ComboBox* MonoContent::addChoice(const juce::String& param, juce::Component* parent,
                                       int x, int y, int w, int h)
{
    auto* c = new juce::ComboBox();
    boxes.emplace_back(c);
    parent->addAndMakeVisible(c);
    c->setBounds(x, y, w, h);
    c->setLookAndFeel(&lnf);
    c->setColour(juce::ComboBox::textColourId, MonoColors::text);
    if (auto* par = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(param)))
        for (int i = 0; i < par->choices.size(); ++i) c->addItem(par->choices[i], i + 1);
    cAtt.add(new juce::AudioProcessorValueTreeState::ComboBoxAttachment(proc.apvts, param, *c));
    return c;
}

juce::ToggleButton* MonoContent::addToggle(const juce::String& param, const juce::String& label,
                                           juce::Component* parent, int x, int y, int w, int h)
{
    auto* t = new juce::ToggleButton(label);
    toggles.emplace_back(t);
    parent->addAndMakeVisible(t);
    t->setBounds(x, y, w, h);
    t->setLookAndFeel(&lnf);
    bAtt.add(new juce::AudioProcessorValueTreeState::ButtonAttachment(proc.apvts, param, *t));
    return t;
}

void MonoContent::addWaveButtons(const juce::String& param, juce::Component* parent, int x, int y, int bw)
{
    for (int i = 0; i < 5; ++i)
    {
        auto* b = new juce::TextButton(waveNames[i]);
        waveBtns.emplace_back(b);
        parent->addAndMakeVisible(b);
        b->setBounds(x + i * (bw + 2), y, bw, 24);
        b->setLookAndFeel(&lnf);
        b->setClickingTogglesState(true);
        b->setRadioGroupId(param.hashCode());
        b->onClick = [&, i, param] {
            if (auto* par = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(param)))
                par->setValueNotifyingHost(par->convertTo0to1((float)i));
        };
        if (auto* par = proc.apvts.getParameter(param))
        {
            int cur = (int)std::round(par->getValue() * 4.0f);
            b->setToggleState(cur == i, juce::dontSendNotification);
        }
        waveBtnInfos.push_back({ b, param, i });
    }
}

void MonoContent::addOctButtons(const juce::String& param, juce::Component* parent, int x, int y, int bw)
{
    const char* names[5] = { "-2", "-1", "0", "+1", "+2" };
    for (int i = 0; i < 5; ++i)
    {
        auto* b = new juce::TextButton(names[i]);
        waveBtns.emplace_back(b);
        parent->addAndMakeVisible(b);
        b->setBounds(x + i * (bw + 2), y, bw, 24);
        b->setLookAndFeel(&lnf);
        b->setClickingTogglesState(true);
        b->setRadioGroupId(param.hashCode() + 99);
        b->onClick = [&, i, param] {
            if (auto* par = dynamic_cast<juce::AudioParameterChoice*>(proc.apvts.getParameter(param)))
                par->setValueNotifyingHost(par->convertTo0to1((float)i));
        };
        if (auto* par = proc.apvts.getParameter(param))
        {
            int cur = (int)std::round(par->getValue() * 4.0f);
            b->setToggleState(cur == i, juce::dontSendNotification);
        }
        waveBtnInfos.push_back({ b, param, i });
    }
}

void MonoContent::refreshPresetName()
{
    int slot = proc.getFactorySlot();
    auto name = proc.getCurrentPresetName();
    juce::String text = slot >= 0 ? juce::String(slot).paddedLeft('0', 3) + "  " + name : name;
    if (nameBtn.getButtonText() != text) nameBtn.setButtonText(text);
}

void MonoContent::paint(juce::Graphics& g)
{
    g.fillAll(MonoColors::bg);
    g.setColour(MonoColors::raised);
    g.fillRect(0, 0, getWidth(), 56);
    g.setColour(MonoColors::blue);
    g.fillRect(0, 56, getWidth(), 2);
    g.setColour(MonoColors::text);
    g.setFont(lnf.logoFont(26.0f));
    g.drawText("MONOSYNTH", 16, 12, 260, 32, juce::Justification::centredLeft, false); // centred in the 56px bar
}

void MonoContent::resized()
{
    if (getWidth() <= 0) return;
    // preset bar centred on the window: < [name] >  SAVE INIT
    const int barW = 32 + 4 + 330 + 4 + 32 + 10 + 60 + 4 + 52;
    int x = (baseW - barW) / 2;
    prevBtn.setBounds(x, 14, 32, 28);          x += 36;
    nameBtn.setBounds(x, 14, 330, 28);         x += 334;
    nextBtn.setBounds(x, 14, 32, 28);          x += 42;
    saveBtn.setBounds(x, 14, 60, 28);          x += 64;
    initBtn.setBounds(x, 14, 52, 28);
    if (volTop != nullptr) volTop->setBounds(getWidth() - 56, 8, 40, 40);
    if (browser != nullptr)
        browser->setBounds(0, 0, baseW, baseH);
}

// ---------------- editor ----------------
MonoEditor::MonoEditor(MonoProcessor& p)
    : juce::AudioProcessorEditor(p), content(p),
      corner(this, getConstrainer())
{
    addAndMakeVisible(content);   // content first...
    addAndMakeVisible(corner);    // ...corner above it
    setSize(MonoContent::baseW, MonoContent::baseH); // size LAST (lesson: layout fires synchronously)
    setResizable(true, true);
    setResizeLimits(768, 500, 2048, 1332);
}

MonoEditor::~MonoEditor() {}

void MonoEditor::paint(juce::Graphics& g) { g.fillAll(MonoColors::bg); }

void MonoEditor::resized()
{
    if (getWidth() <= 0 || getHeight() <= 0) return;
    float s = juce::jmin((float)getWidth() / (float)MonoContent::baseW,
                         (float)getHeight() / (float)MonoContent::baseH);
    s = juce::jlimit(0.4f, 3.0f, s);
    float ox = ((float)getWidth() - (float)MonoContent::baseW * s) * 0.5f;
    float oy = ((float)getHeight() - (float)MonoContent::baseH * s) * 0.5f;
    content.setTransform(juce::AffineTransform::scale(s).translated(ox, oy));
    content.setBounds(0, 0, MonoContent::baseW, MonoContent::baseH);
    corner.setBounds(getWidth() - 20, getHeight() - 20, 20, 20);
}
