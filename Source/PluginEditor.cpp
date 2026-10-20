#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "PresetBrowser.h"
#include "MonoAssets.h"
#include <cmath>

// ---------------- value text ----------------
// MG sync divisions, same table as the processor's mgRateHz (cycles per beat)
static const char* mgSyncNames[11] = { "1/1", "1/2D", "1/2", "1/4D", "1/2T", "1/4", "1/8D", "1/4T", "1/8", "1/8T", "1/16" };

static juce::String fmtHz(float hz)
{
    return hz < 1000.0f ? juce::String(hz, hz < 10.0f ? 2 : (hz < 100.0f ? 1 : 0)) + " Hz"
                        : juce::String(hz / 1000.0f, 2) + " kHz";
}
static juce::String fmtSecs(float t)
{
    return t < 1.0f ? juce::String((int)std::round(t * 1000.0f)) + " ms" : juce::String(t, 2) + " s";
}
static juce::String fmtSigned(float v, int dec, const char* unit)
{
    return (v > 0.0f ? "+" : "") + juce::String(v, dec) + unit;
}

// Stateless (shared by every open editor through pastel::Theme::format).
static juce::String monoFormat(const juce::String& id, float v)
{
    if (id == P::HPF_CUT || id == P::LPF_CUT || id == P::CH_RATE || id == P::MG_RATE) return fmtHz(v);
    if (id == P::EG1_A || id == P::EG1_D || id == P::EG1_R || id == P::EG2_A || id == P::EG2_D || id == P::EG2_R)
        return fmtSecs(v);
    if (id == P::EG1_DELAY || id == P::EG2_HOLD) return v <= 0.0005f ? juce::String("Off") : fmtSecs(v);
    if (id == P::ARP_OCT) return juce::String((int)std::round(v)) + " oct";
    if (id == P::PORTA) return v <= 0.001f ? juce::String("Off") : fmtSecs(0.002f + v * v * 0.6f);
    if (id == P::DL_TIME) return juce::String((int)std::round(v)) + " ms";
    if (id == P::VCO1_PITCH || id == P::VCO2_PITCH) return fmtSigned(v, 1, " st");
    if (id == P::VCO1_TUNE || id == P::VCO2_TUNE) return fmtSigned((float)std::round(v), 0, " ct");
    if (id == P::BEND_RANGE) return juce::String::fromUTF8("\xc2\xb1") + juce::String(v, 1) + " st";
    if (id == P::VOLUME || id == P::VCA_LEVEL)
        return v <= 0.0001f ? juce::String("-inf dB") : fmtSigned(20.0f * std::log10(v), 1, " dB");
    if (id.startsWith("mod") && id.endsWith("amt")) return fmtSigned((float)std::round(v * 100.0f), 0, "%");
    // plain 0..x levels / amounts read as percent of their range
    return juce::String((int)std::round(v * 100.0f)) + "%";
}

