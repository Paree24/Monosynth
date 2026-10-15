#include "PluginProcessor.h"
#include "PluginEditor.h"

static juce::StringArray choiceLabels(std::initializer_list<const char*> v)
{
    juce::StringArray s; for (auto c : v) s.add(c); return s;
}

juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> p;
    auto af = [&](const char* id, const char* name, float lo, float hi, float def) {
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(id, 1), name,
            juce::NormalisableRange<float>(lo, hi, 0.0f, hi > 100.0f ? 0.35f : 1.0f), def));
    };
    auto cf = [&](const char* id, const char* name, juce::StringArray labels, int def) {
        p.push_back(std::make_unique<juce::AudioParameterChoice>(juce::ParameterID(id, 1), name, labels, def));
    };
    auto bf = [&](const char* id, const char* name, bool def) {
        p.push_back(std::make_unique<juce::AudioParameterBool>(juce::ParameterID(id, 1), name, def));
    };

    cf(P::VCO1_WAVE, "VCO1 Wave", choiceLabels({"Saw", "Triangle", "Square", "Pulse", "Sine"}), 0);
    cf(P::VCO1_OCT, "VCO1 Octave", choiceLabels({"-2", "-1", "0", "+1", "+2"}), 2);
    af(P::VCO1_PITCH, "VCO1 Pitch", -12.0f, 12.0f, 0.0f);
    af(P::VCO1_TUNE, "VCO1 Tune", -100.0f, 100.0f, 0.0f);
    af(P::VCO1_PW, "VCO1 PW", 0.05f, 0.95f, 0.5f);

    cf(P::VCO2_WAVE, "VCO2 Wave", choiceLabels({"Saw", "Triangle", "Square", "Pulse", "Sine"}), 2);
    cf(P::VCO2_OCT, "VCO2 Octave", choiceLabels({"-2", "-1", "0", "+1", "+2"}), 2);
    af(P::VCO2_PITCH, "VCO2 Pitch", -12.0f, 12.0f, 0.0f);
    af(P::VCO2_TUNE, "VCO2 Tune", -100.0f, 100.0f, 0.0f);
    af(P::VCO2_PW, "VCO2 PW", 0.05f, 0.95f, 0.5f);

    af(P::MIX_VCO1, "VCO1 Level", 0.0f, 1.0f, 0.8f);
    af(P::MIX_VCO2, "VCO2 Level", 0.0f, 1.0f, 0.0f);
    af(P::MIX_RING, "Ring Level", 0.0f, 1.0f, 0.0f);
    af(P::MIX_NOISE, "Noise Level", 0.0f, 1.0f, 0.0f);
    af(P::THICK, "Thickness", 0.0f, 1.0f, 0.0f);
    cf(P::NOISE_TYPE, "Noise Type", choiceLabels({"White", "Pink"}), 0);

    af(P::HPF_CUT, "HPF Cutoff", 10.0f, 18000.0f, 10.0f);
    af(P::HPF_PEAK, "HPF Peak", 0.0f, 1.0f, 0.0f);
    af(P::LPF_CUT, "LPF Cutoff", 20.0f, 19000.0f, 19000.0f);
    af(P::LPF_RES, "LPF Resonance", 0.0f, 1.0f, 0.0f);
    af(P::DRIVE, "Drive", 0.0f, 1.0f, 0.0f);
    cf(P::FMODEL, "Filter Model", choiceLabels({"Early", "Late"}), 0);
    af(P::SAT_AMT, "Saturation", 0.0f, 1.0f, 0.0f);
    cf(P::SAT_TYPE, "Sat Type", choiceLabels({"Tape", "Tube", "Fold", "Fuzz"}), 0);

    af(P::VCA_LEVEL, "VCA Level", 0.0f, 1.2f, 0.8f);
    af(P::VOLUME, "Volume", 0.0f, 1.2f, 0.5f); // -6 dB default
    bf(P::LIMITER, "Limiter", true);
    af(P::PORTA, "Portamento", 0.0f, 1.0f, 0.0f);
    bf(P::PORTA_ON, "Portamento On", false);
    cf(P::GLIDE_MODE, "Glide Mode", choiceLabels({"Legato", "Always"}), 0);

    cf(P::MG_WAVE, "MG Wave", choiceLabels({"Triangle", "Saw", "Square", "Sine", "S&H"}), 0);
    af(P::MG_RATE, "MG Rate", 0.05f, 30.0f, 4.0f);
    bf(P::MG_SYNC, "MG Sync", false);

    af(P::EG1_A, "EG1 Attack", 0.001f, 4.0f, 0.001f);
    af(P::EG1_D, "EG1 Decay", 0.01f, 8.0f, 8.0f);
    af(P::EG1_S, "EG1 Sustain", 0.0f, 1.0f, 1.0f);
    af(P::EG1_R, "EG1 Release", 0.01f, 10.0f, 0.01f);
    af(P::EG2_A, "EG2 Attack", 0.001f, 4.0f, 0.001f);
    af(P::EG2_D, "EG2 Decay", 0.01f, 8.0f, 8.0f);
    af(P::EG2_S, "EG2 Sustain", 0.0f, 1.0f, 1.0f);
    af(P::EG2_R, "EG2 Release", 0.01f, 10.0f, 0.01f);

    af(P::BEND_RANGE, "Bend Range", 0.0f, 12.0f, 2.0f);
    af(P::VELSENS, "Velocity Sens", 0.0f, 1.0f, 0.5f);

    auto srcLabels = choiceLabels({"---", "MG", "EG1", "EG2", "Velocity", "Aftertouch",
                                   "Key Track", "Mod Wheel", "Pitch Wheel", "Random", "Note On"});
    auto dstLabels = choiceLabels({"---", "VCO1 Pitch", "VCO2 Pitch", "VCO1 PW", "VCO2 PW",
                                   "HPF Cutoff", "LPF Cutoff", "HPF Peak", "LPF Reso", "VCA Level"});
    auto typLabels = choiceLabels({"Bipolar", "Unipolar"});
    // Init patch = neutral instrument: no matrix routing at all (static,
    // wobble-free); the amp envelope still gates via EG2 directly.
    const float defAmt[8] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    const int defSrc[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    const int defDst[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
    const int defTyp[8] = { 0, 0, 0, 1, 1, 0, 0, 0 };
    for (int i = 0; i < P::NMOD; ++i)
    {
        juce::String sid = P::modSrcId(i), aid = P::modAmtId(i);
        juce::String did = P::modDstId(i), tid = P::modTypeId(i);
        p.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(sid, 1), "Mod " + juce::String(i + 1) + " Source", srcLabels, defSrc[i]));
        p.push_back(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID(aid, 1), "Mod " + juce::String(i + 1) + " Amount",
            juce::NormalisableRange<float>(-1.0f, 1.0f), defAmt[i]));
        p.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(did, 1), "Mod " + juce::String(i + 1) + " Destination", dstLabels, defDst[i]));
        p.push_back(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID(tid, 1), "Mod " + juce::String(i + 1) + " Type", typLabels, defTyp[i]));
    }
    // Master FX (simple controls; all off = dry init)
    bf(P::CH_ON, "Chorus On", false);
    af(P::CH_RATE, "Chorus Rate", 0.05f, 8.0f, 0.6f);
    af(P::CH_DEPTH, "Chorus Depth", 0.0f, 1.0f, 0.4f);
    bf(P::DL_ON, "Delay On", false);
    af(P::DL_TIME, "Delay Time", 20.0f, 1500.0f, 350.0f);
    af(P::DL_FB, "Delay FB", 0.0f, 0.85f, 0.35f);
    af(P::DL_MIX, "Delay Mix", 0.0f, 1.0f, 0.25f);
    bf(P::RV_ON, "Reverb On", false);
    af(P::RV_SIZE, "Reverb Size", 0.0f, 1.0f, 0.5f);
    af(P::RV_MIX, "Reverb Mix", 0.0f, 1.0f, 0.25f);
    return { p.begin(), p.end() };
}

