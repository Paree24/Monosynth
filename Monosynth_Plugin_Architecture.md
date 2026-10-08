# Monosynth — MS-20-Inspired Monophonic VA Synth Plugin

## 1. Product goal

**Monosynth** is a monophonic virtual-analog synthesizer inspired by the signal architecture and character of the Korg MS-20, but presented as a modern plugin.

The design deliberately separates:

- **Panel 1 — Synth:** the basic, immediately playable VA instrument.
- **Panel 2 — Mod Matrix:** the modular/routing layer, replacing the original hardware patch bay with a compact, explicit modulation matrix.

The goal is not to reproduce the physical MS-20 UI. The goal is to reproduce the useful synthesis architecture and aggressive, characterful filtering while making the instrument substantially easier to understand and automate in a DAW.

> **Important:** This specification is an MS-20-inspired design. Do not copy Korg artwork, logos, proprietary UI assets, or undocumented proprietary source code. Prefer independently implemented DSP models and clearly documented open-source components.

---

# 2. Recommended plugin technology

Use a modern C++ audio-plugin framework.

### Recommended stack

- **C++17 or C++20**
- **JUCE** for plugin framework/UI
- VST3
- AU on macOS
- CLAP as an optional additional format
- CMake
- Git
- Catch2 / doctest for DSP tests

Suggested project layout:

```text
Monosynth/
├── CMakeLists.txt
├── src/
│   ├── PluginProcessor.*
│   ├── PluginEditor.*
│   │
│   ├── dsp/
│   │   ├── MonosynthVoice.*
│   │   ├── Oscillator.*
│   │   ├── Mixer.*
│   │   ├── MS20Filter.*
│   │   ├── Envelope.*
│   │   ├── LFO.*
│   │   ├── Noise.*
│   │   ├── Portamento.*
│   │   ├── ModMatrix.*
│   │   └── Saturation.*
│   │
│   ├── ui/
│   │   ├── Panel1Synth.*
│   │   ├── Panel2ModMatrix.*
│   │   ├── Knob.*
│   │   ├── Slider.*
│   │   ├── ModAmount.*
│   │   └── Theme.*
│   │
│   └── parameters/
│       └── ParameterIDs.h
│
├── tests/
│   ├── OscillatorTests.*
│   ├── FilterTests.*
│   ├── EnvelopeTests.*
│   └── ModMatrixTests.*
│
└── README.md
```

---

# 3. High-level architecture

```text
                         ┌──────────────────────┐
MIDI NOTE ──────────────►│ Pitch / Voice Engine │
Velocity ───────────────►│ Monophonic Voice     │
Pitch Bend ─────────────►│ Glide / Portamento   │
                         └──────────┬───────────┘
                                    │
                                    ▼
                    ┌───────────────────────────┐
                    │       OSCILLATOR 1        │
                    │ Saw / Triangle / Pulse    │
                    └─────────────┬─────────────┘
                                  │
                                  │
                    ┌─────────────▼─────────────┐
                    │       OSCILLATOR 2        │
                    │ Saw / Square / Pulse       │
                    └─────────────┬─────────────┘
                                  │
                                  ▼
                         ┌────────────────┐
                         │ OSC MIX / DRIVE│
                         └───────┬────────┘
                                 │
                  ┌──────────────▼──────────────┐
                  │         MS-20 FILTER        │
                  │                              │
                  │ HPF → LPF                   │
                  │ Resonance / nonlinear drive │
                  └──────────────┬──────────────┘
                                 │
                                 ▼
                         ┌────────────────┐
                         │      VCA       │
                         │ Amp Envelope   │
                         └───────┬────────┘
                                 │
                                 ▼
                              OUTPUT
```

The modulation layer sits alongside the audio path:

