#pragma once
// Monosynth DSP: oscillators, cascade HPF->LPF filter, envelopes, MG, mono voice.
// Clean-room implementation. Filter topology notes:
// Two cascaded 2-pole TPT state-variable sections (HP then LP) with soft
// saturation on integrator states and in the resonance feedback path, plus
// input drive and 2x internal oversampling of the nonlinear core. The design
// goal (aggressive resonant cascade, stable at all settings) follows the
// well-known public circuit descriptions of the late-70s Japanese
// HPF-into-LPF monosynth and published TPT filter techniques; no vendor code
// or artwork is used here.
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <cmath>
#include <array>

namespace MonoDSP
{

inline float midiToFreq(float m) { return 440.0f * std::pow(2.0f, (m - 69.0f) / 12.0f); }
inline double rangeMult(int r) // octave switch -2..+2
{
    switch (r) { case 0: return 0.25; case 1: return 0.5; case 2: return 1.0; case 3: return 2.0; default: return 4.0; }
}

inline float polyblep(double phase, double dt)
{
    if (phase < dt) { phase /= dt; return (float)(phase + phase - phase * phase - 1.0); }
    if (phase > 1.0 - dt) { phase = (phase - 1.0) / dt; return (float)(phase * phase + phase + phase + 1.0); }
    return 0.0f;
}

// wave: 0 Saw 1 Triangle 2 Square 3 Pulse 4 Sine
inline float oscSample(int wave, double phase, float pw, double dt)
{
    switch (wave)
    {
        case 0: { float v = (float)(2.0 * phase - 1.0); v -= polyblep(phase, dt); return v * 0.9f; }
        case 1: return (float)(4.0 * std::abs(phase - 0.5) - 1.0) * 0.9f;
        case 2: {
            float v = phase < 0.5 ? 1.0f : -1.0f;
            v += polyblep(phase, dt);
            double ph2 = phase - 0.5; if (ph2 < 0) ph2 += 1.0;
            v -= polyblep(ph2, dt);
            return v * 0.62f;
        }
        case 3: {
            float v = phase < (double)pw ? 1.0f : -1.0f;
            v += polyblep(phase, dt);
            double ph2 = phase - (double)pw; if (ph2 < 0) ph2 += 1.0;
            v -= polyblep(ph2, dt);
            return v * 0.62f;
        }
        default: return (float)std::sin(2.0 * juce::MathConstants<double>::pi * phase) * 0.9f;
    }
}

// ---------------- ADSR ----------------
class EnvADSR
{
public:
    void setSampleRate(double sr) { sampleRate = sr; }
    void setParams(float a, float d, float s, float r)
    {
        att = std::max(a, 0.001f); dec = std::max(d, 0.002f);
        sus = juce::jlimit(0.0f, 1.0f, s); rel = std::max(r, 0.005f);
    }
    void noteOn() { state = State::Attack; }
    void noteOff() { if (state != State::Idle) state = State::Release; }
    void reset() { state = State::Idle; level = 0.0f; }
    bool isActive() const { return state != State::Idle; }
    inline float next()
    {
        switch (state)
        {
            case State::Idle: return 0.0f;
            case State::Attack: {
                float ka = 1.0f - std::exp(-3.0f / (att * (float)sampleRate));
                level += (1.0f - level) * ka;
                if ((1.0f - level) < 0.005f) state = State::Decay;
                return level;
            }
            case State::Decay: {
                float kd = 1.0f - std::exp(-3.0f / (dec * (float)sampleRate));
                level += (sus - level) * kd;
                if (std::fabs(level - sus) < 0.002f) { level = sus; state = State::Sustain; }
                return level;
            }
            case State::Sustain: return sus;
            case State::Release: {
                float kr = std::exp(-3.0f / (rel * (float)sampleRate));
                level *= kr;
                if (level < 0.0006f) { level = 0.0f; state = State::Idle; return 0.0f; }
                return level;
            }
        }
        return 0.0f;
    }
private:
    enum class State { Idle, Attack, Decay, Sustain, Release };
    State state = State::Idle;
    double sampleRate = 44100.0;
    float att = 0.01f, dec = 0.2f, sus = 0.7f, rel = 0.3f, level = 0.0f;
};

// ---------------- Modulation generator (LFO) ----------------
// wave: 0 Tri 1 Saw 2 Square 3 Sine 4 S&H
class ModGen
{
public:
    void setSampleRate(double sr) { sampleRate = sr; }
    void reset() { phase = 0.0; snh = 0.0f; }
    void noteOn() {}
    float next(float rateHz, int wave, juce::Random& rng)
    {
        phase += (double)rateHz / sampleRate;
        if (phase >= 1.0) { phase -= 1.0; snh = rng.nextFloat() * 2.0f - 1.0f; }
        switch (wave)
        {
            case 0: return (float)(4.0 * std::abs(phase - 0.5) - 1.0);
            case 1: return (float)(2.0 * phase - 1.0);
            case 2: return phase < 0.5 ? 1.0f : -1.0f;
            case 3: return (float)std::sin(2.0 * juce::MathConstants<double>::pi * phase);
            default: return snh;
        }
    }
private:
    double sampleRate = 44100.0, phase = 0.0;
    float snh = 0.0f;
};

// ---------------- Cascade filter: HPF -> LPF ----------------
// Circuit-logic model derived from the late-70s Japanese HPF-into-LPF
// monosynth filter family (studied via the L71 eurorack clone schematic and
// the published electro-music service schematic: dual-OTA gm-C integrators,
// resistive resonance feedback through a log-taper pot, low headroom OTA
// stages, resonant highpass in series with the lowpass).
//
// Captured behaviors (own clean-room formulation, no vendor code):
//  - two gm-C integrator stages per section, solved with a Zavalishin TPT
//    linear pass plus a relaxation pass where the resonance feedback goes
//    through the OTA tanh transfer (linear response exact at small signals,
//    compressive scream at large signals);
//  - log-taper resonance pot (matches the A100k hardware taper);
//  - resonance steals bass (strong on Early, mild on Late) via a
//    Q-dependent subtraction of the lowpassed output;
//  - low input headroom (stages overdrive easily, like the hardware);
//  - loop sustains into self-oscillation whistle at maximum resonance,
//    amplitude-bounded by the OTA saturation.
struct OTASection
{
    void reset() { ic1 = ic2 = 0.0f; }
    inline void step(float x, float g, float k, float satG,
                     float& outLP, float& outHP)
    {
        float a1 = 1.0f / (1.0f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;
        // pass 1: linear TPT solve
        float v3 = x - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        // pass 2: OTA-saturated resonance feedback (one relaxation pass).
        // The stage output clips first (OTA voltage swing), then the
        // resistor network scales it: fb = k * sat(v1). tanh(u)/u -> 1 for
        // small u, so small-signal response is unchanged; large resonant
        // swings compress the feedback and the loop sings instead of
        // clipping. Integrator states are deliberately NOT clamped (that
        // would strangle the resonance); a far-off numerical safety net
        // below only catches pathological blowups.
        float fbLin = k * v1;
        float fb = k * std::tanh(v1 * satG) / satG;
        float v3b = x - ic2 - (fb - fbLin);
        v1 = a1 * ic1 + a2 * v3b;
        float v2 = ic2 + a2 * ic1 + a3 * v3b;
        ic1 = juce::jlimit(-16.0f, 16.0f, 2.0f * v1 - ic1);
        ic2 = juce::jlimit(-16.0f, 16.0f, 2.0f * v2 - ic2);
        outLP = v2;
        outHP = x - k * v1 - v2;
    }
    float ic1 = 0.0f, ic2 = 0.0f;
};

static inline float resQ(float r, int model) // log-taper pot -> stage Q
{
    float t = std::pow(juce::jlimit(0.0f, 1.0f, r), 2.2f);
    float qmax = model == 0 ? 14.0f : 9.0f;
    return 0.5f + t * qmax;
}

class CascadeFilter
{
public:
    void prepare(double sr) { sampleRate = sr; reset(); }
    void reset() { hpSec.reset(); lpSec.reset(); blp = 0.0f; }
    void setModel(int m) { model = juce::jlimit(0, 1, m); }