MonoProcessor::MonoProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "MONO", createParameterLayout())
{
    auto raw = [&](const juce::String& id) {
        auto* v = apvts.getRawParameterValue(id);
        jassert(v != nullptr);
        return v;
    };
    pp.vco1wave = raw(P::VCO1_WAVE); pp.vco1oct = raw(P::VCO1_OCT); pp.vco1pitch = raw(P::VCO1_PITCH);
    pp.vco1tune = raw(P::VCO1_TUNE); pp.vco1pw = raw(P::VCO1_PW);
    pp.vco2wave = raw(P::VCO2_WAVE); pp.vco2oct = raw(P::VCO2_OCT); pp.vco2pitch = raw(P::VCO2_PITCH);
    pp.vco2tune = raw(P::VCO2_TUNE); pp.vco2pw = raw(P::VCO2_PW);
    pp.mix1 = raw(P::MIX_VCO1); pp.mix2 = raw(P::MIX_VCO2); pp.ring = raw(P::MIX_RING);
    pp.noise = raw(P::MIX_NOISE); pp.thick = raw(P::THICK); pp.noiseType = raw(P::NOISE_TYPE);
    pp.hpfCut = raw(P::HPF_CUT); pp.hpfPeak = raw(P::HPF_PEAK); pp.lpfCut = raw(P::LPF_CUT);
    pp.lpfRes = raw(P::LPF_RES); pp.drive = raw(P::DRIVE); pp.fmodel = raw(P::FMODEL);
    pp.satAmt = raw(P::SAT_AMT); pp.satType = raw(P::SAT_TYPE);
    pp.vca = raw(P::VCA_LEVEL); pp.volume = raw(P::VOLUME); pp.limiter = raw(P::LIMITER);
    pp.porta = raw(P::PORTA); pp.portaOn = raw(P::PORTA_ON); pp.glideMode = raw(P::GLIDE_MODE);
    pp.mgWave = raw(P::MG_WAVE); pp.mgRate = raw(P::MG_RATE); pp.mgSync = raw(P::MG_SYNC);
    pp.eg1a = raw(P::EG1_A); pp.eg1d = raw(P::EG1_D); pp.eg1s = raw(P::EG1_S); pp.eg1r = raw(P::EG1_R);
    pp.eg2a = raw(P::EG2_A); pp.eg2d = raw(P::EG2_D); pp.eg2s = raw(P::EG2_S); pp.eg2r = raw(P::EG2_R);
    pp.bendRange = raw(P::BEND_RANGE); pp.velSens = raw(P::VELSENS);
    pp.chOn = raw(P::CH_ON); pp.chRate = raw(P::CH_RATE); pp.chDepth = raw(P::CH_DEPTH);
    pp.dlOn = raw(P::DL_ON); pp.dlTime = raw(P::DL_TIME); pp.dlFb = raw(P::DL_FB); pp.dlMix = raw(P::DL_MIX);
    pp.rvOn = raw(P::RV_ON); pp.rvSize = raw(P::RV_SIZE); pp.rvMix = raw(P::RV_MIX);
    for (int i = 0; i < P::NMOD; ++i)
    {
        pp.modSrc[i] = raw(P::modSrcId(i)); pp.modAmt[i] = raw(P::modAmtId(i));
        pp.modDst[i] = raw(P::modDstId(i)); pp.modTyp[i] = raw(P::modTypeId(i));
    }
    heldNotes.reserve(128); // never reallocates on the audio thread
    rng.setSeedRandomly();
    loadPreset(0);
}