```text
                 ┌──────────────────────────┐
                 │       MOD SOURCES        │
                 │                          │
                 │ LFO                      │
                 │ EG1                      │
                 │ EG2 / Amp EG             │
                 │ Velocity                  │
                 │ Aftertouch                │
                 │ Mod Wheel                │
                 │ Key Tracking              │
                 │ Oscillator 1              │
                 │ Oscillator 2              │
                 │ Random / S&H              │
                 │ MIDI CC                   │
                 └────────────┬─────────────┘
                              │
                              ▼
                    ┌───────────────────┐
                    │    MOD MATRIX     │
                    │                   │
                    │ Source → Amount   │
                    │ Source → Target   │
                    │ Bipolar/unipolar  │
                    │ Curve / smoothing │
                    └─────────┬─────────┘
                              │
                              ▼
                ┌────────────────────────────┐
                │         TARGETS            │
                │                            │
                │ VCO1 pitch                │
                │ VCO2 pitch                │
                │ VCO1 shape                │
                │ VCO2 shape                │
                │ HPF cutoff                │
                │ LPF cutoff                │
                │ HPF resonance             │
                │ LPF resonance             │
                │ VCA level                 │
                │ LFO rate                  │
                │ OSC mix                   │
                │ Noise level               │
                │ Drive                     │
                └────────────────────────────┘
```

---

# 4. Panel 1 — SYNTH

Panel 1 should contain **only the basic VA synthesis controls**.

Avoid exposing the entire modular system here.

The user should be able to open Monosynth and immediately understand:

> Oscillators → Filter → Amp

## Panel 1 layout

```text
┌──────────────────────────────────────────────────────────────┐
│ MONOSYNTH                                      [P1] [P2]      │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  OSC 1                 OSC 2                 MIX             │
│  ┌─────────────┐       ┌─────────────┐       ┌─────────────┐ │
│  │ WAVE        │       │ WAVE        │       │ OSC 1       │ │
│  │ SAW         │       │ SQUARE      │       │     ●────── │ │
│  │             │       │             │       │ OSC 2       │ │
│  │ OCT   TUNE  │       │ OCT   TUNE  │       │     ●────── │ │
│  └─────────────┘       └─────────────┘       │ NOISE       │ │
│                                              └─────────────┘ │
│                                                              │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  FILTER                                                      │
│  ┌──────────────────────┐       ┌─────────────────────────┐   │
│  │ HPF                  │       │ LPF                     │   │
│  │                      │       │                         │   │
│  │ CUTOFF       RES     │       │ CUTOFF          RES     │   │
│  │   ●           ●      │       │   ●              ●      │   │
│  └──────────────────────┘       └─────────────────────────┘   │
│                                                              │
│  DRIVE                         FILTER ROUTING                 │
│     ●                           HPF → LPF                      │
│                                                              │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│  AMP ENV                    GLOBAL                            │
│  A    D    S    R           GLIDE       MASTER              │
│  ●    ●    ●    ●            ●            ●                 │
│                                                              │
└──────────────────────────────────────────────────────────────┘
```

## Panel 1 controls

### OSC 1

- Waveform:
  - Saw
  - Triangle
  - Pulse
  - Square
- Octave:
  - 16'
  - 8'
  - 4'
- Fine tune
- Pulse width when applicable

### OSC 2

- Waveform:
  - Saw
  - Square
  - Pulse
  - Triangle
- Octave
- Fine tune
- Pulse width

### Mixer

- OSC 1 level
- OSC 2 level
- Noise level

Noise types:

- White
- Pink (optional)

### Filter

Use the characteristic MS-20 architecture:

```text
INPUT
  │
  ▼
 HPF
  │
  ▼
 LPF
  │
  ▼
 OUTPUT
```

Controls:

- HPF cutoff
- HPF resonance
- LPF cutoff
- LPF resonance
- Filter drive

Optional filter routing modes:

```text
HPF → LPF
LPF only
HPF only
```

The default should be:

```text
HPF → LPF
```

### Amp envelope

Basic ADSR:

- Attack
- Decay
- Sustain
- Release

### Global

- Glide
- Master volume

---

# 5. Panel 2 — MOD MATRIX

Panel 2 replaces the original MS-20 patch bay.

Do not reproduce the physical patch-panel layout.

Instead, make modulation explicit:

```text
SOURCE → AMOUNT → TARGET
```