    // Full cascade: returns filtered sample. All cutoffs in Hz, peaks 0..1.
    inline float process(float x, float hpfCut, float hpfPeak, float lpfCut,
                         float lpfRes, float drive)
    {
        float fcH = juce::jlimit(10.0f, 18000.0f, hpfCut);
        float fcL = juce::jlimit(20.0f, 19000.0f, lpfCut);
        // low headroom input stage: overdrives easily, like the hardware.
        // Transparent when drive is at 0.
        if (drive > 0.001f)
        {
            float dg = 1.0f + drive * 9.0f;
            x = std::tanh(x * dg) / (1.0f + drive * 1.5f);
        }

        float qH = resQ(hpfPeak, model), qL = resQ(lpfRes, model);
        // self-oscillation zone: damping drops toward zero and circuit noise
        // keeps the loop singing (like the hardware whistle with no input)
        bool susH = hpfPeak >= 0.985f, susL = lpfRes >= 0.985f;
        float kH = 1.0f / qH, kL = 1.0f / qL;
        if (susH) kH *= 0.05f;
        if (susL) kL *= 0.05f;
        // 2x internal: step at double rate, hold input, average sub-outputs
        float sr2 = (float)sampleRate * 2.0f;
        float gH = juce::jmin(std::tan(juce::MathConstants<float>::pi * fcH / sr2), 7.0f);
        float gL = juce::jmin(std::tan(juce::MathConstants<float>::pi * fcL / sr2), 7.0f);
        // OTA transfer hardness per model: Early clips harder, Late softer.
        // OTA transfer hardness per model: Early clips harder, Late softer.
        // Kept gentle enough that low-resonance settings stay linear (scope-
        // clean); the huge resonant swings still compress deeply and scream.
        float satG = model == 0 ? 1.2f : 0.9f;

        float lp1, hp1, lp2, hp2, dummy;
        hpSec.step(x,  gH, kH, satG, dummy, hp1);
        hpSec.step(x,  gH, kH, satG, dummy, hp2);
        float h = (hp1 + hp2) * 0.5f;
        if (susL || susH) // circuit-noise excitation in the whistle zone
        {
            whState = whState * 1664525u + 1013904223u;
            h += ((float)(whState >> 16) / 32768.0f - 1.0f) * 0.02f;
        }
        lpSec.step(h,  gL, kL, satG, lp1, dummy);
        lpSec.step(h,  gL, kL, satG, lp2, dummy);
        float y = (lp1 + lp2) * 0.5f;
        // resonance steals bass (strong on Early, mild on Late)
        float bassK = (model == 0 ? 0.8f : 0.35f) * lpfRes * lpfRes;
        if (bassK > 0.001f)
        {
            float gB = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * 300.0f / sr2);
            float yl = (lp1 + lp2) * 0.5f;
            blp += gB * (yl - blp);
            y -= bassK * blp;
        }
        y = 3.0f * std::tanh(y / 3.0f); // high rails: clean when calm, clips when screaming
        if (! std::isfinite(y)) { reset(); return 0.0f; }
        return y;
    }

