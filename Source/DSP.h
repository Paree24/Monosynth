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
    // invalidate every cached time so the next setParams() rebuilds all
    // coefficients for the new rate (not just the attack)
    void setSampleRate(double sr) { sampleRate = sr; att = dec = rel = -1.0f; }
    // Cheap to call every block: coefficients are only recomputed on change,
    // so knob moves reach held notes (sustain/release apply immediately).
    void setParams(float a, float d, float s, float r)
    {
        a = std::max(a, 0.001f); d = std::max(d, 0.002f);
        s = juce::jlimit(0.0f, 1.0f, s); r = std::max(r, 0.005f);
        const float sr = (float)sampleRate;
        // attack charges toward kAttackAim (above the 1.0 peak) and stops at
        // 1.0, like the analog EG's capacitor aimed at a higher rail: a near-
        // linear rise that reaches the peak in exactly the attack time
        if (a != att)
        {
            att = a;
            const float tau = att / std::log(kAttackAim / (kAttackAim - 1.0f));
            ka = 1.0f - std::exp(-1.0f / (tau * sr));
        }
        if (d != dec) { dec = d; kd = 1.0f - std::exp(-3.0f / (dec * sr)); }
        if (r != rel) { rel = r; kr = std::exp(-3.0f / (rel * sr)); }
        sus = s;
        if (state == State::Sustain) level = sus;
    }
    // envelope extras (both 0 = a plain ADSR): EG1 DELAY holds the attack off
    // after the gate opens; EG2 HOLD keeps the gate open for at least that
    // long, so a short tap still runs the whole envelope.
    void setDelayHold(float delaySec, float holdSec)
    {
        delaySamps = (int) (std::max(0.0f, delaySec) * sampleRate);
        holdSamps = (int) (std::max(0.0f, holdSec) * sampleRate);
    }
    void noteOn()
    {
        holdLeft = holdSamps;
        releasePending = false;
        if (delaySamps > 0) { delayLeft = delaySamps; state = State::Delay; }
        else state = State::Attack;
    }
    void noteOff()
    {
        if (state == State::Idle) return;
        if (holdLeft > 0) { releasePending = true; return; } // hold: release when it runs out
        state = State::Release;
    }
    void reset() { state = State::Idle; level = 0.0f; holdLeft = 0; releasePending = false; }
    bool isActive() const { return state != State::Idle; }
    inline float next()
    {
        if (holdLeft > 0 && --holdLeft == 0 && releasePending && state != State::Idle)
        {
            releasePending = false;
            state = State::Release;
        }
        switch (state)
        {
            case State::Idle: return 0.0f;
            case State::Delay:
                if (--delayLeft <= 0) state = State::Attack;
                return level; // the output waits where it is (0 from rest)
            case State::Attack:
                level += (kAttackAim - level) * ka;
                if (level >= 1.0f) { level = 1.0f; state = State::Decay; }
                return level;
            case State::Decay:
                level += (sus - level) * kd;
                if (std::fabs(level - sus) < 0.002f) { level = sus; state = State::Sustain; }
                return level;
            case State::Sustain: return sus;
            case State::Release:
                level *= kr;
                if (level < 0.0006f) { level = 0.0f; state = State::Idle; return 0.0f; }
                return level;
        }
        return 0.0f;
    }
