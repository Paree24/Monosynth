#include "PresetBrowser.h"
#include "PluginProcessor.h"

static const char* kBanks[] = { "All", "Bass", "Acid", "Lead", "Keys", "Drums", "FX", "User" };
static constexpr int kUserBank = 7;

void PresetBrowser::styleEditor(juce::TextEditor& ed, const juce::String& hint)
{
    ed.setLookAndFeel(&lnf);
    ed.setTextToShowWhenEmpty(hint, MonoColors::textDim.withAlpha(0.7f));
    ed.setColour(juce::TextEditor::backgroundColourId, MonoColors::control);
    ed.setColour(juce::TextEditor::textColourId, MonoColors::text);
    ed.setColour(juce::TextEditor::outlineColourId, MonoColors::border);
    ed.setColour(juce::TextEditor::focusedOutlineColourId, MonoColors::text);
    ed.setFont(lnf.uiFont(13.0f));
    ed.setIndents(8, 5);
    addAndMakeVisible(ed);
}

PresetBrowser::PresetBrowser(MonoProcessor& p, MonoLNF& l)
    : proc(p), lnf(l)
{
    setLookAndFeel(&lnf);
    setWantsKeyboardFocus(true);

    styleEditor(search, "Search presets...");
    search.addListener(this);

    for (int b = 0; b < 8; ++b)
    {
        auto* btn = new juce::TextButton(kBanks[b]);
        bankBtns.emplace_back(btn);
        addAndMakeVisible(btn);
        btn->setColour(juce::TextButton::textColourOffId, MonoColors::textDim);
        btn->setColour(juce::TextButton::textColourOnId, MonoColors::text);
        btn->onClick = [this, b] { activeBank = b; refreshBankButtons(); rebuildRows(); };
    }
    refreshBankButtons();

    list.setLookAndFeel(&lnf);
    list.setColour(juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setRowHeight(26);
    addAndMakeVisible(list);
    auto& sb = list.getVerticalScrollBar();
    sb.setColour(juce::ScrollBar::thumbColourId, MonoColors::textDim.withAlpha(0.7f));
    sb.setColour(juce::ScrollBar::trackColourId, juce::Colours::transparentBlack);
    sb.setColour(juce::ScrollBar::backgroundColourId, juce::Colours::transparentBlack);

    styleEditor(nameBox, "Preset name...");
    nameBox.setInputRestrictions(64);
    nameBox.onTextChange = [this] { setOverwriteArmed(false); };
    nameBox.onReturnKey = [this] { clickSave(); };

    closeBtn.onClick = [this] { if (onClose) onClose(); };
    saveBtn.onClick = [this] { clickSave(); };
    delBtn.onClick = [this]
    {
        int r = list.getSelectedRow();
        if (r < 0 || r >= (int)rows.size()) return;
        if (rows[(size_t)r].isUser)
        {
            if (deleteUserPreset(rows[(size_t)r].file)) rebuildRows();
        }
        else if (rows[(size_t)r].shadowed)
        {
            if (deleteFactoryShadow(rows[(size_t)r].index)) rebuildRows(); // restore shipped file
        }
    };
    overBtn.onClick = [this]
    {
        int r = list.getSelectedRow();
        if (r < 0 || r >= (int)rows.size() || rows[(size_t)r].isUser) return;
        int idx = rows[(size_t)r].index;
        if (overwriteFactoryPreset(idx, proc))
        {
            rebuildRows();
            for (int i = 0; i < (int)rows.size(); ++i)
                if (! rows[(size_t)i].isUser && rows[(size_t)i].index == idx)
                { selectQuietly(i); break; }
            if (onPresetChanged) onPresetChanged();
        }
    };
    for (auto* b : { &closeBtn, &saveBtn, &overBtn, &delBtn })
    {
        addAndMakeVisible(b);
        b->setColour(juce::TextButton::buttonColourId, MonoColors::raised);
        b->setColour(juce::TextButton::textColourOnId, MonoColors::text);
        b->setColour(juce::TextButton::textColourOffId, MonoColors::text);
    }
    rebuildRows();
}

PresetBrowser::~PresetBrowser()
{
    for (auto* ed : { &search, &nameBox }) ed->setLookAndFeel(nullptr);
    list.setLookAndFeel(nullptr);
    setLookAndFeel(nullptr);
}

// SAVE (inline, no pop-up): saves under the name field. An existing user
// preset needs a second click ("OVERWRITE?"); editing the name disarms it.
void PresetBrowser::clickSave()
{
    if (nameBox.getText().trim().isEmpty()) { nameBox.grabKeyboardFocus(); return; }
    auto name = sanitisePresetName(nameBox.getText());
    bool exists = getUserPresetDir().getChildFile(name + ".xml").existsAsFile();
    if (exists && ! overwriteArmed) { setOverwriteArmed(true); return; }
    setOverwriteArmed(false);
    saveAs(name);
}

void PresetBrowser::setOverwriteArmed(bool on)
{
    overwriteArmed = on;
    saveBtn.setButtonText(on ? "OVERWRITE?" : "SAVE");
    saveBtn.setColour(juce::TextButton::buttonColourId, on ? MonoColors::text.withAlpha(0.25f) : MonoColors::raised);
}

void PresetBrowser::saveAs(const juce::String& name)
{
    if (! saveUserPreset(name, proc)) return;
    activeBank = kUserBank;
    search.setText("", false);
    nameBox.setText(name, false);
    refreshBankButtons(); rebuildRows();
    for (int i = 0; i < (int)rows.size(); ++i)
        if (rows[(size_t)i].name == name) { selectQuietly(i); break; }
    list.repaint();
    if (onPresetChanged) onPresetChanged();
}

void PresetBrowser::openForSave()
{
    setVisible(true);
    nameBox.grabKeyboardFocus();
    nameBox.selectAll();
}

void PresetBrowser::visibilityChanged()
{
    if (isVisible())
    {
        // prefill with the current sound's name (not the generic init one)
        auto cur = proc.getCurrentPresetName();
        nameBox.setText(cur == "Init Patch" ? juce::String() : cur, false);
        setOverwriteArmed(false);
        rescanFactoryPresets();
        rebuildRows();
        // reveal the current sound in the list without re-loading it
        for (int i = 0; i < (int)rows.size(); ++i)
        {
            auto& r = rows[(size_t)i];
            bool hit = r.isUser ? (proc.getFactorySlot() < 0 && r.name == cur)
                                : (r.index == proc.getFactorySlot());
            if (hit) { selectQuietly(i); list.scrollToEnsureRowIsOnscreen(i); break; }
        }
        grabKeyboardFocus();
    }
}

bool PresetBrowser::keyPressed(const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { if (onClose) onClose(); return true; }
    return false;
}

void PresetBrowser::refreshBankButtons()
{
    for (size_t b = 0; b < bankBtns.size(); ++b)
    {
        bool on = ((int)b == activeBank);
        bankBtns[b]->setToggleState(on, juce::dontSendNotification);
        bankBtns[b]->setColour(juce::TextButton::textColourOffId, on ? MonoColors::text : MonoColors::textDim);
    }
}

void PresetBrowser::refreshActionButtons()
{
    int r = list.getSelectedRow();
    bool valid = r >= 0 && r < (int)rows.size();
    bool user = valid && rows[(size_t)r].isUser;
    overBtn.setEnabled(valid && ! user);
    delBtn.setEnabled(valid && (user || rows[(size_t)r].shadowed));
    delBtn.setButtonText(valid && ! user ? "RESTORE" : "DELETE");
}

void PresetBrowser::rebuildRows()
{
    rows.clear();
    juce::String q = search.getText().trim().toLowerCase();
    if (activeBank == kUserBank)
    {
        for (auto& u : scanUserPresets())
        {
            if (q.isNotEmpty() && ! u.name.toLowerCase().contains(q)) continue;
            rows.push_back({ true, -1, u.name, "User", u.file, false });
        }
    }
    else
    {
        for (auto& pr : getFactoryPresets())
        {
            if (activeBank != 0 && pr.bank != activeBank) continue;
            auto bank = bankName(pr.bank);
            if (q.isNotEmpty() && ! pr.name.toLowerCase().contains(q)
                && ! bank.toLowerCase().contains(q)) continue; // search matches bank too
            rows.push_back({ false, pr.index, pr.name + (pr.shadowed ? " *" : ""), bank, pr.file, pr.shadowed });
        }
    }
    list.updateContent();
    { const juce::ScopedValueSetter<bool> svs(quietSelect, true); list.deselectAllRows(); }
    list.repaint();
    refreshActionButtons();
}

int PresetBrowser::getNumRows() { return (int)rows.size(); }

void PresetBrowser::paintListBoxItem(int row, juce::Graphics& g, int w, int h, bool selected)
{
    if (row < 0 || row >= (int)rows.size()) return;
    auto& r = rows[(size_t)row];
    bool isCurrent = r.isUser ? (proc.getFactorySlot() < 0 && r.name == proc.getCurrentPresetName())
                              : (r.index == proc.getFactorySlot());
    if (selected)
    {
        g.setColour(MonoColors::text.withAlpha(0.16f));
        g.fillRoundedRectangle(2.0f, 1.0f, (float)w - 4.0f, (float)h - 2.0f, 3.0f);
    }
    g.setColour(isCurrent ? MonoColors::text : MonoColors::textDim.withAlpha(0.8f));
    g.setFont(lnf.uiFont(12.0f));
    juce::String num = r.isUser ? juce::String("USR") : juce::String(r.index).paddedLeft('0', 3);
    g.drawText(num, 10, 0, 40, h, juce::Justification::centredLeft, false);
    g.setColour(isCurrent ? MonoColors::text : MonoColors::textDim);
    g.setFont(isCurrent ? lnf.titleFont(13.0f) : lnf.uiFont(13.0f));
    g.drawText(r.name, 54, 0, w - 54 - 110, h, juce::Justification::centredLeft, true);
    g.setColour(MonoColors::textDim.withAlpha(0.7f));
    g.setFont(lnf.uiFont(12.0f));
    g.drawText(r.bank, w - 104, 0, 92, h, juce::Justification::centredRight, false);
}

void PresetBrowser::selectQuietly(int row)
{
    const juce::ScopedValueSetter<bool> svs(quietSelect, true);
    list.selectRow(row, true, true);
}

void PresetBrowser::selectedRowsChanged(int)
{
    refreshActionButtons();
    list.repaint();
    if (quietSelect) return;
    int r = list.getSelectedRow();
    if (r < 0 || r >= (int)rows.size()) return;
    auto& sel = rows[(size_t)r];
    if (sel.isUser) loadUserPreset(sel.file, proc);
    else proc.loadPreset(sel.index);
    if (onPresetChanged) onPresetChanged();
    list.repaint();
}

void PresetBrowser::listBoxItemDoubleClicked(int, const juce::MouseEvent&)
{
    if (onClose) onClose();
}

void PresetBrowser::textEditorTextChanged(juce::TextEditor&) { rebuildRows(); }

void PresetBrowser::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::black.withAlpha(0.55f)); // scrim
    auto pf = panel.toFloat();
    g.setColour(MonoColors::panel);
    g.fillRoundedRectangle(pf, 6.0f);
    g.setColour(MonoColors::raised);
    g.fillRoundedRectangle(pf.getX(), pf.getY(), pf.getWidth(), 34.0f, 6.0f);
    g.fillRect(pf.getX(), pf.getY() + 28.0f, pf.getWidth(), 6.0f);
    g.setColour(MonoColors::border);
    g.drawRoundedRectangle(pf.reduced(0.5f), 6.0f, 1.0f);
    // footer separator above the save row
    g.drawHorizontalLine(panel.getBottom() - 48, pf.getX() + 12.0f, pf.getRight() - 12.0f);
    g.setColour(MonoColors::text);
    g.setFont(lnf.titleFont(14.0f));
    g.drawText("PRESETS", panel.getX() + 14, panel.getY(), 200, 34, juce::Justification::centredLeft, false);
}