MonoProcessor::~MonoProcessor() {}

void MonoProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    cachedSr = sampleRate > 0.0 ? sampleRate : 44100.0;
    spec.sampleRate = cachedSr;
    spec.maximumBlockSize = (juce::uint32) samplesPerBlock;
    spec.numChannels = 2;
    voice.prepare(cachedSr);
    mg.setSampleRate(cachedSr);
    chorus.prepare(cachedSr, samplesPerBlock);
    delay.prepare(cachedSr);
    verb.prepare(cachedSr);
    spec.numChannels = 1;
    auto hp = juce::dsp::IIR::Coefficients<float>::makeHighPass(cachedSr, 12.0f, 0.7071f);
    for (auto& f : dcBlock) { f.coefficients = hp; f.prepare(spec); f.reset(); }
    fxWasOn[0] = fxWasOn[1] = fxWasOn[2] = false;
    MonoDSP::SynthParams d0 = collectParams();
    smHpf = d0.hpfCut; smHpfPk = d0.hpfPeak; smLpf = d0.lpfCut; smLpfRs = d0.lpfRes;
    smDrv = d0.drive; smSat = d0.satAmt; smThick = d0.thick; smM1 = d0.mix1; smM2 = d0.mix2; smRing = d0.ring; smNz = d0.noise;
    smVol = d0.volume; smVca = d0.vca;
    limGain = 1.0f; limPeak = 0.0f; mgSmooth = 0.0f; bendSm = bendSemis;
}