private:
    enum class State { Idle, Delay, Attack, Decay, Sustain, Release };
    static constexpr float kAttackAim = 1.3f;
    int delaySamps = 0, holdSamps = 0, delayLeft = 0, holdLeft = 0;
    bool releasePending = false;
    State state = State::Idle;
    double sampleRate = 44100.0;
    float att = -1.0f, dec = -1.0f, sus = 0.7f, rel = -1.0f, level = 0.0f;
    float ka = 0.0f, kd = 0.0f, kr = 0.0f;
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
//    linear pass plus a relaxation pass where the resonance is a positive
//    feedback of the bandpass node through the OTA tanh transfer, around a
//    fixed Q = 0.5 damping (k = 2 - r). Linear response is exact at small
//    signals; as level rises the feedback compresses, the effective damping
//    grows and the resonance tames itself instead of running away;
//  - log-taper resonance pot (matches the A100k hardware taper);
//  - resonance steals bass (strong on Early, mild on Late) via a
//    Q-dependent subtraction of the lowpassed output, with the shelf corner
//    kept below the cutoff so low resonant whistles survive;
//  - low input headroom (stages overdrive easily, like the hardware);
//  - past the self-oscillation point the linear loop gain exceeds unity, a
//    whisper of circuit noise inside each section starts it, and the OTA
//    saturation sets the whistle amplitude (both HPF PEAK and LPF RES sing).
struct OTASection
{
    void reset() { ic1 = ic2 = 0.0f; }
    // k: linear damping (2 = no resonance, < 0 = self-oscillating),
    // nz: circuit noise injected into the bandpass integrator
    inline void step(float x, float g, float k, float satG, float nz,
                     float& outLP, float& outHP)
    {
        float a1 = 1.0f / (1.0f + g * (g + k));
        float a2 = g * a1;
        float a3 = g * a2;
        // pass 1: linear TPT solve
        float v3 = x - ic2;
        float v1 = a1 * ic1 + a2 * v3;
        // pass 2: OTA-saturated resonance feedback (one relaxation pass).
        // The resonance path r*v1 (r = 2 - k) clips first (OTA voltage
        // swing): the input sees r*(v1 - sat(v1)) of extra damping, which is
        // zero for small v1 and grows with level, so loud resonant swings
        // compress. Integrator states are deliberately NOT clamped (that
        // would strangle the resonance); a far-off numerical safety net
        // below only catches pathological blowups.
        float r = 2.0f - k;
        float xe = x - r * (v1 - std::tanh(v1 * satG) / satG);
        float v3b = xe - ic2;
        v1 = a1 * ic1 + a2 * v3b;
        float v2 = ic2 + a2 * ic1 + a3 * v3b;
        ic1 = juce::jlimit(-16.0f, 16.0f, 2.0f * v1 - ic1 + nz);
        ic2 = juce::jlimit(-16.0f, 16.0f, 2.0f * v2 - ic2);
        outLP = v2;
        outHP = xe - k * v1 - v2;
    }
    float ic1 = 0.0f, ic2 = 0.0f;
};

static inline float resQ(float r, int model) // log-taper pot -> stage Q
{
    float t = std::pow(juce::jlimit(0.0f, 1.0f, r), 2.2f);
    float qmax = model == 0 ? 14.0f : 9.0f;
    return 0.5f + t * qmax;
}

// Self-oscillation zone: damping fades through zero over the last stretch of
// the pot instead of stepping at a threshold (a hard step made the resonance
// jump audibly as a smoothed knob or the mod matrix crossed it).
static inline float whistleAmt(float r)
{
    float t = juce::jlimit(0.0f, 1.0f, (r - 0.95f) / 0.035f);
    return t * t * (3.0f - 2.0f * t);
}
static inline float resK(float r, int model)
{
    float w = whistleAmt(r);
    return (1.0f / resQ(r, model)) * (1.0f - 0.95f * w) - 0.15f * w;
}


class CascadeFilter
{
public:
    void prepare(double sr)
    {
        sampleRate = sr; reset();
    }
    void reset() { hpSec.reset(); lpSec.reset(); blp = 0.0f; }
    void setModel(int m) { model = juce::jlimit(0, 1, m); }

    // Once per host sample: cutoffs in Hz, peaks 0..1. The core then runs at
    // twice the host rate (tick() is called twice per host sample).
    inline void setControls(float hpfCut, float hpfPeak, float lpfCut, float lpfRes)
    {
        float fcH = juce::jlimit(10.0f, 18000.0f, hpfCut);
        float fcL = juce::jlimit(20.0f, 19000.0f, lpfCut);
        kH = resK(hpfPeak, model); kL = resK(lpfRes, model);
        wH = whistleAmt(hpfPeak); wL = whistleAmt(lpfRes);
        float sr2 = (float)sampleRate * 2.0f;
        gH = juce::jmin(std::tan(juce::MathConstants<float>::pi * fcH / sr2), 7.0f);
        gL = juce::jmin(std::tan(juce::MathConstants<float>::pi * fcL / sr2), 7.0f);
        // OTA transfer hardness per model: Early clips harder, Late softer
        // (louder, cleaner whistle). Low-resonance settings stay linear.
        satG = model == 0 ? 0.6f : 0.45f;
        // resonance steals bass (strong on Early, mild on Late); the shelf
        // corner follows the cutoff (300 Hz, or 0.3x cutoff when lower) so it
        // never lands on top of a low resonant peak
        bassK = (model == 0 ? 0.8f : 0.35f) * lpfRes * lpfRes;
        float wB = 2.0f * juce::MathConstants<float>::pi * juce::jmin(300.0f, 0.3f * fcL) / sr2;
        gBass = wB / (1.0f + wB);
    }

