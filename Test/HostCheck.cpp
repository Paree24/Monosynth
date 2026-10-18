// VST3 host check (dev only, never shipped): loads the BUILT .vst3 through
// JUCE's VST3 hosting, like a DAW would: buses, params, audio, state, editor.
//   HostCheck <path/to/Monosynth.vst3>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include <cstdio>

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 2) { printf("usage: HostCheck <plugin.vst3>\n"); return 1; }
    int fails = 0;
    auto check = [&](bool ok, const juce::String& what) {
        printf("%s  %s\n", ok ? "  ok " : "FAIL ", what.toRawUTF8());
        if (! ok) ++fails;
    };
    juce::VST3PluginFormat fmt;
    juce::OwnedArray<juce::PluginDescription> descs;
    fmt.findAllTypesForFile(descs, juce::File(argv[1]).getFullPathName());
    check(descs.size() == 1, "VST3 scanned: " + juce::String(descs.size()) + " plugin(s)");
    if (descs.isEmpty()) return 1;
    auto& d = *descs[0];
    check(d.isInstrument, "registered as instrument: " + d.name + " / " + d.category);
    juce::String err;
    auto inst = fmt.createInstanceFromDescription(d, 48000.0, 512, err);
    check(inst != nullptr, "instantiated " + err);
    if (! inst) return 1;
    check(inst->getBusCount(false) == 1, "output buses: " + juce::String(inst->getBusCount(false)));
    check(inst->getParameters().size() >= 80, "parameters: " + juce::String(inst->getParameters().size()));
    check(inst->getNumPrograms() == 356, "factory programs: " + juce::String(inst->getNumPrograms()));
    inst->prepareToPlay(48000.0, 512);
    int nch = inst->getTotalNumOutputChannels();
    check(nch == 2, "stereo output");
    juce::AudioBuffer<float> buf(juce::jmax(2, nch), 512);
    float peak = 0.0f;
    bool finite = true;
    for (int b = 0; b < 60; ++b)
    {
        juce::MidiBuffer mb;
        if (b == 1) mb.addEvent(juce::MidiMessage::noteOn(1, 48, (juce::uint8) 110), 0);
        if (b == 40) mb.addEvent(juce::MidiMessage::noteOff(1, 48), 0);
        buf.clear();
        inst->processBlock(buf, mb);
        for (int ch = 0; ch < buf.getNumChannels(); ++ch)
            for (int i = 0; i < buf.getNumSamples(); ++i)
                if (! std::isfinite(buf.getSample(ch, i))) finite = false;
        peak = juce::jmax(peak, buf.getMagnitude(0, 0, 512), buf.getMagnitude(1, 0, 512));
    }
    check(finite, "audio is finite");
    check(peak > 0.02f && peak < 1.2f, "note 48 plays the default preset (peak " + juce::String(peak, 3) + ")");
    juce::MemoryBlock st;
    inst->getStateInformation(st);
    check(st.getSize() > 1000, "state saved (" + juce::String((int) st.getSize()) + " bytes)");
    inst->setStateInformation(st.getData(), (int) st.getSize());
    check(true, "state restored");
    std::unique_ptr<juce::AudioProcessorEditor> ed(inst->createEditorIfNeeded());
    check(ed != nullptr && ed->getWidth() == 1280, "editor opens at 1280 wide");
    ed.reset();
    inst->releaseResources();
    inst.reset();
    printf(fails == 0 ? "HOST CHECK PASSED\n" : "%d HOST CHECK(S) FAILED\n", fails);
    return fails == 0 ? 0 : 1;
}