void MonoProcessor::releaseResources() {}

MonoDSP::SynthParams MonoProcessor::collectParams()
{
    MonoDSP::SynthParams p;
    auto f = [](std::atomic<float>* v) { return v->load(); };
    auto i = [](std::atomic<float>* v) { return (int) std::round(v->load()); };
    auto b = [](std::atomic<float>* v) { return v->load() > 0.5f; };
    p.vco1wave = i(pp.vco1wave); p.vco1oct = i(pp.vco1oct);
    p.vco1pitch = f(pp.vco1pitch); p.vco1tune = f(pp.vco1tune); p.vco1pw = f(pp.vco1pw);
    p.vco2wave = i(pp.vco2wave); p.vco2oct = i(pp.vco2oct);
    p.vco2pitch = f(pp.vco2pitch); p.vco2tune = f(pp.vco2tune); p.vco2pw = f(pp.vco2pw);
    p.mix1 = f(pp.mix1); p.mix2 = f(pp.mix2);
    p.ring = f(pp.ring); p.noise = f(pp.noise);
    p.thick = f(pp.thick);
    p.noiseType = i(pp.noiseType);
    p.hpfCut = f(pp.hpfCut); p.hpfPeak = f(pp.hpfPeak);
    p.lpfCut = f(pp.lpfCut); p.lpfRes = f(pp.lpfRes);
    p.drive = f(pp.drive); p.fmodel = i(pp.fmodel);
    p.satAmt = f(pp.satAmt); p.satType = i(pp.satType);
    p.vca = f(pp.vca); p.volume = f(pp.volume); p.limiter = b(pp.limiter);
    p.porta = f(pp.porta); p.portaOn = b(pp.portaOn); p.glideMode = i(pp.glideMode);
    p.mgWave = i(pp.mgWave); p.mgRate = f(pp.mgRate); p.mgSync = b(pp.mgSync);
    p.eg1a = f(pp.eg1a); p.eg1d = f(pp.eg1d); p.eg1s = f(pp.eg1s); p.eg1r = f(pp.eg1r);
    p.eg2a = f(pp.eg2a); p.eg2d = f(pp.eg2d); p.eg2s = f(pp.eg2s); p.eg2r = f(pp.eg2r);
    p.bendRange = f(pp.bendRange); p.velSens = f(pp.velSens);
    p.chOn = b(pp.chOn); p.chRate = f(pp.chRate); p.chDepth = f(pp.chDepth);
    p.dlOn = b(pp.dlOn); p.dlTime = f(pp.dlTime); p.dlFb = f(pp.dlFb); p.dlMix = f(pp.dlMix);
    p.rvOn = b(pp.rvOn); p.rvSize = f(pp.rvSize); p.rvMix = f(pp.rvMix);
    for (int k = 0; k < P::NMOD; ++k)
    {
        auto& r = p.routes[(size_t) k];
        r.src = i(pp.modSrc[k]); r.amt = f(pp.modAmt[k]);
        r.dst = i(pp.modDst[k]); r.type = i(pp.modTyp[k]);
    }
    p.pitchBend = bendSemis;
    return p;
}

void MonoProcessor::updateHeld(int note, bool on)
{
    // kept in press order: releasing a note returns to the most recent one
    // still held (last-note priority, matching how new notes always sound)
    heldNotes.erase(std::remove(heldNotes.begin(), heldNotes.end(), note), heldNotes.end());
    if (on) heldNotes.push_back(note);
}

void MonoProcessor::allNotesOff()
{
    heldNotes.clear();
    voice.noteOff();
}