## Mod Matrix UI

```text
┌──────────────────────────────────────────────────────────────┐
│ MOD MATRIX                                     [P1] [P2]     │
├──────────────────────────────────────────────────────────────┤
│                                                              │
│ SOURCE          AMOUNT          TARGET           CURVE        │
│ ──────────────────────────────────────────────────────────── │
│ EG1             [-100% ─●─ +100%] LPF CUTOFF      LINEAR     │
│ LFO             [-100% ─●─ +100%] VCO1 PITCH     LINEAR     │
│ VELOCITY        [-100% ─●─ +100%] VCA LEVEL      EXPO       │
│ KEY TRACK       [-100% ─●─ +100%] LPF CUTOFF      LINEAR     │
│ MOD WHEEL       [-100% ─●─ +100%] LFO DEPTH      LINEAR     │
│                                                              │
│ [+ ADD MODULATION]                                            │
│                                                              │
├──────────────────────────────────────────────────────────────┤
│ SOURCES                                                      │
│                                                              │
│ LFO       EG1       EG2       VELOCITY       KEY TRACK       │
│ MOD WHEEL  AFTERTOUCH  RANDOM  OSC1  OSC2  MIDI CC           │
│                                                              │
├──────────────────────────────────────────────────────────────┤
│ TARGETS                                                      │
│ VCO1 PITCH  VCO2 PITCH  OSC1 PW  OSC2 PW                     │
│ OSC MIX     HPF CUTOFF  LPF CUTOFF  HPF RES  LPF RES         │
│ VCA LEVEL   NOISE LEVEL  DRIVE  LFO RATE                     │
└──────────────────────────────────────────────────────────────┘
```

## Matrix behavior

Each modulation route should contain:

```text
source
amount
target
curve
```

Optional advanced fields:

```text
polarity
smoothing
minimum
maximum
```

For the first implementation, keep it to:

```text
SOURCE
AMOUNT
TARGET
```

This keeps the interface fast.

---

# 6. Modulation sources

Implement these first:

| Source | Range |
|---|---|
| LFO | -1 to +1 |
| EG1 | 0 to +1 |
| EG2 | 0 to +1 |
| Velocity | 0 to +1 |
| Key Track | -1 to +1 |
| Mod Wheel | 0 to +1 |
| Aftertouch | 0 to +1 |
| Random | -1 to +1 |
| OSC1 | -1 to +1 |
| OSC2 | -1 to +1 |

Optional later:

- MIDI CC
- Pitch bend
- Gate
- Trigger
- External audio envelope
- Sample & Hold

---

# 7. Envelopes

Use two envelopes.

## EG1 — Filter / modulation envelope

MS-20-inspired envelope:

```text
Delay → Attack → Hold → Decay → Release
```

For the initial version, it is acceptable to simplify this to:

```text
Attack → Decay → Sustain → Release
```

Then add Delay/Hold as an extension.

Recommended EG1 destinations:

- LPF cutoff
- HPF cutoff
- oscillator pitch
- oscillator pulse width
- VCA

## EG2 — Amp envelope

Standard ADSR:

```text
Attack
Decay
Sustain
Release
```

Primary destination:

```text
VCA
```

---

# 8. LFO / Modulation Generator

One LFO.

Waveforms:

- Triangle
- Sine
- Saw
- Square
- Sample & Hold

Parameters:

- Rate
- Waveform
- Sync
- Phase
- Amount

Tempo sync:

```text
1/1
1/2
1/4
1/8
1/16
1/32
```

Free-running and retrigger modes should both be supported.

---

# 9. Oscillator implementation

The oscillators are virtual analog, not simple wavetable playback.

Implement:

- band-limited saw
- band-limited square
- band-limited pulse
- triangle

Preferred techniques:

- PolyBLEP
- BLAMP where appropriate
- minBLEP
- oversampling

For a monophonic synth, CPU requirements are modest, so prioritize quality over extreme optimization.

## Oscillator state

Each oscillator should maintain:

```cpp
phase
frequency
waveform
pulseWidth
level
octave
fineTune
phaseReset
```