    // Single-section access for tests (uses host-rate single step).
    float processHP(float x, float cutoff, float peak)
    {
        float g = std::tan(juce::MathConstants<float>::pi *
                           juce::jlimit(10.0f, 18000.0f, cutoff) / (float)sampleRate);
        g = juce::jmin(g, 7.0f);
        float q = resQ(peak, model);
        float k = 1.0f / q;
        if (peak >= 0.985f) k *= 0.05f;
        float outLP, outHP;
        hpSec.step(x, g, k, 1.2f, outLP, outHP);
        return outHP;
    }
    float processLP(float x, float cutoff, float res)
    {
        float g = std::tan(juce::MathConstants<float>::pi *
                           juce::jlimit(20.0f, 19000.0f, cutoff) / (float)sampleRate);
        g = juce::jmin(g, 7.0f);
        float q = resQ(res, model);
        float k = 1.0f / q;
        if (res >= 0.985f) k *= 0.05f;
        float outLP, outHP;
        lpSec.step(x, g, k, 1.2f, outLP, outHP);
        return outLP;
    }

private:
    double sampleRate = 44100.0;
    int model = 0;
    OTASection hpSec, lpSec;
    float blp = 0.0f; // bass-loss tracker state
    juce::uint32 whState = 22222u; // whistle-zone noise source
};

// ---------------- Saturation: multi-mode output distortion ----------------
// type: 0 Tape (soft tanh) 1 Tube (asymmetric + even harmonics)
//       2 Fold (sine wavefold) 3 Fuzz (hard clip)
// amt 0 = transparent bypass.
class Saturation
{
public:
    inline float process(float x, float amt, int type)
    {
        if (amt <= 0.001f) return x;
        amt = juce::jlimit(0.0f, 1.0f, amt);
        float y = x;
        switch (juce::jlimit(0, 3, type))
        {
            case 0: { // Tape
                float g = 1.0f + amt * 6.0f;
                y = std::tanh(x * g) / (1.0f + amt * 0.9f);
                break;
            }
            case 1: { // Tube
                float g = 1.0f + amt * 8.0f;
                float xp = x >= 0.0f ? x * g : x * g * 0.65f;
                float t = std::tanh(xp);
                y = (t + 0.14f * t * t * (x >= 0.0f ? 1.0f : -1.0f)) / (1.0f + amt * 1.1f);
                break;
            }
            case 2: { // Fold
                float g = (1.0f + amt * 5.0f) * 1.1f;
                y = std::sin(x * g) * (0.85f - amt * 0.15f);
                break;
            }
            default: { // Fuzz
                float g = 1.0f + amt * 12.0f;
                y = juce::jlimit(-1.0f, 1.0f, x * g);
                y = std::tanh(y * 2.0f) * 0.85f;
                break;
            }
        }
        if (! std::isfinite(y)) return 0.0f;
        return y;
    }
};

// ---------------- Mod matrix ----------------
struct ModRoute { int src = 0; float amt = 0.0f; int dst = 0; int type = 0; };
// src: 0 None 1 MG 2 EG1 3 EG2 4 Vel 5 AT 6 KeyTrk 7 ModWh 8 PitchWh 9 Rand 10 Gate
// dst: 0 None 1 VCO1P 2 VCO2P 3 VCO1PW 4 VCO2PW 5 HPFCut 6 LPFCut 7 HPFPeak 8 LPFRes 9 VCA

struct SynthParams
{
    int vco1wave = 0, vco1oct = 2, vco2wave = 2, vco2oct = 2;
    float vco1pitch = 0.0f, vco1tune = 0.0f, vco1pw = 0.5f;
    float vco2pitch = 0.0f, vco2tune = 5.0f, vco2pw = 0.5f;
    float mix1 = 0.8f, mix2 = 0.0f, ring = 0.0f, noise = 0.0f;
    int noiseType = 0;
    float thick = 0.3f; // osc-bus soft saturation (thickness without extra oscillators)
    float hpfCut = 30.0f, hpfPeak = 0.1f, lpfCut = 2500.0f, lpfRes = 0.2f, drive = 0.1f;
    int fmodel = 0;
    float satAmt = 0.0f; int satType = 0;
    float vca = 0.8f, volume = 0.8f, porta = 0.0f;
    bool limiter = true;
    bool portaOn = false; int glideMode = 0;
    int mgWave = 0; float mgRate = 4.0f; bool mgSync = false;
    float eg1a = 0.005f, eg1d = 0.35f, eg1s = 0.2f, eg1r = 0.25f;
    float eg2a = 0.002f, eg2d = 0.15f, eg2s = 0.85f, eg2r = 0.15f;
    float bendRange = 2.0f, velSens = 0.5f;
    bool chOn = false; float chRate = 0.6f, chDepth = 0.4f;
    bool dlOn = false; float dlTime = 350.0f, dlFb = 0.35f, dlMix = 0.25f;
    bool rvOn = false; float rvSize = 0.5f, rvMix = 0.25f;
    std::array<ModRoute, 8> routes {};
    float pitchBend = 0.0f; // semitones, smoothed
};

// Pink noise (Paul Kellet economised filter)
struct PinkNoise
{
    void reset() { b0 = b1 = b2 = b3 = b4 = b5 = b6 = 0.0f; }
    inline float next(juce::Random& rng)
    {
        float w = rng.nextFloat() * 2.0f - 1.0f;
        b0 = 0.99886f * b0 + w * 0.0555179f;
        b1 = 0.99332f * b1 + w * 0.0750759f;
        b2 = 0.96900f * b2 + w * 0.1538520f;
        b3 = 0.86650f * b3 + w * 0.3104856f;
        b4 = 0.55000f * b4 + w * 0.5329522f;
        b5 = -0.7616f * b5 - w * 0.0168980f;
        float p = b0 + b1 + b2 + b3 + b4 + b5 + b6 + w * 0.5362f;
        b6 = w * 0.115926f;
        return p * 0.11f;
    }
    float b0 = 0, b1 = 0, b2 = 0, b3 = 0, b4 = 0, b5 = 0, b6 = 0;
};

// ---------------- Monophonic voice ----------------
class MonoVoice
{
public:
    bool active = false;
    int note = -1;
    float velocity = 0.8f;