void MonoProcessor::handleMidi(const juce::MidiMessage& msg, const MonoDSP::SynthParams& tmp)
{
    if (msg.isNoteOn())
    {
        int n = msg.getNoteNumber();
        float vel = msg.getFloatVelocity();
        bool trueLegato = ! heldNotes.empty();
        updateHeld(n, true);
        bool wasActive = voice.active;
        bool glide = false;
        if (tmp.portaOn && wasActive)
            glide = (tmp.glideMode == 1) ? true : trueLegato;
        // detached notes always re-articulate; overlapping legato slurs
        voice.noteOn(n, vel, tmp, glide, ! trueLegato || ! wasActive);
    }
    else if (msg.isNoteOff())
    {
        int n = msg.getNoteNumber();
        updateHeld(n, false);
        if (! heldNotes.empty())
        {
            int last = heldNotes.back();
            voice.note = last;
            voice.setTarget((float) last + bendSm);
        }
        else voice.noteOff();
    }
    else if (msg.isPitchWheel())
    {
        int v = msg.getPitchWheelValue();
        if (std::abs(v - 8192) <= 48) v = 8192;
        float range = tmp.bendRange;
        bendSemis = ((float) v - 8192.0f) / 8192.0f * range;
        pitchWheelSrc = ((float) v - 8192.0f) / 8192.0f;
    }
    // channel-mode messages are controllers too: test them before the
    // generic controller branch or they never fire
    else if (msg.isAllSoundOff())
    {
        heldNotes.clear();
        voice.kill();
    }
    else if (msg.isAllNotesOff()) allNotesOff();
    else if (msg.isResetAllControllers())
    {
        modWheel = 0.0f; aftertouch = 0.0f;
        bendSemis = 0.0f; pitchWheelSrc = 0.0f;
    }
    else if (msg.isController())
    {
        if (msg.getControllerNumber() == 1) modWheel = (float) msg.getControllerValue() / 127.0f;
    }
    else if (msg.isChannelPressure()) aftertouch = (float) msg.getChannelPressureValue() / 127.0f;
    else if (msg.isAftertouch())
    {
        if (msg.getNoteNumber() == voice.note) aftertouch = (float) msg.getAfterTouchValue() / 127.0f;
    }
}

static float mgRateHz(float knob, bool sync, double bpm)
{
    if (! sync) return knob;
    static constexpr float divs[] = { 0.25f, 0.333f, 0.5f, 0.667f, 0.75f, 1.0f, 1.333f, 1.5f, 2.0f, 3.0f, 4.0f };
    float beats = (float) (bpm / 60.0);
    float t = juce::jmap(knob, 0.05f, 30.0f, 0.0f, 1.0f);
    int idx = juce::jlimit(0, 10, (int) std::round(t * 10.0f));
    return beats * divs[idx];
}

// Transparent safety ceiling: bit-exact below 0 dBFS, compresses only above.
// (A tanh backstop would audibly round healthy signals; this one doesn't.)
static inline float softCeil(float x)
{
    float ax = std::fabs(x);
    if (ax <= 1.0f) return x;
    float over = ax - 1.0f;
    return (x > 0.0f ? 1.0f : -1.0f) * (1.0f + over / (1.0f + over * over));
}

void MonoProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi){
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();
    if (pendingPanic.exchange(false))
    {
        heldNotes.clear();
        bendSemis = 0.0f; bendSm = 0.0f; pitchWheelSrc = 0.0f;
        modWheel = 0.0f; aftertouch = 0.0f;
        voice.kill();
        chorus.clear(); delay.clear(); verb.clear();
    }
    if (pendingAllOff.exchange(false))
    {
        allNotesOff();
        bendSemis = 0.0f; pitchWheelSrc = 0.0f;
    }
    MonoDSP::SynthParams p = collectParams();
    if (auto* ph = getPlayHead())
        if (auto pos = ph->getPosition())
            if (pos->getBpm()) hostBpm = *pos->getBpm();

    auto midiIt = midi.cbegin();
    const auto midiEnd = midi.cend();

    int numSamples = buffer.getNumSamples();
    float* outL = buffer.getWritePointer(0);
    float* outR = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

    float smK = 1.0f - std::exp(-1.0f / (0.008f * (float) cachedSr));
    float mgK = 1.0f - std::exp(-1.0f / (0.004f * (float) cachedSr));
    float rateHz = mgRateHz(p.mgRate, p.mgSync, hostBpm);

    // Snapshot smoother targets BEFORE the loop (never read-then-overwrite p.* inside).
    const float tHpf = p.hpfCut, tHpfPk = p.hpfPeak, tLpf = p.lpfCut, tLpfRs = p.lpfRes;
    const float tDrv = p.drive, tM1 = p.mix1, tM2 = p.mix2, tRing = p.ring, tNz = p.noise;
    const float tVol = p.volume, tVca = p.vca, tSat = p.satAmt, tThk = p.thick;
    voice.setModel(p.fmodel);
    voice.updateEnvelopes(p);

    for (int s = 0; s < numSamples; ++s)
    {
        for (; midiIt != midiEnd && (*midiIt).samplePosition <= s; ++midiIt)
            handleMidi((*midiIt).getMessage(), p);

        smHpf += (tHpf - smHpf) * smK;         p.hpfCut = smHpf;
        smHpfPk += (tHpfPk - smHpfPk) * smK;   p.hpfPeak = smHpfPk;
        smLpf += (tLpf - smLpf) * smK;         p.lpfCut = smLpf;
        smLpfRs += (tLpfRs - smLpfRs) * smK;   p.lpfRes = smLpfRs;
        smDrv += (tDrv - smDrv) * smK;         p.drive = smDrv;
        smSat += (tSat - smSat) * smK;         p.satAmt = smSat;
        smThick += (tThk - smThick) * smK;     p.thick = smThick;
        smM1 += (tM1 - smM1) * smK;            p.mix1 = smM1;
        smM2 += (tM2 - smM2) * smK;            p.mix2 = smM2;
        smRing += (tRing - smRing) * smK;      p.ring = smRing;
        smNz += (tNz - smNz) * smK;            p.noise = smNz;
        smVol += (tVol - smVol) * smK;         p.volume = smVol;
        smVca += (tVca - smVca) * smK;         p.vca = smVca;
        bendSm += (bendSemis - bendSm) * smK;  p.pitchBend = bendSm;

        if (voice.active) voice.setTarget((float) voice.note + bendSm);

        float mgRaw = mg.next(rateHz, p.mgWave, rng);
        mgSmooth += (mgRaw - mgSmooth) * mgK;

        float e1 = 0.0f, e2 = 0.0f, vs = 0.0f;
        if (voice.active)
            vs = voice.render(p, mgSmooth, aftertouch, modWheel, pitchWheelSrc, e1, e2);
        float g = vs * p.volume * 0.9f;
        outL[s] = g;
        if (outR) outR[s] = g;
    }
    for (; midiIt != midiEnd; ++midiIt) // stray events past the block end
        handleMidi((*midiIt).getMessage(), p);
    midi.clear();

    // Master FX: chorus -> delay -> reverb (each bypassed when off/mix 0).
    // Enabling an effect starts from silence, not from the tail that was
    // frozen in its buffer when it was switched off.
    if (outR != nullptr)
    {
        if (p.chOn && ! fxWasOn[0]) chorus.clear();
        if (p.dlOn && ! fxWasOn[1]) delay.clear();
        if (p.rvOn && ! fxWasOn[2]) verb.clear();
        fxWasOn[0] = p.chOn; fxWasOn[1] = p.dlOn; fxWasOn[2] = p.rvOn;
        if (p.chOn) chorus.process(outL, outR, numSamples, p.chRate, p.chDepth, 0.5f);
        if (p.dlOn) delay.process(outL, outR, numSamples, p.dlTime, p.dlFb, p.dlMix);
        if (p.rvOn) verb.process(outL, outR, numSamples, p.rvSize, 0.5f, p.rvMix);
    }

    // 12 Hz cleanup (DC + subsonics; per channel), then musical peak limiter + idle gate.
    // A single stage kills DC dead with minimal phase dispersion in the audio
    // band (extra cascaded stages audibly tilt saw/square waves on a scope
    // for no musical benefit).
    {
        juce::dsp::AudioBlock<float> block(buffer);
        for (int ch = 0; ch < juce::jmin(2, buffer.getNumChannels()); ++ch)
        {
            auto chBlock = block.getSingleChannelBlock((size_t) ch);
            juce::dsp::ProcessContextReplacing<float> ctx(chBlock);
            dcBlock[ch].process(ctx);
        }
    }
    {
        float att = 1.0f - std::exp(-1.0f / (0.0003f * (float) cachedSr));
        float rel = 1.0f - std::exp(-1.0f / (0.080f * (float) cachedSr));
        float gateStep = 1.0f / (0.050f * (float) cachedSr);
        const float thr = 0.7f;
        bool limOn = p.limiter;
        bool anyActive = voice.active;
        if (anyActive) idleSamples = 0; else idleSamples += numSamples;
        bool shouldClose = idleSamples > (int) (6.0 * cachedSr);
        int nch = buffer.getNumChannels();
        bool wasOpen = gateG > 0.0f;
        for (int s = 0; s < numSamples; ++s)
        {
            if (shouldClose && gateG > 0.0f) gateG = juce::jmax(0.0f, gateG - gateStep);
            else if (! shouldClose) gateG = 1.0f;
            if (limOn)
            {
                float a = 0.0f;
                for (int ch = 0; ch < nch; ++ch) a = juce::jmax(a, std::fabs(buffer.getSample(ch, s)));
                if (a > limPeak) limPeak = a;
                else limPeak += (0.0f - limPeak) * rel;
                float target = (limPeak > thr) ? thr / limPeak : 1.0f;
                limGain += (target - limGain) * (target < limGain ? att : rel);
                for (int ch = 0; ch < nch; ++ch)
                    buffer.setSample(ch, s, softCeil(buffer.getSample(ch, s) * limGain) * gateG);
            }
            else
            {
                limPeak = 0.0f; limGain = 1.0f;
                if (gateG != 1.0f)
                    for (int ch = 0; ch < nch; ++ch)
                        buffer.setSample(ch, s, buffer.getSample(ch, s) * gateG);
            }
        }
        // gate fully closed: drop the muted FX tails so they don't resurface
        // when the next note reopens it
        if (wasOpen && gateG <= 0.0f) { chorus.clear(); delay.clear(); verb.clear(); }
    }
}