// ---------------- look and feel ----------------
MonoLNF::MonoLNF()
{
    latoReg   = juce::Typeface::createSystemTypefaceFor(MonoAssets::LatoRegular_ttf, MonoAssets::LatoRegular_ttfSize);
    latoBold  = juce::Typeface::createSystemTypefaceFor(MonoAssets::LatoBold_ttf, MonoAssets::LatoBold_ttfSize);
    latoBlack = juce::Typeface::createSystemTypefaceFor(MonoAssets::LatoBlack_ttf, MonoAssets::LatoBlack_ttfSize);
    // the shared v3 views draw monochrome, in Lato
    pastel::Theme::accent  = MonoColors::ivory;
    pastel::Theme::inner   = MonoColors::inner;
    pastel::Theme::grid    = MonoColors::grid;
    pastel::Theme::edge    = MonoColors::edge;
    pastel::Theme::comboBg = MonoColors::control;
    pastel::Theme::text    = MonoColors::text;
    pastel::Theme::textDim = MonoColors::textDim;
    pastel::Theme::faint   = MonoColors::faint;
    pastel::Theme::handle  = MonoColors::handle;
    pastel::Theme::ink     = MonoColors::ink;
    pastel::Theme::regular = latoReg;
    pastel::Theme::bold    = latoBold;
    pastel::Theme::format  = monoFormat;
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
juce::Font MonoLNF::getLabelFont(juce::Label&) { return uiFont(12.0f); }
juce::Font MonoLNF::getComboBoxFont(juce::ComboBox&) { return titleFont(11.5f); }
juce::Font MonoLNF::getPopupMenuFont() { return uiFont(13.0f); }
juce::Font MonoLNF::getTextButtonFont(juce::TextButton&, int) { return titleFont(12.0f); }

void MonoLNF::positionComboBoxText(juce::ComboBox& box, juce::Label& label)
{
    label.setBounds(4, 0, box.getWidth() - 22, box.getHeight());
    label.setFont(getComboBoxFont(box));
}

// arc knob: track, value arc ("bipolar" = from 12 o'clock), needle
void MonoLNF::drawRotarySlider(juce::Graphics& g, int x, int y, int w, int h,
                               float pos, float start, float end, juce::Slider& slider)
{
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float r = juce::jmin(w, h) * 0.5f - 2.0f;
    const float a = start + pos * (end - start);
    const float sw = juce::jmax(2.6f, r * 0.17f);
    const float arcR = r - sw * 0.5f;
    const auto stroke = juce::PathStrokeType(sw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
    juce::Path track;
    track.addCentredArc(cx, cy, arcR, arcR, 0.0f, start, end, true);
    g.setColour(MonoColors::edge);
    g.strokePath(track, stroke);
    const bool bipolar = (bool)slider.getProperties()["bipolar"];
    const float from = bipolar ? (start + end) * 0.5f : start;
    if (std::abs(a - from) > 0.01f)
    {
        juce::Path val;
        val.addCentredArc(cx, cy, arcR, arcR, 0.0f, juce::jmin(from, a), juce::jmax(from, a), true);
        g.setColour(accentFor(slider));
        g.strokePath(val, stroke);
    }
    const float br = r - sw - 1.5f;
    g.setColour(juce::Colour(0xff26292f));
    g.fillEllipse(cx - br, cy - br, br * 2, br * 2);
    g.setColour(MonoColors::edge);
    g.drawEllipse(cx - br, cy - br, br * 2, br * 2, 1.0f);
    const float dx = std::sin(a), dy = -std::cos(a);
    g.setColour(MonoColors::handle);
    g.drawLine(cx + dx * br * 0.25f, cy + dy * br * 0.25f, cx + dx * (br - 2.0f), cy + dy * (br - 2.0f), 2.0f);
}

// faders (vertical) and amount bars (horizontal, "bipolar" fills from the centre)
void MonoLNF::drawLinearSlider(juce::Graphics& g, int x, int y, int w, int h, float pos,
                               float, float, juce::Slider::SliderStyle, juce::Slider& s)
{
    const auto acc = accentFor(s);
    if (s.isVertical())
    {
        const float cx = x + w * 0.5f, top = (float)y + 6.0f, bot = (float)(y + h) - 6.0f;
        const float py = juce::jlimit(top, bot, pos);
        g.setColour(MonoColors::track);
        g.fillRoundedRectangle(cx - 2.0f, top, 4.0f, bot - top, 2.0f);
        g.setColour(acc);
        g.fillRoundedRectangle(cx - 2.0f, py, 4.0f, bot - py, 2.0f);
        g.setColour(MonoColors::handle);
        g.fillRoundedRectangle(cx - 11.0f, py - 5.0f, 22.0f, 10.0f, 3.0f);
        return;
    }
    const float cy = y + h * 0.5f, l = (float)x + 6.0f, rr = (float)(x + w) - 6.0f;
    const float px = juce::jlimit(l, rr, pos);
    g.setColour(MonoColors::track);
    g.fillRoundedRectangle(l, cy - 2.0f, rr - l, 4.0f, 2.0f);
    g.setColour(acc);
    if ((bool)s.getProperties()["bipolar"])
    {
        const float m = (l + rr) * 0.5f;
        g.fillRoundedRectangle(juce::jmin(m, px), cy - 2.0f, std::abs(px - m), 4.0f, 2.0f);
        g.setColour(MonoColors::faint);
        g.drawVerticalLine((int)m, cy - 5.0f, cy + 5.0f);
    }
    else
        g.fillRoundedRectangle(l, cy - 2.0f, px - l, 4.0f, 2.0f);
    g.setColour(MonoColors::handle);
    g.fillEllipse(px - 6.0f, cy - 6.0f, 12.0f, 12.0f);
}

// toggles: "power" property = the EFX power glyph, else a pill that fills when on
void MonoLNF::drawToggleButton(juce::Graphics& g, juce::ToggleButton& b, bool hover, bool)
{
    const bool on = b.getToggleState();
    const auto acc = accentFor(b);
    auto r = b.getLocalBounds().toFloat();
    if ((bool)b.getProperties()["power"])
    {
        const auto c = on ? acc : MonoColors::faint;
        const float cx = r.getCentreX(), cy = r.getCentreY(), rad = 8.5f, ir = rad * 0.52f;
        g.setColour(c);
        g.drawEllipse(cx - rad, cy - rad, rad * 2, rad * 2, 1.4f);
        juce::Path arc;
        arc.addCentredArc(cx, cy, ir, ir, 0.0f, juce::degreesToRadians(35.0f), juce::degreesToRadians(325.0f), true);
        g.strokePath(arc, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.drawLine(cx, cy - ir - 1.5f, cx, cy - 0.5f, 1.4f);
        return;
    }
    r = r.reduced(1.0f);
    const float rad = r.getHeight() * 0.5f;
    if (on)
    {
        g.setColour(acc);
        g.fillRoundedRectangle(r, rad);
    }
    else
    {
        g.setColour(acc.withAlpha(hover ? 0.8f : 0.5f));
        g.drawRoundedRectangle(r, rad, 1.2f);
    }
    g.setColour(on ? MonoColors::ink : acc);
    g.setFont(titleFont(10.5f));
    g.drawText(b.getButtonText().toUpperCase(), r, juce::Justification::centred, true);
}

void MonoLNF::drawComboBox(juce::Graphics& g, int w, int h, bool,
                           int, int, int, int, juce::ComboBox& box)
{
    // background + arrow only; JUCE paints the selected text itself
    g.setColour(MonoColors::control);
    g.fillRoundedRectangle(0, 0, (float)w, (float)h, 5.0f);
    g.setColour(box.hasKeyboardFocus(true) ? accentFor(box) : MonoColors::edge);
    g.drawRoundedRectangle(0.5f, 0.5f, w - 1.0f, h - 1.0f, 5.0f, 1.0f);
    juce::Path arrow;
    arrow.startNewSubPath(w - 15.0f, h * 0.5f - 2.0f);
    arrow.lineTo(w - 11.0f, h * 0.5f + 2.0f);
    arrow.lineTo(w - 7.0f, h * 0.5f - 2.0f);
    g.setColour(MonoColors::textDim);
    g.strokePath(arrow, juce::PathStrokeType(1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

// panel: neutral edge, dot + title
void MonoLNF::drawGroupComponentOutline(juce::Graphics& g, int w, int h, const juce::String& text,
                                        const juce::Justification&, juce::GroupComponent& group)
{
    g.setColour(MonoColors::panel);
    g.fillRoundedRectangle(0, 0, (float)w, (float)h, 8.0f);
    g.setColour(MonoColors::border);
    g.drawRoundedRectangle(0.5f, 0.5f, w - 1.0f, h - 1.0f, 8.0f, 1.0f);
    g.setColour(accentFor(group));
    g.fillEllipse(12.4f, 14.4f, 7.2f, 7.2f);
    g.setColour(MonoColors::text);
    g.setFont(logoFont(13.0f));
    g.drawText(text.toUpperCase(), 26, 6, w - 60, 24, juce::Justification::centredLeft, true);
}

void MonoLNF::drawButtonBackground(juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hover, bool down)
{
    const float w = (float)b.getWidth(), h = (float)b.getHeight();
    g.setColour(down ? juce::Colour(0xff2b2e34) : MonoColors::control);
    g.fillRoundedRectangle(0, 0, w, h, 5.0f);
    g.setColour(hover || b.hasKeyboardFocus(true) ? MonoColors::ivory.withAlpha(0.7f) : MonoColors::edge);
    g.drawRoundedRectangle(0.5f, 0.5f, w - 1, h - 1, 5.0f, 1.0f);
}

void MonoLNF::drawPopupMenuBackground(juce::Graphics& g, int w, int h)
{
    g.setColour(MonoColors::panel);
    g.fillRoundedRectangle(0, 0, (float)w, (float)h, 6.0f);
    g.setColour(MonoColors::border);
    g.drawRoundedRectangle(0.5f, 0.5f, w - 1.0f, h - 1.0f, 6.0f, 1.0f);
}

// ---------------- MonoContent ----------------
MonoContent::MonoContent(MonoProcessor& p) : proc(p)
{
    using pastel::Wave;
    setLookAndFeel(&lnf);

    addAndMakeVisible(prevBtn); addAndMakeVisible(nextBtn);
    addAndMakeVisible(nameBtn); addAndMakeVisible(saveBtn);
    addAndMakeVisible(initBtn); addAndMakeVisible(panicBtn);
    for (auto* b : { &undoBtn, &redoBtn, &randBtn, &mutateBtn, &locksBtn }) addAndMakeVisible(b);
    for (auto* b : { &prevBtn, &nextBtn, &nameBtn, &saveBtn, &initBtn, &panicBtn,
                     &undoBtn, &redoBtn, &randBtn, &mutateBtn, &locksBtn })
    {
        b->setLookAndFeel(&lnf);
        b->setColour(juce::TextButton::buttonColourId, MonoColors::control);
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
    panicBtn.onClick = [&] { proc.panic(); };
    undoBtn.onClick = [&] { proc.undoEdit(); refreshPresetName(); };
    redoBtn.onClick = [&] { proc.redoEdit(); refreshPresetName(); };
    randBtn.onClick = [&] { proc.randomize(); refreshPresetName(); };
    mutateBtn.onClick = [&] { proc.mutate(); refreshPresetName(); };
    locksBtn.onClick = [&]
    {
        // ticked = locked: RANDOM / MUTATE leave that section alone
        juce::PopupMenu m;
        m.addSectionHeader("Locked sections");
        const auto names = MonoProcessor::randGroupNames();
        for (int g = 0; g < MonoProcessor::RG_COUNT; ++g)
            m.addItem(g + 1, names[g], true, proc.isGroupLocked(g));
        m.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(&locksBtn),
                        [this](int id) { if (id > 0) proc.setGroupLocked(id - 1, ! proc.isGroupLocked(id - 1)); });
    };
    undoBtn.setTooltip("Undo the last change (Ctrl+Z)");
    redoBtn.setTooltip("Redo (Ctrl+Shift+Z)");
    randBtn.setTooltip("Random patch inside musical limits (unlocked sections only, one undo step)");
    mutateBtn.setTooltip("Nudge the unlocked knobs a little (one undo step)");
    locksBtn.setTooltip("Choose which sections RANDOM / MUTATE leave alone");
    nameBtn.setTooltip("Browse presets");
    prevBtn.setTooltip("Previous preset");
    nextBtn.setTooltip("Next preset");
    saveBtn.setTooltip("Save the current sound as a user preset");
    initBtn.setTooltip("Reset every control to the init patch");
    panicBtn.setTooltip("Silence the voice, clear held notes and FX tails");
    refreshPresetName();

    const std::vector<Wave> vcoWaves { Wave::saw, Wave::tri, Wave::square, Wave::pulse, Wave::sine }; // VCO_WAVE order

    // ---- row 1: VCO 1, VCO 2, mixer, filter, VCA, output ----
    auto vco = [&](const char* title, int x, const char* wave, const char* pw, const char* oct,
                   const char* pitch, const char* tune)
    {
        auto* g = addGroup(title, x, 64, 224, 252);
        addView<pastel::WaveView>(g, { 12, 36, 200, 70 }, proc.apvts, wave, pw, vcoWaves)
            ->setTooltip("Drag sideways on the pulse wave to set its width");
        addSeg(wave, g, { 12, 112, 200, 24 }, {}, vcoWaves);
        addSeg(oct, g, { 12, 142, 200, 22 });
        addKnob(pitch, "Pitch", g, 62, 200, 18, true);
        addKnob(tune, "Tune", g, 162, 200, 18, true);
    };
    vco("VCO 1", 8, P::VCO1_WAVE, P::VCO1_PW, P::VCO1_OCT, P::VCO1_PITCH, P::VCO1_TUNE);
    vco("VCO 2", 240, P::VCO2_WAVE, P::VCO2_PW, P::VCO2_OCT, P::VCO2_PITCH, P::VCO2_TUNE);

    auto* gMix = addGroup("Mixer", 472, 64, 140, 252);
    addFader(P::MIX_VCO1, "VCO1", gMix, 22, 36, 118);
    addFader(P::MIX_VCO2, "VCO2", gMix, 54, 36, 118);
    addFader(P::MIX_RING, "Ring", gMix, 86, 36, 118);
    addFader(P::MIX_NOISE, "Noise", gMix, 118, 36, 118);
    addCaption("NOISE", gMix, 12, 190, 50);
    addSeg(P::NOISE_TYPE, gMix, { 70, 190, 58, 20 }, { "W", "P" })->setTooltip("White / pink noise");
    addBar(P::THICK, "Thick", gMix, 12, 220, 116)->setTooltip("Thickness: soft saturation on the oscillator bus");

    auto* gFilt = addGroup("Filter", 620, 64, 336, 252);
    addCaption("HPF > LPF", gFilt, 230, 8, 90, juce::Justification::centredRight);
    addView<pastel::FilterView>(gFilt, { 12, 36, 312, 146 }, proc.apvts, P::LPF_CUT, P::LPF_RES, juce::String(),
                            P::HPF_CUT, P::HPF_PEAK)
        ->setTooltip("Drag a handle: cutoff sideways, resonance / peak up and down");
    addKnob(P::DRIVE, "Drive", gFilt, 44, 210, 16);
    addCaption("MODEL", gFilt, 90, 196, 60);
    addSeg(P::FMODEL, gFilt, { 140, 192, 184, 24 });

    auto* gVca = addGroup("VCA", 964, 64, 120, 252);
    addKnob(P::VCA_LEVEL, "Level", gVca, 60, 96, 32);
    addKnob(P::VCA_INIT, "Initial", gVca, 60, 196, 16)
        ->setTooltip("Initial gain: the VCA stays open this much with no envelope (drones)");

    auto* gOut = addGroup("Output", 1092, 64, 180, 252);
    addKnob(P::VOLUME, "Volume", gOut, 90, 100, 34);
    addToggle(P::LIMITER, "Limiter", gOut, 50, 190, 80, 24)->setTooltip("Master limiter");

    // ---- row 2: modulation generator, envelopes, expression ----
    auto* gMg = addGroup("Mod Generator", 8, 324, 224, 160);
    addView<pastel::LfoView>(gMg, { 12, 36, 200, 46 }, proc.apvts, P::MG_WAVE, juce::String(),
                         std::vector<Wave> { Wave::tri, Wave::saw, Wave::square, Wave::sine, Wave::snh });
    addSeg(P::MG_WAVE, gMg, { 12, 88, 200, 22 }, {}, { Wave::tri, Wave::saw, Wave::square, Wave::sine, Wave::snh });
    addKnob(P::MG_RATE, "Rate", gMg, 30, 134, 13, false, true);
    addToggle(P::MG_SYNC, "Sync", gMg, 150, 124, 62, 22)->setTooltip("Lock the rate to the host tempo");

    auto* eg1 = addView<pastel::EnvView>(addGroup("Envelope 1", 240, 324, 372, 160), { 12, 34, 348, 120 }, proc.apvts,
                                     juce::StringArray { P::EG1_A, P::EG1_D, P::EG1_S, P::EG1_R, P::EG1_DELAY },
                                     "PITCH / MOD", pastel::EnvView::Extra::delay);
    eg1->setTimeSkew(3.0f);
    eg1->setTooltip("Drag the points; the first one is the delay before the attack");
    auto* eg2 = addView<pastel::EnvView>(addGroup("Envelope 2", 620, 324, 372, 160), { 12, 34, 348, 120 }, proc.apvts,
                                     juce::StringArray { P::EG2_A, P::EG2_D, P::EG2_S, P::EG2_R, P::EG2_HOLD },
                                     "VCF / VCA", pastel::EnvView::Extra::hold);
    eg2->setTimeSkew(3.0f);
    eg2->setTooltip("Drag the points; the hold keeps the gate open at least this long");

    auto* gExp = addGroup("Express", 1000, 324, 272, 160);
    addKnob(P::VELSENS, "Velocity", gExp, 40, 74, 18);
    addKnob(P::BEND_RANGE, "Bend", gExp, 106, 74, 18);
    addToggle(P::PORTA_ON, "Glide", gExp, 150, 38, 60, 22)->setTooltip("Portamento on / off");
    addKnob(P::PORTA, "Time", gExp, 238, 62, 15);
    addSeg(P::GLIDE_MODE, gExp, { 150, 120, 110, 22 })->setTooltip("Glide only between legato notes, or always");

    // ---- row 3: modulation matrix as patch-cable rows ----
    auto* gMat = addGroup("Modulation Matrix", 8, 492, 760, 332);
    for (int r = 0; r < 8; ++r)
    {
        const int y = 40 + r * 35;
        MatrixRow row;
        auto* num = addCaption(juce::String(r + 1), gMat, 12, y + 4, 18, juce::Justification::centred);
        num->setColour(juce::Label::textColourId, MonoColors::faint);
        row.parts.push_back(num);
        row.parts.push_back(addChoice(P::modSrcId(r), gMat, 36, y, 150, 26));
        auto* amt = addBar(P::modAmtId(r), {}, gMat, 196, y + 1, 270, true);
        amt->setTooltip("Amount (double-click = 0)");
        row.parts.push_back(amt);
        auto* arrow = addCaption(juce::String::fromUTF8("\xe2\x86\x92"), gMat, 470, y + 4, 22, juce::Justification::centred);
        row.parts.push_back(arrow);
        row.parts.push_back(addChoice(P::modDstId(r), gMat, 496, y, 166, 26));
        row.parts.push_back(addSeg(P::modTypeId(r), gMat, { 670, y + 2, 78, 22 }, { "BI", "UNI" }));
        matrixRows.push_back(row);
    }

    // arpeggiator
    auto* gArp = addGroup("Arpeggiator", 776, 492, 200, 332);
    addPower(P::ARP_ON, gArp, 170, 6)->setTooltip("Arpeggiator on / off");
    addSeg(P::ARP_MODE, gArp, { 12, 40, 176, 24 }, { "UP", "DOWN", "U/D", "RND" });
    addChoice(P::ARP_DIV, gArp, 12, 72, 120, 24);
    addToggle(P::ARP_LATCH, "Latch", gArp, 138, 73, 50, 22)->setTooltip("Keep the pattern going after the keys go up");
    addKnob(P::ARP_OCT, "Octaves", gArp, 100, 150, 26);
    addKnob(P::ARP_GATE, "Gate", gArp, 52, 250, 22);
    addKnob(P::ARP_SWING, "Swing", gArp, 148, 250, 22);

    // effects: one row each; drag the pictures
    auto* gFx = addGroup("Effects", 984, 492, 288, 332);
    addCaption("SAT > CHO > DLY > REV", gFx, 120, 8, 156, juce::Justification::centredRight);
    {
        int y0 = 36;
        addCaption("DISTORTION", gFx, 12, y0, 120);
        addView<pastel::DriveView>(gFx, { 12, y0 + 16, 140, 52 }, proc.apvts, P::SAT_AMT, P::SAT_TYPE)
            ->setTooltip("Drag up / down: saturation amount");
        addSeg(P::SAT_TYPE, gFx, { 160, y0 + 30, 116, 22 }, { "TAPE", "TUBE", "FOLD", "FUZZ" });
        y0 += 72;
        addCaption("CHORUS", gFx, 12, y0, 120);
        addPower(P::CH_ON, gFx, 252, y0 - 6);
        addView<pastel::FxView>(gFx, { 12, y0 + 16, 264, 52 }, proc.apvts, pastel::FxView::Style::chorus,
                            P::CH_RATE, P::CH_DEPTH, P::CH_ON)->setTooltip("Drag: rate sideways, depth up / down");
        y0 += 72;
        addCaption("DELAY", gFx, 12, y0, 60);
        addPower(P::DL_ON, gFx, 252, y0 - 6);
        addView<pastel::FxView>(gFx, { 12, y0 + 16, 130, 52 }, proc.apvts, pastel::FxView::Style::delay,
                            P::DL_TIME, P::DL_FB, P::DL_ON, P::DL_SYNC, P::DL_DIV)
            ->setTooltip("Drag: time sideways (when not synced), feedback up / down");
        addToggle(P::DL_SYNC, "Sync", gFx, 150, y0 + 17, 46, 20)->setTooltip("Lock the delay time to the host tempo");
        dlDivBox = addChoice(P::DL_DIV, gFx, 200, y0 + 16, 76, 22);
        addCaption("MIX", gFx, 150, y0 + 48, 28);
        addBar(P::DL_MIX, {}, gFx, 176, y0 + 44, 100);
        y0 += 72;
        addCaption("REVERB", gFx, 12, y0, 60);
        addPower(P::RV_ON, gFx, 252, y0 - 6);
        addView<pastel::FxView>(gFx, { 12, y0 + 16, 264, 52 }, proc.apvts, pastel::FxView::Style::reverb,
                            P::RV_SIZE, P::RV_MIX, P::RV_ON, juce::String(), juce::String(), "SIZE", "MIX")
            ->setTooltip("Drag: size sideways, mix up / down");
    }

    // preset browser overlay (created last: paints above everything)
    browser = std::make_unique<PresetBrowser>(proc, lnf);
    addAndMakeVisible(browser.get());
    browser->setVisible(false);
    browser->onClose = [&] { browser->setVisible(false); };
    browser->onPresetChanged = [&] { refreshPresetName(); };

    timerCallback();
    startTimerHz(4);
}

MonoContent::~MonoContent() { setLookAndFeel(nullptr); }

void MonoContent::timerCallback()
{
    undoBtn.setEnabled(proc.canUndo());
    redoBtn.setEnabled(proc.canRedo());
    if (dlDivBox != nullptr)
        if (auto* r = proc.apvts.getRawParameterValue(P::DL_SYNC))
            dlDivBox->setVisible(r->load() > 0.5f);
    for (size_t i = 0; i < matrixRows.size(); ++i)
        if (auto* src = proc.apvts.getRawParameterValue(P::modSrcId((int)i)))
        {
            const float a = src->load() < 0.5f ? 0.4f : 1.0f;   // "---" = unpatched
            for (auto* c : matrixRows[i].parts)
                if (c->getAlpha() != a) c->setAlpha(a);
        }
    refreshPresetName();
}

juce::GroupComponent* MonoContent::addGroup(const juce::String& title, int x, int y, int w, int h)
{
    auto* g = new juce::GroupComponent(title, title);
    groups.emplace_back(g);
    addAndMakeVisible(g);
    g->setBounds(x, y, w, h);
    g->setLookAndFeel(&lnf);
    return g;
}

juce::Label* MonoContent::addCaption(const juce::String& text, juce::Component* parent, int x, int y, int w,
                                     juce::Justification just)
{
    auto* l = new juce::Label(text, text);
    labels.emplace_back(l);
    parent->addAndMakeVisible(l);
    l->setBounds(x, y, w, 18);
    l->setJustificationType(just);
    l->setBorderSize({});
    l->setColour(juce::Label::textColourId, MonoColors::textDim);
    l->setFont(lnf.titleFont(9.5f));
    l->setInterceptsMouseClicks(false, false);
    return l;
}

// value text for a knob / bar: the MG rate reads as a division while synced
static void installText(juce::Slider* s, MonoProcessor& proc, const juce::String& param)
{
    s->textFromValueFunction = [&proc, param](double v)
    {
        if (param == P::MG_RATE)
            if (auto* r = proc.apvts.getRawParameterValue(P::MG_SYNC); r != nullptr && r->load() > 0.5f)
                return juce::String(mgSyncNames[juce::jlimit(0, 10, (int)std::round(juce::jmap((float)v, 0.05f, 30.0f, 0.0f, 1.0f) * 10.0f))]);
        return monoFormat(param, (float)v);
    };
}

juce::Slider* MonoContent::addKnob(const juce::String& param, const juce::String& label, juce::Component* parent,
                                   int cx, int cy, int r, bool bipolar, bool beside)
{
    auto* s = new juce::Slider();
    knobs.emplace_back(s);
    parent->addAndMakeVisible(s);
    s->setBounds(cx - r - 3, cy - r - 3, 2 * r + 6, 2 * r + 6);
    s->setSliderStyle(juce::Slider::RotaryVerticalDrag);
    s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s->setLookAndFeel(&lnf);
    if (bipolar) s->getProperties().set("bipolar", true);
    if (auto* prm = proc.apvts.getParameter(param))
    {
        sAtt.add(new juce::AudioProcessorValueTreeState::SliderAttachment(proc.apvts, param, *s));
        s->setDoubleClickReturnValue(true, prm->convertFrom0to1(prm->getDefaultValue()));
        s->setPopupDisplayEnabled(true, true, this);
        s->setTooltip(prm->getName(64));
    }
    installText(s, proc, param); // after the attachment, which installs the raw text function
    auto mk = [&](const juce::String& txt, juce::Colour c, float fh, juce::Rectangle<int> b)
    {
        auto* l = new juce::Label({}, txt);
        labels.emplace_back(l);
        parent->addAndMakeVisible(l);
        l->setBounds(b);
        l->setJustificationType(beside ? juce::Justification::centredLeft : juce::Justification::centred);
        l->setBorderSize({});
        l->setColour(juce::Label::textColourId, c);
        l->setFont(lnf.titleFont(fh));
        l->setInterceptsMouseClicks(false, false);
        return l;
    };
    const auto nameB  = beside ? juce::Rectangle<int>(cx + r + 8, cy - 13, 90, 12) : juce::Rectangle<int>(cx - 44, cy + r + 4, 88, 12);
    const auto valueB = beside ? juce::Rectangle<int>(cx + r + 8, cy, 90, 14) : juce::Rectangle<int>(cx - 44, cy + r + 16, 88, 14);
    mk(label.toUpperCase(), MonoColors::textDim, 9.5f, nameB);
    auto* value = mk({}, MonoColors::text, 11.0f, valueB);
    s->onValueChange = [s, value] { value->setText(s->getTextFromValue(s->getValue()), juce::dontSendNotification); };
    s->onValueChange();
    return s;
}

juce::Slider* MonoContent::addFader(const juce::String& param, const juce::String& label, juce::Component* parent,
                                    int cx, int y, int h)
{
    auto* s = new juce::Slider();
    knobs.emplace_back(s);
    parent->addAndMakeVisible(s);
    s->setBounds(cx - 14, y, 28, h);
    s->setSliderStyle(juce::Slider::LinearVertical);
    s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s->setLookAndFeel(&lnf);
    if (auto* prm = proc.apvts.getParameter(param))
    {
        sAtt.add(new juce::AudioProcessorValueTreeState::SliderAttachment(proc.apvts, param, *s));
        s->setDoubleClickReturnValue(true, prm->convertFrom0to1(prm->getDefaultValue()));
        s->setTooltip(prm->getName(64));
    }
    installText(s, proc, param);
    auto* name = addCaption(label.toUpperCase(), parent, cx - 20, y + h + 2, 40, juce::Justification::centred);
    name->setFont(lnf.titleFont(9.0f));
    auto* value = addCaption({}, parent, cx - 20, y + h + 15, 40, juce::Justification::centred);
    value->setColour(juce::Label::textColourId, MonoColors::text);
    value->setFont(lnf.titleFont(10.5f));
    s->onValueChange = [s, value] { value->setText(s->getTextFromValue(s->getValue()), juce::dontSendNotification); };
    s->onValueChange();
    return s;
}

juce::Slider* MonoContent::addBar(const juce::String& param, const juce::String& label, juce::Component* parent,
                                  int x, int y, int w, bool bipolar)
{
    const int lw = label.isEmpty() ? 0 : 48;
    if (lw > 0) addCaption(label.toUpperCase(), parent, x, y + 3, lw);
    auto* s = new juce::Slider();
    knobs.emplace_back(s);
    parent->addAndMakeVisible(s);
    s->setBounds(x + lw, y, w - lw - 46, 24);
    s->setSliderStyle(juce::Slider::LinearHorizontal);
    s->setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s->setLookAndFeel(&lnf);
    if (bipolar) s->getProperties().set("bipolar", true);
    if (auto* prm = proc.apvts.getParameter(param))
    {
        sAtt.add(new juce::AudioProcessorValueTreeState::SliderAttachment(proc.apvts, param, *s));
        s->setDoubleClickReturnValue(true, prm->convertFrom0to1(prm->getDefaultValue()));
    }
    installText(s, proc, param);
    auto* value = addCaption({}, parent, x + w - 44, y + 3, 44, juce::Justification::centredRight);
    value->setColour(juce::Label::textColourId, MonoColors::text);
    value->setFont(lnf.titleFont(11.0f));
    s->onValueChange = [s, value] { value->setText(s->getTextFromValue(s->getValue()), juce::dontSendNotification); };
    s->onValueChange();
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

juce::ToggleButton* MonoContent::addPower(const juce::String& param, juce::Component* parent, int x, int y)
{
    auto* t = addToggle(param, {}, parent, x, y, 24, 24);
    t->getProperties().set("power", true);
    return t;
}

pastel::SegChoice* MonoContent::addSeg(const juce::String& param, juce::Component* parent, juce::Rectangle<int> b,
                                   juce::StringArray labelsIn, std::vector<pastel::Wave> icons)
{
    return addView<pastel::SegChoice>(parent, b, proc.apvts, param, labelsIn, std::move(icons));
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
    // the Pastel family mark: four rounded squares (the only colour on the panel)
    const juce::uint32 cols[4] = { 0xffe88a8a, 0xff8fd0c2, 0xffe9a3b8, 0xffa9d58e };
    for (int i = 0; i < 4; ++i)
    {
        g.setColour(juce::Colour(cols[i]));
        g.fillRoundedRectangle(18.0f + (float)(i % 2) * 10.0f, 17.0f + (float)(i / 2) * 10.0f, 8.0f, 8.0f, 2.0f);
    }
    g.setColour(MonoColors::text);
    g.setFont(lnf.logoFont(20.0f));
    g.drawText("MONOSYNTH", 44, 12, 200, 28, juce::Justification::centredLeft, false);
    g.setColour(MonoColors::border);
    g.fillRect(8, 52, getWidth() - 16, 1);
}

void MonoContent::resized()
{
    if (getWidth() <= 0) return;
    undoBtn.setBounds(196, 12, 28, 28);
    redoBtn.setBounds(228, 12, 28, 28);
    // preset strip centred: < [name] >
    int x = (baseW - (28 + 4 + 300 + 4 + 28)) / 2;
    prevBtn.setBounds(x, 12, 28, 28);   x += 32;
    nameBtn.setBounds(x, 12, 300, 28);  x += 304;
    nextBtn.setBounds(x, 12, 28, 28);
    int r = baseW - 8;
    for (auto* b : { &panicBtn, &initBtn, &locksBtn, &mutateBtn, &randBtn, &saveBtn })
    {
        const int w = b == &panicBtn ? 62 : (b == &randBtn || b == &mutateBtn ? 68 : 54);
        b->setBounds(r - w, 12, w, 28);
        r -= w + 4;
    }
    if (browser != nullptr)
        browser->setBounds(0, 0, baseW, baseH);
}

// ---------------- editor ----------------
MonoEditor::MonoEditor(MonoProcessor& p)
    : juce::AudioProcessorEditor(p), proc(p), content(p),
      corner(this, getConstrainer())
{
    addAndMakeVisible(content);   // content first...
    addAndMakeVisible(corner);    // ...corner above it
    setSize(MonoContent::baseW, MonoContent::baseH); // size LAST (lesson: layout fires synchronously)
    setResizable(true, true);
    setResizeLimits(768, 500, 2048, 1332);
    setWantsKeyboardFocus(true);
}

MonoEditor::~MonoEditor() {}

bool MonoEditor::keyPressed(const juce::KeyPress& k)
{
    const auto m = k.getModifiers();
    if (m.isCommandDown() && (k.getKeyCode() == 'Z' || k.getKeyCode() == 'z'))
    {
        if (m.isShiftDown()) proc.redoEdit(); else proc.undoEdit();
        content.refreshPresetName();
        return true;
    }
    return false;
}

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