Use continuous phase accumulation.

Avoid naive:

```cpp
output = phase < 0.5 ? 1.0 : -1.0;
```

because this produces severe aliasing.

---

# 10. MS-20-style filter

The filter is one of the defining parts of Monosynth.

The original MS-20 appeared with different filter implementations across revisions, so design the DSP layer so that the filter model can be replaced without changing the UI or modulation architecture.

Recommended interface:

```cpp
class MS20Filter
{
public:
    void prepare(double sampleRate);

    void reset();

    void setCutoff(float hz);
    void setResonance(float amount);
    void setDrive(float amount);

    float processHP(float input);
    float processLP(float input);

    void process(float* buffer, int numSamples);
};
```

The filter should support:

```text
HPF
LPF
HPF → LPF
```

and nonlinear resonance behavior.

## Open-source references

### FAUST Korg 35 implementation

The FAUST libraries contain an implementation of the **Korg 35 LPF**, the filter associated with the MS-10 and early MS-20.

Repository:

https://github.com/grame-cncm/faustlibraries

The implementation is in:

```text
vaeffects.lib
```

The library explicitly identifies `korg35LPF` and provides a permissive MIT-style STK-4.3 license for that implementation.

Use this as a reference/component only after reviewing the exact license and project compatibility.

### FAUST Filters

Another useful project is:

https://github.com/SpotlightKid/faustfilters

It provides virtual-analog plugins including:

- Korg 35 HPF
- Korg 35 LPF
- other VA filter models

This is particularly useful if you want a ready-made reference implementation and test target.

### L71 MS20VCF

https://github.com/L71/MS20VCF

This is a hardware Korg MS-20-style VCF project containing:

- schematic
- PCB files
- documentation

It is useful for understanding the analog circuit topology rather than directly copying DSP code.

### aphex-move

https://github.com/filliformes/aphex-move

This project is especially useful for studying a modern clean-room MS-20-inspired implementation. Its source describes both:

- KORG-35 / early MS-20 style filtering
- later OTA / LM13600 style filtering

It also references Tim Stinchcombe's MS-20 filter research and uses TPT-based digital filter techniques.

---

# 11. Filter model recommendation

Implement the filter in stages.

## Version 1

Start with a stable digital approximation:

```text
input
  │
  ▼
nonlinear input drive
  │
  ▼
2-pole MS-20-inspired filter
  │
  ├── HP output
  │
  └── LP output
```

Then add:

- nonlinear integrators
- resonance saturation
- feedback clipping
- oversampling

## Version 2

Support two selectable models internally:

```text
K35
OTA
```

UI can initially hide this choice.

Later expose:

```text
FILTER MODEL
[K35] [OTA]
```

This allows the plugin to cover both broad families of MS-20 filter behavior without redesigning the synth.

---

# 12. Oversampling

The nonlinear filter and oscillator stages can produce significant high-frequency content.

Recommended processing:

```text
DAW sample rate
      │
      ▼
  2x / 4x oversample
      │
      ▼
oscillators
      │
      ▼
drive
      │
      ▼
MS20 filter
      │
      ▼
VCA
      │
      ▼
anti-alias filter
      │
      ▼
downsample
      │
      ▼
DAW
```

Start with:

- 2x oversampling

Add:

- 4x oversampling

as a quality option.

---

# 13. Nonlinear behavior

A major part of the MS-20 character comes from nonlinear behavior.

Do not model the filter as a completely clean textbook 2-pole filter.

Potential nonlinear elements:

```text
input saturation
stage saturation
resonance feedback saturation
output saturation
```

A simple starting point:

```cpp
x = tanh(drive * x);
```

But the final filter should preferably use nonlinearities inside the filter structure rather than only placing a waveshaper before/after it.

---

# 14. Mod matrix DSP architecture

The modulation system should be independent of the UI.

Example:

```cpp
struct ModRoute
{
    ModSource source;
    ModTarget target;
    float amount;
};
```

At audio rate:

```cpp
for each route:
{
    sourceValue = getSourceValue(route.source);

    modulation =
        sourceValue *
        route.amount;

    targetAccumulator[route.target] += modulation;
}
```

Then parameter values are calculated:

```cpp
cutoff =
    baseCutoff
    + mod[LPF_CUTOFF];
```

For exponential parameters such as pitch and cutoff, prefer multiplicative/exponential modulation.

Example:

```text
cutoff = baseCutoff * 2^(modulation)
```

This gives musical octave-based modulation.

For level parameters:

```text
level = baseLevel + modulation
```

Clamp where appropriate.

---

# 15. Parameter architecture

Use normalized parameters internally:

```text
0.0 → 1.0
```

Convert to meaningful DSP values at the DSP boundary.

Example:

```text
LPF cutoff:
20 Hz → 20 kHz

LPF resonance:
0 → 1

Oscillator fine:
-100 cents → +100 cents

Drive:
0 → 1
```

Do not store raw GUI pixel positions or arbitrary values in DSP code.

---

# 16. Parameter smoothing

All user-controlled continuous parameters should be smoothed where necessary.

Especially:

- oscillator pitch
- oscillator level
- cutoff
- resonance
- drive
- VCA
- modulation amounts

Use sample/block-rate smoothing rather than instantly jumping parameters.

Recommended starting time:

```text
5–20 ms
```

However:

- envelopes should remain responsive
- MIDI note events should not feel sluggish
- audio-rate modulation should bypass slow UI smoothing

Separate:

```text
GUI smoothing
```

from:

```text
audio-rate modulation
```

---

# 17. MIDI behavior

Monophonic operation:

```text
MIDI note on
    ↓
current note
    ↓
voice pitch
```

Recommended note priority:

```text
Last note priority
```

Later options:

- low note
- high note
- first note

Implement:

- MIDI note
- velocity
- pitch bend
- mod wheel
- aftertouch
- sustain pedal

---

# 18. Glide

Portamento should be implemented as a smoothed pitch target.

Parameters:

```text
Glide Time
Glide Mode
```

Modes:

```text
Always
Legato
```

Recommended default:

```text
Legato
```

---

# 19. UI visual language

The requested visual direction is:

> **grayscale + pastel**

Avoid a traditional black-and-neon modular-synth aesthetic.

## Base palette

Use mostly grayscale:

```text
Background      #151515
Panel           #202020
Panel raised    #292929
Control         #343434
Border          #4A4A4A
Text primary    #E8E8E8
Text secondary  #A8A8A8
```

Use pastel colors only as functional accents.

Suggested accents:

```text
Pastel Blue     #9DB7D5
Pastel Mint     #A8D5C2
Pastel Yellow   #E5D79A
Pastel Pink     #D8A9B8
Pastel Lavender #B8ADD8
```

Do not make every control colorful.

The pastel colors should communicate function.

Example:

```text
OSC     pastel blue
FILTER  pastel lavender
ENV     pastel yellow
LFO     pastel mint
MOD     pastel pink
```

---

# 20. UI interaction rules

The interface should feel like an instrument, not a spreadsheet.

Knobs:

- drag vertically
- double click → default
- shift → fine adjustment
- command/ctrl → fine adjustment if appropriate

Mod matrix:

- dropdown for source
- amount slider/knob
- dropdown for target
- delete button

Add route:

```text
+ ADD MODULATION
```

Maximum initial routes:

```text
16
```

Increase later if needed.

---

# 21. Mod matrix usability

Avoid making users scroll through a huge list.

Use:

```text
5–8 visible routes
```

with scrolling.

Example:

```text
┌───────────────────────────────────────────┐
│ SOURCE       AMOUNT       TARGET          │
├───────────────────────────────────────────┤
│ LFO          ───●────     LPF CUTOFF      │
│ EG1          ─────●──     HPF CUTOFF      │
│ KEY TRACK    ──●─────     LPF CUTOFF      │
│ VELOCITY     ─────●──     VCA LEVEL       │
│ MOD WHEEL    ──●─────     VCO1 PITCH      │
├───────────────────────────────────────────┤
│ + ADD MODULATION                           │
└───────────────────────────────────────────┘
```