void PresetBrowser::resized()
{
    panel = juce::Rectangle<int>(0, 0, 700, 560).withCentre(getLocalBounds().getCentre());
    auto inner = panel.reduced(12, 0);
    closeBtn.setBounds(panel.getRight() - 38, panel.getY() + 5, 26, 24);
    search.setBounds(inner.getX(), panel.getY() + 46, inner.getWidth(), 28);
    const int gap = 6, n = (int)bankBtns.size();
    int bw = (inner.getWidth() - gap * (n - 1)) / n;
    int extra = inner.getWidth() - (bw * n + gap * (n - 1)); // spread rounding so row is flush both ends
    int x = inner.getX();
    for (int b = 0; b < n; ++b)
    {
        int w = bw + (b < extra ? 1 : 0);
        bankBtns[(size_t)b]->setBounds(x, panel.getY() + 82, w, 24);
        x += w + gap;
    }
    list.setBounds(inner.getX(), panel.getY() + 114, inner.getWidth(), panel.getBottom() - 56 - (panel.getY() + 114));
    int fy = panel.getBottom() - 38;
    nameBox.setBounds(inner.getX(), fy, 280, 28);
    saveBtn.setBounds(inner.getX() + 288, fy, 110, 28);
    delBtn.setBounds(inner.getRight() - 90, fy, 90, 28);
    overBtn.setBounds(inner.getRight() - 90 - 8 - 130, fy, 130, 28);
}

void PresetBrowser::mouseDown(const juce::MouseEvent& e)
{
    if (! panel.contains(e.getPosition()) && onClose) onClose(); // scrim click closes
}