    // One oversampled-rate step of the cascade (input drive -> HPF -> LPF).
    inline float tick(float x, float drive)
    {
        // low headroom input stage: overdrives easily, like the hardware.
        // Transparent when drive is at 0.
        if (drive > 0.001f)
        {
            float dg = 1.0f + drive * 9.0f;
            x = std::tanh(x * dg) / (1.0f + drive * 1.5f);
        }
        float nzH = wH > 0.0f ? circuitNoise() * wH : 0.0f;
        float nzL = wL > 0.0f ? circuitNoise() * wL : 0.0f;
        float lp, h, dummy;
        hpSec.step(x, gH, kH, satG, nzH, dummy, h);
        lpSec.step(h, gL, kL, satG, nzL, lp, dummy);
        float y = lp;
        // (tracker always runs so it never restarts from a stale value when
        // resonance is swept up from zero)
        blp += gBass * (y - blp);
        if (bassK > 0.001f) y -= bassK * blp;
        y = 3.0f * std::tanh(y / 3.0f); // high rails: clean when calm, clips when screaming
        if (! std::isfinite(y)) { reset(); return 0.0f; }
        return y;
    }

    // Host-rate convenience: holds the input for both oversampled steps and
    // averages them (tests and simple callers; the voice feeds tick() from
    // its own 2x upsampler).
    inline float process(float x, float hpfCut, float hpfPeak, float lpfCut,
                         float lpfRes, float drive)
    {
        setControls(hpfCut, hpfPeak, lpfCut, lpfRes);
        float a = tick(x, drive);
        float b = tick(x, drive);
        return (a + b) * 0.5f;
    }

    // Single-section access for tests (uses host-rate single step).
    float processHP(float x, float cutoff, float peak)
    {
        float g = std::tan(juce::MathConstants<float>::pi *
                           juce::jlimit(10.0f, 18000.0f, cutoff) / (float)sampleRate);
        g = juce::jmin(g, 7.0f);
        float k = resK(peak, model);
        float outLP, outHP;
        hpSec.step(x, g, k, 0.6f, 0.0f, outLP, outHP);
        return outHP;
    }
    float processLP(float x, float cutoff, float res)
    {
        float g = std::tan(juce::MathConstants<float>::pi *
                           juce::jlimit(20.0f, 19000.0f, cutoff) / (float)sampleRate);
        g = juce::jmin(g, 7.0f);
        float k = resK(res, model);
        float outLP, outHP;
        lpSec.step(x, g, k, 0.6f, 0.0f, outLP, outHP);
        return outLP;
    }

private:
    // tiny white circuit noise (-60 dB): only fed in near self-oscillation,
    // enough to start the whistle from silence, far below audibility
    inline float circuitNoise()
    {
        whState = whState * 1664525u + 1013904223u;
        return ((float)(whState >> 16) / 32768.0f - 1.0f) * 1.0e-3f;
    }
    double sampleRate = 44100.0;
    int model = 0;
    OTASection hpSec, lpSec;
    float gH = 0.01f, gL = 0.1f, kH = 2.0f, kL = 2.0f, wH = 0.0f, wL = 0.0f, satG = 0.6f;
    float blp = 0.0f, gBass = 0.02f, bassK = 0.0f; // bass-loss tracker state + coefficients
    juce::uint32 whState = 22222u; // circuit-noise source
};

