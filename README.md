# Monosynth — Monophonic Retro Analog Synthesizer (VST3 + Standalone)

> **Disclaimer:** this project is vibe-coded for personal use. It is provided
> as-is, without warranty of any kind. The author is not responsible for
> anything — Use at your own risk.

Monosynth is a monophonic virtual-analog synthesizer: dual PolyBLEP VCOs
(saw/triangle/square/pulse/sine) with ring modulation, white/pink noise and an
osc-bus Thickness saturator, feeding a cascaded resonant **HPF → LPF** filter
with drive (Early/Late models), dual ADSR envelopes, a tempo-syncable
modulation generator (triangle/saw/square/sine/S&H), portamento (legato/always),
an 8-slot modulation matrix, multi-mode saturation (Tape/Tube/Fold/Fuzz), a
master FX section (chorus, stereo delay, modulated algorithmic reverb), a
switchable master limiter, 356 factory presets as editable data files (never
compiled in), user preset save/load with factory shadowing, and a full preset
browser with search.

## License

GPL version 3 — see [LICENSE](LICENSE). This also satisfies the JUCE
framework's licensing terms for this project.

Embedded typefaces (Lato Regular + Bold + Black) are SIL Open Font License 1.1;
see [Assets/OFL-NOTICE.txt](Assets/OFL-NOTICE.txt).

## Building

Requirements: CMake 3.22+, a C++17 compiler, Ninja (or Make), Git (JUCE is
fetched automatically), and JUCE's Linux dependencies (ALSA, X11, freetype,
etc.) — see the JUCE docs for the full list.

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Outputs:

- VST3: `build/Monosynth_artefacts/Release/VST3/Monosynth.vst3` (+ factory presets under
  `Contents/Resources/Factory/`)
- Standalone: `build/Monosynth_artefacts/Release/Standalone/Monosynth`
- Headless self-test: `build/MonoHarness_artefacts/Release/MonoHarness`
  (runs the full DSP/UI regression suite; exit 0 = all good)
- Preset generator: `build/PresetDump_artefacts/Release/PresetDump Factory`
  (regenerates the `Factory/*.xml` data files from the recipes)

Install the VST3 by copying `Monosynth.vst3` to your plugin folder:

| OS      | VST3 folder                          | Notes                                        |
|---------|--------------------------------------|----------------------------------------------|
| Linux   | `~/.vst3/`                           | Rescan plugins in your DAW afterwards        |
| macOS   | `~/Library/Audio/Plug-Ins/VST3/`     | Unsigned build: right-click → Open once      |
| Windows | `C:\Program Files\Common Files\VST3\`| Rescan plugins in your DAW afterwards        |

User presets live in `Documents/Monosynth Presets/` (created on first save);
factory overwrites shadow shipped files from `Documents/Monosynth Presets/Factory/`
without ever touching the install bundle.

### OS-specific notes

> Only the Linux build has actually been compiled and tested here. The macOS
> and Windows steps below follow the standard JUCE/CMake flow and *should*
> work, but if you hit an OS-specific snag, please file an issue with the
> failing command and its full output.

#### Linux (verified)

1. Install dependencies (Debian/Ubuntu shown; Fedora/Arch: equivalent
   `-devel` packages):
   ```sh
   sudo apt install build-essential cmake ninja-build git pkg-config \
     libasound2-dev libx11-dev libxcomposite-dev libxcursor-dev \
     libxinerama-dev libxrandr-dev libfreetype6-dev libfontconfig1-dev \
     libcurl4-openssl-dev libwebkit2gtk-4.1-dev
   ```
2. Configure + build (JUCE is fetched automatically — needs network access):
   ```sh
   cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
   cmake --build build --config Release
   ```
3. Run the self-test (must print `ALL TESTS PASSED`):
   ```sh
   ./build/MonoHarness_artefacts/Release/MonoHarness
   ```
4. Install: copy `build/Monosynth_artefacts/Release/VST3/Monosynth.vst3` to `~/.vst3/`
   (factory presets travel inside the bundle), then rescan plugins in your DAW.

#### macOS (not yet built here)

1. Install Xcode command-line tools, CMake, Ninja and Git:
   ```sh
   xcode-select --install
   brew install cmake ninja git
   ```
2. Same configure + build as Linux. For a universal (Intel + Apple Silicon)
   binary, add `-DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"` to the configure step.
3. Run `./build/MonoHarness_artefacts/Release/MonoHarness` — expect `ALL TESTS PASSED`.
4. Install: copy `Monosynth.vst3` to `~/Library/Audio/Plug-Ins/VST3/`. These are
   unsigned personal builds: if macOS refuses to load them, ad-hoc sign with
   `codesign --force --deep -s - <path>` and/or strip the quarantine flag
   with `xattr -dr com.apple.quarantine <path>`. Codesigning/notarisation for
   distribution is out of scope for this project.

#### Windows (not yet built here)

1. Install Visual Studio 2022 with the **Desktop development with C++**
   workload (MSVC v143 or newer), plus CMake, Ninja and Git
   (`winget install Kitware.CMake Ninja-build.Ninja Git.Git` works).
2. Open an **x64 Native Tools Command Prompt** (so MSVC is on `PATH`), then
   the same configure + build commands as Linux. Alternatively use the
   Visual Studio generator instead of Ninja:
   ```bat
   cmake -S . -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release
   ```
3. Run `build\MonoHarness_artefacts\Release\MonoHarness.exe` — expect `ALL TESTS PASSED`.
4. Install: copy `Monosynth.vst3` to `C:\Program Files\Common Files\VST3\`, then
   rescan plugins in your DAW. No ASIO SDK or extra setup required.

## Repository layout

- `Source/` — plugin DSP, UI, preset backend
  (`DSP.h` voice/filter, `FX.h` chorus/delay/reverb, `PluginProcessor.*`,
  `PluginEditor.*`, `PresetBrowser.*`, `Parameters.h`, `PresetRecipes.*`)
- `Factory/*.xml` — the 356 factory presets (data, editable by hand;
  regenerate with PresetDump after recipe changes)
- `Assets/` — embedded fonts + OFL notice
- `Test/` — headless regression suite (`Harness.cpp`) + preset generator (`Dump.cpp`)
