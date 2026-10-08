#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginEditor.h"
#include "Presets.h"

class MonoProcessor;

// Preset browser overlay: search + bank filter + audition list,
// factory data files plus user presets saved on disk.
class PresetBrowser : public juce::Component,
                      private juce::ListBoxModel,
                      private juce::TextEditor::Listener
{
public:
    explicit PresetBrowser(MonoProcessor& p, MonoLNF& lnf);
    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent& e) override;
    void visibilityChanged() override;

    std::function<void()> onClose;
    std::function<void()> onPresetChanged; // fired after any load (factory or user)
    juce::String lastLoadedName;
    bool lastLoadedIsUser = false;

private:
    struct Row { bool isUser = false; int index = -1; juce::String name; juce::File file; bool shadowed = false; };

    int getNumRows() override;
    void paintListBoxItem(int row, juce::Graphics&, int w, int h, bool selected) override;
    void selectedRowsChanged(int lastRow) override;
    void listBoxItemDoubleClicked(int row, const juce::MouseEvent&) override;
    void textEditorTextChanged(juce::TextEditor&) override;

    void rebuildRows();
    void refreshBankButtons();

    MonoProcessor& proc;
    MonoLNF& lnf;
    juce::TextEditor search;
    juce::ListBox list { "presets", this };
    juce::TextButton closeBtn { "X" };
    juce::TextButton saveBtn { "SAVE" };
    juce::TextButton overBtn { "OVER" };
    juce::TextButton delBtn { "DEL" };
    std::vector<std::unique_ptr<juce::TextButton>> bankBtns;
    std::vector<Row> rows;
    int activeBank = 0; // 0 all, 1..6 factory banks, 7 user
    int currentFactory = -1;
    juce::Rectangle<int> panel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PresetBrowser)
};