---

# 22. Default modulation routes

Monosynth should have useful defaults without requiring Panel 2.

Internally initialize:

```text
EG1  → LPF CUTOFF      +0.50
EG2  → VCA LEVEL       +1.00
KEY  → LPF CUTOFF      +0.25
LFO  → VCO1 PITCH      +0.00
```

The actual visible modulation matrix can show these.

The synth should sound useful immediately after loading a preset.

---

# 23. Suggested default patch

```text
OSC1
Wave: Saw
Octave: 8'
Level: 0.70

OSC2
Wave: Square
Octave: 8'
Fine: +5 cents
Level: 0.30

Noise
Level: 0.00

HPF
Cutoff: 30 Hz
Resonance: 0.05

LPF
Cutoff: 2.5 kHz
Resonance: 0.20

Drive
0.10

EG1
A: 5 ms
D: 350 ms
S: 0.20
R: 250 ms

EG2
A: 2 ms
D: 150 ms
S: 0.85
R: 150 ms

Glide
0 ms
```

---

# 24. DSP execution order

Recommended per-block architecture:

```text
MIDI
 │
 ▼
Voice state
 │
 ├── pitch
 ├── velocity
 └── gate
 │
 ▼
Modulation sources
 │
 ├── LFO
 ├── EG1
 ├── EG2
 ├── key tracking
 ├── velocity
 └── other sources
 │
 ▼
Mod matrix
 │
 ▼
Oscillator parameter calculation
 │
 ▼
OSC 1 ──────┐
            ├── Mixer ── Drive ── HPF ── LPF ── VCA
OSC 2 ──────┤
Noise ──────┘
                         │
                         ▼
                       Output
```

---

# 25. Important DSP design rule

The modulation matrix should **not** directly modify GUI parameters.

Bad:

```text
modMatrix → GUI slider → DSP
```

Good:

```text
baseParameter
      +
modulationAccumulator
      ↓
effectiveParameter
      ↓
DSP
```

This makes automation, presets, modulation and host synchronization much easier.

---

# 26. Automation

Every primary Panel 1 control should be a host-automatable parameter.

Panel 2 modulation amounts should also be automatable.

Recommended parameter groups:

```text
Oscillator
Filter
Envelope
LFO
Mixer
Global
Mod Matrix
```

Use stable parameter IDs.

Do not generate parameter IDs dynamically from display names.

---

# 27. Preset format

Store:

```text
preset name
version
all synth parameters
all modulation routes
filter model
oversampling setting
```

Example:

```json
{
  "name": "Aggressive Bass",
  "version": 1,
  "osc1": {},
  "osc2": {},
  "filter": {},
  "envelopes": {},
  "lfo": {},
  "modMatrix": [
    {
      "source": "EG1",
      "amount": 0.62,
      "target": "LPF_CUTOFF"
    }
  ]
}
```

Use JUCE's `AudioProcessorValueTreeState` for parameter/state management.

---

# 28. Testing requirements

## Oscillators

Test:

- frequency accuracy
- phase continuity
- waveform amplitude
- aliasing behavior
- pulse width

## Filter

Test:

- cutoff response
- resonance
- stability
- self-oscillation behavior
- HP response
- LP response
- HP → LP cascade
- parameter changes while audio is running

## Mod matrix

Test:

```text
one source → one target
multiple sources → one target
zero amount
positive amount
negative amount
parameter limits
```

## MIDI

Test:

- note on
- note off
- legato
- retrigger
- pitch bend
- velocity
- sustain

---

# 29. Performance requirements

The plugin is monophonic, so prioritize sound quality.

Targets:

- real-time safe audio thread
- no memory allocation in `processBlock`
- no locks in DSP
- no GUI calls from DSP
- deterministic preset loading
- stable behavior at 44.1, 48, 88.2 and 96 kHz

Use oversampling only around nonlinear sections if CPU profiling shows it is necessary.

---

# 30. Development phases

## Phase 1 — Skeleton

Implement:

- JUCE plugin
- CMake
- VST3
- AU
- parameter system
- basic UI
- MIDI

Deliverable:

> Silent but fully functional plugin shell.

## Phase 2 — Basic VA

Implement:

- OSC1
- OSC2
- noise
- mixer
- VCA
- ADSR

Deliverable:

> Basic playable monophonic synthesizer.

## Phase 3 — Filter

Implement:

- HPF
- LPF
- resonance
- nonlinear behavior
- HPF → LPF routing

Deliverable:

> MS-20-inspired playable synth.

## Phase 4 — Modulation

Implement:

- LFO
- EG1
- EG2
- key tracking
- velocity
- mod wheel
- modulation matrix

Deliverable:

> Full Panel 2 modular routing system.

## Phase 5 — Character

Add:

- saturation
- filter oversampling
- oscillator drift
- subtle analog instability
- improved resonance behavior

Do not overdo drift.

The synth should remain playable and predictable.

## Phase 6 — Presets

Create:

- basses
- leads
- acid sounds
- percussion
- drones
- FX
- sequences

At least 30 factory presets.

---

# 31. What NOT to implement initially

Keep the first version focused.

Do not initially implement:

- external audio input
- ESP pitch detector
- ring modulation
- sequencer
- polyphony
- effects rack
- reverb
- delay
- chorus
- complex patch cables
- multi-page modulation editors

These can be added later.

---

# 32. Optional MS-20 features for Version 2

After the core synth is stable, consider:

### Ring modulation

```text
OSC1 × OSC2
```

### External signal processor

```text
Audio Input
    │
    ├── Envelope follower
    ├── Pitch detector
    └── Gate detector
```

### Sample & Hold

```text
Noise
  ↓
S&H
  ↓
Mod Matrix
```

### Filter self-oscillation

Allow resonance to reach self-oscillation.

This is particularly important for expressive filter sweeps and percussion sounds.

---

# 33. Open-source/reference links

Use these as research/reference starting points:

- **FAUST libraries — Korg 35 LPF implementation**
  https://github.com/grame-cncm/faustlibraries

- **FAUST Filters — Korg 35 HPF/LPF plugins**
  https://github.com/SpotlightKid/faustfilters

- **L71 MS20VCF — MS-20-style analog filter schematic/project**
  https://github.com/L71/MS20VCF

- **aphex-move — clean-room MS-20-inspired K35/OTA DSP implementation**
  https://github.com/filliformes/aphex-move

- **Tim Stinchcombe MS-20 filter research/reference material**
  Use the research cited by the projects above when deriving the nonlinear digital model.

Before incorporating external code, verify its current license and preserve required copyright/license notices.

---

# 34. Recommended architecture summary

The final Monosynth architecture should be:

```text
                       MONOSYNTH
                           │
          ┌────────────────┴────────────────┐
          │                                 │
       PANEL 1                           PANEL 2
       SYNTH                           MOD MATRIX
          │                                 │
          │                         ┌───────┴────────┐
          │                         │ Sources        │
          │                         │ Amounts        │
          │                         │ Targets        │
          │                         └───────┬────────┘
          │                                 │
          └────────────────┬────────────────┘
                           │
                           ▼
                    EFFECTIVE PARAMETERS
                           │
                           ▼
                ┌─────────────────────┐
                │ OSCILLATOR ENGINE   │
                │                     │
                │ OSC1   OSC2   Noise │
                └──────────┬──────────┘
                           │
                           ▼
                    ┌─────────────┐
                    │     MIX     │
                    └──────┬──────┘
                           │
                           ▼
                    ┌─────────────┐
                    │     HPF     │
                    └──────┬──────┘
                           │
                           ▼
                    ┌─────────────┐
                    │     LPF     │
                    └──────┬──────┘
                           │
                           ▼
                    ┌─────────────┐
                    │     VCA     │
                    └──────┬──────┘
                           │
                           ▼
                         AUDIO
```

The central design principle is:

> **Panel 1 makes the synth playable. Panel 2 makes the synth modular.**

That separation should be maintained throughout the implementation.