    void prepare(double sr)
    {
        sampleRate = sr;
        eg1.setSampleRate(sr); eg2.setSampleRate(sr);
        filt.prepare(sr);
        rng.setSeedRandomly();
        pink.reset();
    }
    void setModel(int m) { filt.setModel(m); }

    void noteOn(int midiNote, float vel, const SynthParams& p, bool glide, bool retrigger)
    {
        bool wasActive = active;
        note = midiNote; velocity = vel; active = true; gated = true;
        float target = (float)midiNote + p.pitchBend;
        if (! glide) { curMidi = target; targetMidi = target; }
        else targetMidi = target;
        if (! wasActive)
        {
            ph1 = rng.nextFloat(); ph2 = rng.nextFloat();
            filt.reset();
            randomHold = rng.nextFloat() * 2.0f - 1.0f;
            slopCut = 1.0f + (rng.nextFloat() * 2.0f - 1.0f) * 0.02f;
            clickCount = 0;
        }
        eg1.setParams(p.eg1a, p.eg1d, p.eg1s, p.eg1r);
        eg2.setParams(p.eg2a, p.eg2d, p.eg2s, p.eg2r);
        if (retrigger) { eg1.noteOn(); eg2.noteOn(); }
    }
    void noteOff() { gated = false; eg1.noteOff(); eg2.noteOff(); }
    void kill() { active = false; eg1.reset(); eg2.reset(); }
    void setTarget(float m) { targetMidi = m; }
    float lastOut() const { return lastSample; }