// ---------------- 2x oversampling (halfband FIR pair) ----------------
// Up: zero-stuff + lowpass at the 2x rate; down: lowpass + keep every second
// sample. 63-tap Blackman-windowed sinc, cutoff at the host Nyquist: flat to
// ~0.4 fs, about -75 dB images/aliases, 31 host samples of delay overall.
struct HalfbandOS
{
    static constexpr int N = 63;
    void prepare()
    {
        const int M = (N - 1) / 2;
        double sum = 0.0;
        for (int i = 0; i < N; ++i)
        {
            const double n = i - M;
            const double sinc = n == 0 ? 0.5 : std::sin(juce::MathConstants<double>::pi * 0.5 * n) / (juce::MathConstants<double>::pi * n);
            const double w = 0.42 - 0.5 * std::cos(2.0 * juce::MathConstants<double>::pi * i / (N - 1))
                           + 0.08 * std::cos(4.0 * juce::MathConstants<double>::pi * i / (N - 1));
            h[i] = (float) (sinc * w);
            sum += h[i];
        }
        for (auto& c : h) c = (float) (c / sum);
        reset();
    }
    void reset() { std::fill(std::begin(up), std::end(up), 0.0f); std::fill(std::begin(dn), std::end(dn), 0.0f); upPos = dnPos = 0; }
    // one host sample in, two 2x-rate samples out
    inline void upsample(float x, float& y0, float& y1)
    {
        push(up, upPos, 2.0f * x); y0 = conv(up, upPos);
        push(up, upPos, 0.0f);     y1 = conv(up, upPos);
    }
    // two 2x-rate samples in, one host sample out
    inline float downsample(float x0, float x1)
    {
        push(dn, dnPos, x0);
        push(dn, dnPos, x1);
        return conv(dn, dnPos);
    }
private:
    static inline void push(float* buf, int& pos, float x) { pos = (pos + 1) % N; buf[pos] = x; }
    inline float conv(const float* buf, int pos) const
    {
        float acc = 0.0f;
        for (int i = 0; i < N; ++i) acc += h[i] * buf[(pos - i + N) % N];
        return acc;
    }
    float h[N] {}, up[N] {}, dn[N] {};
    int upPos = 0, dnPos = 0;
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
    bool dlSync = false; int dlDiv = 1;
    float eg1Delay = 0.0f, eg2Hold = 0.0f; // EG1 DELAY / EG2 HOLD (s)
    float vcaInit = 0.0f;                   // VCA initial gain: drones at this level
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
        glideKnob = -1.0f; // glide coefficient is rate-dependent: rebuild on next render
        filt.prepare(sr);
        os.prepare();
        rng.setSeedRandomly();
        pink.reset();
    }
    void setModel(int m) { filt.setModel(m); }

    void noteOn(int midiNote, float vel, const SynthParams& p, bool glide, bool retrigger)
    {
        bool wasActive = active;
        note = midiNote; velocity = vel; active = true; gated = true;
        float target = (float)midiNote; // bend is added after the glide slew
        if (! glide) { curMidi = target; targetMidi = target; }
        else targetMidi = target;
        if (! wasActive)
        {
            ph1 = rng.nextFloat(); ph2 = rng.nextFloat();
            filt.reset();
            os.reset();
            randomHold = rng.nextFloat() * 2.0f - 1.0f;
            slopCut = 1.0f + (rng.nextFloat() * 2.0f - 1.0f) * 0.02f;
            clickCount = 0;
        }
        eg1.setParams(p.eg1a, p.eg1d, p.eg1s, p.eg1r);
        eg2.setParams(p.eg2a, p.eg2d, p.eg2s, p.eg2r);
        eg1.setDelayHold(p.eg1Delay, 0.0f);
        eg2.setDelayHold(0.0f, p.eg2Hold);
        if (retrigger) { eg1.noteOn(); eg2.noteOn(); }
    }
    void noteOff() { gated = false; eg1.noteOff(); eg2.noteOff(); }
    // once per block: envelope knobs act on the note that is already sounding
    void updateEnvelopes(const SynthParams& p)
    {
        eg1.setParams(p.eg1a, p.eg1d, p.eg1s, p.eg1r);
        eg2.setParams(p.eg2a, p.eg2d, p.eg2s, p.eg2r);
        eg1.setDelayHold(p.eg1Delay, 0.0f);
        eg2.setDelayHold(0.0f, p.eg2Hold);
    }
    void kill() { active = false; eg1.reset(); eg2.reset(); }
    void setTarget(float m) { targetMidi = m; }
    float lastOut() const { return lastSample; }

    float render(const SynthParams& p, float mgVal, float aftertouch,
                 float modWheel, float pitchWheel, float& outEg1, float& outEg2)
    {
        // portamento glide toward target
        if (p.portaOn && p.porta > 0.001f)
        {
            if (p.porta != glideKnob)
            {
                glideKnob = p.porta;
                float t = 0.002f + p.porta * p.porta * 0.6f;
                glideCoef = 1.0f - (float)std::exp(-1.0 / (sampleRate * t));
            }
            curMidi += (targetMidi - curMidi) * glideCoef;
        }
        else curMidi = targetMidi;

        float e1 = eg1.next();
        float e2 = eg2.next();
        outEg1 = e1; outEg2 = e2;
        // VCA initial gain drones on with no envelope at all; only a
        // closed VCA lets the voice go idle
        if (! eg1.isActive() && ! eg2.isActive() && ! gated && p.vcaInit <= 0.001f)
        { active = false; lastSample = 0.0f; return 0.0f; }

        // ---- modulation sources ----
        // pitch bend rides on top of the glide: the wheel never gets slewed by
        // portamento (the bend is a direct pitch CV, not a key CV)
        const float pitchMidi = curMidi + p.pitchBend;
        // follows the gliding pitch, so key-tracked cutoff slides with portamento
        float keyTrk = juce::jlimit(-1.0f, 1.0f, (pitchMidi - 60.0f) / 36.0f);
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
        double f1 = (double)midiToFreq(pitchMidi + semi1) * rangeMult(p.vco1oct);
        double f2 = (double)midiToFreq(pitchMidi + semi2) * rangeMult(p.vco2oct);
        f1 = juce::jlimit(1.0, 18000.0, f1); f2 = juce::jlimit(1.0, 18000.0, f2);
        double dt1 = f1 / sampleRate, dt2 = f2 / sampleRate;
        ph1 += dt1; if (ph1 >= 1.0) ph1 -= 1.0;
        ph2 += dt2; if (ph2 >= 1.0) ph2 -= 1.0;
        float s1 = oscSample(p.vco1wave, ph1, pw1, dt1);
        float s2 = oscSample(p.vco2wave, ph2, pw2, dt2);
        float ringS = (s1 * s2) * 1.4f;
        float nz = p.noiseType == 0 ? (rng.nextFloat() * 2.0f - 1.0f) * 0.6f : pink.next(rng);

        float mixed = s1 * p.mix1 + s2 * p.mix2 + ringS * p.ring + nz * p.noise;

        float hCut = juce::jlimit(10.0f, 18000.0f,
            p.hpfCut * std::pow(2.0f, mHpfOct) * slopCut);
        float lCut = juce::jlimit(20.0f, 19000.0f,
            p.lpfCut * std::pow(2.0f, mLpfOct) * slopCut);
        float hPk = juce::jlimit(0.0f, 1.0f, p.hpfPeak + mHpfPk);
        float lRs = juce::jlimit(0.0f, 1.0f, p.lpfRes + mLpfRs);

        float ampVel = (1.0f - 0.7f * p.velSens) + 0.7f * p.velSens * velocity;
        float vcaM = juce::jlimit(0.0f, 1.5f, p.vca + mVca);
        // VCA control = EG2 + initial gain, capped at fully open
        const float vcaCv = juce::jmin(1.0f, e2 + p.vcaInit);
        const float gain = vcaCv * ampVel * vcaM;
        float clickG = 1.0f;
        if (clickCount < 96)
        {
            clickG = 0.5f - 0.5f * std::cos(juce::MathConstants<float>::pi * (float)clickCount / 96.0f);
            ++clickCount;
        }

        // nonlinear core at 2x: thickness -> drive + filter cascade -> VCA ->
        // saturation, between a halfband upsampler and decimator
        filt.setControls(hCut, hPk, lCut, lRs);
        float u[2];
        os.upsample(mixed, u[0], u[1]);
        for (auto& m : u)
        {
            // osc-bus soft saturation ("thickness"): rounds the summed waves
            // into each other and blooms harmonics. Transparent at 0.
            if (p.thick > 0.001f)
            {
                float t = juce::jlimit(0.0f, 1.0f, p.thick);
                float y = std::tanh(m * (1.0f + 4.0f * t));
                m = y / (1.0f + 0.9f * t) + 0.06f * t * y * y;
            }
            m = sat.process(filt.tick(m * 0.65f, p.drive) * gain, p.satAmt, p.satType);
        }
        lastSample = os.downsample(u[0], u[1]) * clickG;
        return lastSample;
    }

    double sampleRate = 44100.0;

private:
    bool gated = true;
    EnvADSR eg1, eg2;
    CascadeFilter filt;
    HalfbandOS os;
    Saturation sat;
    PinkNoise pink;
    juce::Random rng;
    double ph1 = 0.0, ph2 = 0.0;
    float curMidi = 60.0f, targetMidi = 60.0f;
    float randomHold = 0.0f, slopCut = 1.0f;
    float glideKnob = -1.0f, glideCoef = 1.0f;
    int clickCount = 96;
    float lastSample = 0.0f;
};

} // namespace MonoDSP