void MonoProcessor::setCurrentProgram(int index)
{
    if (index >= 0 && index < (int) getFactoryPresets().size()) loadPreset(index);
}

const juce::String MonoProcessor::getProgramName(int index) { return presetName(index); }

void MonoProcessor::setPresetIdentity(int factoryIndex, const juce::String& name)
{
    currentPreset = factoryIndex;
    currentName = name;
}

void MonoProcessor::resetParamsToDefaults()
{
    for (auto* param : getParameters())
        if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(param))
            r->setValueNotifyingHost(r->getDefaultValue());
}

void MonoProcessor::loadPreset(int index)
{
    int n = juce::jmax(1, (int) getFactoryPresets().size());
    index = juce::jlimit(0, n - 1, index);
    if (! applyPreset(*this, index)) // sets slot + name on success
    {
        resetParamsToDefaults();
        setPresetIdentity(-1, "Init Patch");
    }
}

void MonoProcessor::loadInit()
{
    resetParamsToDefaults();
    setPresetIdentity(-1, "Init Patch");
    pendingAllOff = true; // serviced on the audio thread
}

void MonoProcessor::panic() { pendingPanic = true; }

juce::ValueTree MonoProcessor::presetState()
{
    auto state = apvts.copyState();
    state.setProperty("presetName", currentName, nullptr);
    return state;
}

// Reset to defaults, then apply every stored param (unknown ids ignored).
// Files store denormalised real units.
bool MonoProcessor::loadPresetState(const juce::ValueTree& vt)
{
    if (! vt.isValid()) return false;
    resetParamsToDefaults();
    int applied = 0;
    for (int i = 0; i < vt.getNumChildren(); ++i)
    {
        auto child = vt.getChild(i);
        if (! child.hasType("PARAM")) continue;
        if (auto* par = apvts.getParameter(child.getProperty("id").toString()))
        {
            par->setValueNotifyingHost(par->convertTo0to1((float) child.getProperty("value", 0.0)));
            ++applied;
        }
    }
    return applied > 0;
}

juce::String MonoProcessor::getPresetName(int i) const { return presetName(i); }

void MonoProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = presetState();
    state.setProperty("prog", currentPreset.load(), nullptr);
    if (auto xml = state.createXml()) copyXmlToBinary(*xml, destData);
}

void MonoProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        auto vt = juce::ValueTree::fromXml(*xml);
        if (vt.isValid() && vt.hasType(apvts.state.getType()))
        {
            apvts.replaceState(vt);
            int prog = vt.getProperty("prog", 0);
            juce::String nm = vt.getProperty("presetName", juce::String()).toString();
            if (nm.isEmpty()) nm = prog >= 0 ? presetName(prog) : juce::String("Init Patch");
            setPresetIdentity(prog, nm);
        }
    }
}

juce::AudioProcessorEditor* MonoProcessor::createEditor() { return new MonoEditor(*this); }

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new MonoProcessor(); }