    float render(const SynthParams& p, float mgVal, float aftertouch,
                 float modWheel, float pitchWheel, float& outEg1, float& outEg2)
    {
        // portamento glide toward target
        if (p.portaOn && p.porta > 0.001f)
        {
            float t = 0.002f + p.porta * p.porta * 0.6f;
            float c = 1.0f - (float)std::exp(-1.0 / (sampleRate * t));
            curMidi += (targetMidi - curMidi) * c;
        }
        else curMidi = targetMidi;

        float e1 = eg1.next();
        float e2 = eg2.next();
        outEg1 = e1; outEg2 = e2;
        if (! eg1.isActive() && ! eg2.isActive() && ! gated)
        { active = false; lastSample = 0.0f; return 0.0f; }

        // ---- modulation sources ----
        float keyTrk = juce::jlimit(-1.0f, 1.0f, ((float)note - 60.0f) / 36.0f);
        float srcVals[11] = {};
        srcVals[1] = mgVal; srcVals[2] = e1; srcVals[3] = e2;
        srcVals[4] = velocity; srcVals[5] = aftertouch; srcVals[6] = keyTrk;
        srcVals[7] = modWheel; srcVals[8] = pitchWheel; srcVals[9] = randomHold;
        srcVals[10] = gated ? 1.0f : 0.0f;

        float mPitch1 = 0.0f, mPitch2 = 0.0f;   // semitones
        float mPw1 = 0.0f, mPw2 = 0.0f;
        float mHpfOct = 0.0f, mLpfOct = 0.0f;    // octaves
        float mHpfPk = 0.0f, mLpfRs = 0.0f, mVca = 0.0f;
        for (auto& r : p.routes)
        {
            if (r.src <= 0 || r.dst <= 0 || r.amt == 0.0f) continue;
            float v = srcVals[juce::jlimit(0, 10, r.src)];
            if (r.type == 1) // unipolar: fold bipolar sources to 0..1
            {
                if (r.src == 1 || r.src == 6 || r.src == 8 || r.src == 9) v = (v + 1.0f) * 0.5f;
                v = juce::jmax(0.0f, v);
            }
            float m = v * r.amt;
            switch (r.dst)
            {
                case 1: mPitch1 += m * 12.0f; break;
                case 2: mPitch2 += m * 12.0f; break;
                case 3: mPw1 += m * 0.45f; break;
                case 4: mPw2 += m * 0.45f; break;
                case 5: mHpfOct += m * 5.0f; break;
                case 6: mLpfOct += m * 5.0f; break;
                case 7: mHpfPk += m * 0.8f; break;
                case 8: mLpfRs += m * 0.8f; break;
                case 9: mVca += m; break;
                default: break;
            }
        }

        float pw1 = juce::jlimit(0.03f, 0.97f, p.vco1pw + mPw1);
        float pw2 = juce::jlimit(0.03f, 0.97f, p.vco2pw + mPw2);

        float semi1 = p.vco1pitch + p.vco1tune / 100.0f + mPitch1;
        float semi2 = p.vco2pitch + p.vco2tune / 100.0f + mPitch2;
        double f1 = (double)midiToFreq(curMidi + semi1) * rangeMult(p.vco1oct);
        double f2 = (double)midiToFreq(curMidi + semi2) * rangeMult(p.vco2oct);
        f1 = juce::jlimit(1.0, 18000.0, f1); f2 = juce::jlimit(1.0, 18000.0, f2);
        double dt1 = f1 / sampleRate, dt2 = f2 / sampleRate;
        ph1 += dt1; if (ph1 >= 1.0) ph1 -= 1.0;
        ph2 += dt2; if (ph2 >= 1.0) ph2 -= 1.0;
        float s1 = oscSample(p.vco1wave, ph1, pw1, dt1);
        float s2 = oscSample(p.vco2wave, ph2, pw2, dt2);
        float ringS = (s1 * s2) * 1.4f;
        float nz = p.noiseType == 0 ? (rng.nextFloat() * 2.0f - 1.0f) * 0.6f : pink.next(rng);

        float mixed = s1 * p.mix1 + s2 * p.mix2 + ringS * p.ring + nz * p.noise;
        // osc-bus soft saturation ("thickness"): rounds the summed waves into
        // each other and blooms harmonics. Transparent at 0.
        if (p.thick > 0.001f)
        {
            float t = juce::jlimit(0.0f, 1.0f, p.thick);
            float y = std::tanh(mixed * (1.0f + 4.0f * t));
            y = y / (1.0f + 0.9f * t) + 0.06f * t * y * y;
            mixed = y;
        }
        mixed *= 0.65f;

        float hCut = juce::jlimit(10.0f, 18000.0f,
            p.hpfCut * std::pow(2.0f, mHpfOct) * slopCut);
        float lCut = juce::jlimit(20.0f, 19000.0f,
            p.lpfCut * std::pow(2.0f, mLpfOct) * slopCut);
        float hPk = juce::jlimit(0.0f, 1.0f, p.hpfPeak + mHpfPk);
        float lRs = juce::jlimit(0.0f, 1.0f, p.lpfRes + mLpfRs);

        float y = filt.process(mixed, hCut, hPk, lCut, lRs, p.drive);

        float ampVel = (1.0f - 0.7f * p.velSens) + 0.7f * p.velSens * velocity;
        float vcaM = juce::jlimit(0.0f, 1.5f, p.vca + mVca);
        float clickG = 1.0f;
        if (clickCount < 96)
        {
            clickG = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::pi * (float)clickCount / 96.0f);
            ++clickCount;
        }
        lastSample = sat.process(y * e2 * ampVel * vcaM, p.satAmt, p.satType) * clickG;
        return lastSample;
    }

    double sampleRate = 44100.0;

private:
    bool gated = true;
    EnvADSR eg1, eg2;
    CascadeFilter filt;
    Saturation sat;
    PinkNoise pink;
    juce::Random rng;
    double ph1 = 0.0, ph2 = 0.0;
    float curMidi = 60.0f, targetMidi = 60.0f;
    float randomHold = 0.0f, slopCut = 1.0f;
    int clickCount = 96;
    float lastSample = 0.0f;
};

} // namespace MonoDSP
