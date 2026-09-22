# OMGNEDD

A vocal character processor, built as a VST3 plugin with JUCE 8 and drawn as a
piece of studio rack hardware.

Three character engines — **UNDERWATER**, **DISTORTION**, **SATURATION** — sit
under one CHARACTER macro, with a seven band equaliser, a compressor that can
work either side of the character engine, a de-esser, a vocal attack shaper, a
pitch and time modulation section, multiband drive, six intelligent vocal macros
and a final mix stage around them.

Everything on the panel is a real parameter driving real DSP. The build proves
it: the verification target refuses to pass if a control exists that is not
attached to an `AudioProcessorValueTreeState` parameter, or if a parameter
exists that no control can reach.

## The workflow

1. Load OMGNEDD on a recorded vocal.
2. Pick an engine.
3. Turn CHARACTER.
4. Set MIX.
5. Refine with the EQ, the compressor, the de-esser and the six macros.
6. Save a preset, automate anything, open ADV for the engineering controls.

RANDOM draws a complete patch whose parts were chosen against each other rather
than independently; LOCK decides which modules it is allowed to touch.

## Building

```bash
git clone --depth 1 --branch 8.0.8 https://github.com/juce-framework/JUCE.git JUCE
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
```

JUCE 8.0.8 is expected as a sibling checkout at `JUCE/`. The VST3 lands in
`build/OMGNEDD_artefacts/Release/VST3/OMGNEDD.vst3`.

Full instructions, including the Visual Studio commands, where to copy the
plugin and how to make FL Studio find it, are in
[docs/BUILD.md](docs/BUILD.md).

## Verifying

```bash
cmake --build build --target OMGNEDD_Tests
./build/OMGNEDD_Tests_artefacts/Release/OMGNEDD_Tests
```

The verification target runs the real processor and the real editor. What it
checks, and why each check is there, is in [docs/TESTING.md](docs/TESTING.md).

## Documentation

| Document | What it covers |
| --- | --- |
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | Signal flow, the parameter table, state, presets, the interface |
| [docs/DSP.md](docs/DSP.md) | What every engine and module actually does, stage by stage |
| [docs/BUILD.md](docs/BUILD.md) | Build commands, installation, FL Studio, troubleshooting |
| [docs/TESTING.md](docs/TESTING.md) | The verification target, the benchmark, what is and is not covered |
| [docs/PRESETS.md](docs/PRESETS.md) | The factory bank, user presets, favourites, RANDOM and the locks |

## Source layout

```
CMakeLists.txt             plugin, verification target, optional benchmark
Source/
  Parameters.h/.cpp        the single parameter table: ids, ranges, choices,
                           tooltips. The APVTS layout, the right-click menus
                           and the advanced panel are all generated from it.
  PluginProcessor.*        chain, oversampling, sidechain, metering, state
  PluginEditor.*           window, scaling, responsive layout, overlays
  StateManager.*           A/B slots and undo/redo over the whole state
  Randomizer.h             RANDOM and the per-module locks
  Presets/                 factory bank, user presets on disk, favourites
  DSP/
    Utils.h                smoothers, envelope followers, LFOs, meter source
    UnderwaterEngine.h     modulated resonant water filter, murk, pressure, bubble
    DistortionEngine.h     eleven algorithms, bias, asymmetry, bit and rate crush
    SaturationEngine.h     six models, density, harmonics, tone, soft clip
    MultibandDrive.h       low / mid / high drive into the character engine
    ModulationEngine.h     chorus, micro pitch, vibrato, tape wow, tape flutter
    TransientShaper.h      vocal attack and body
    EqSection.h            seven bands, six shapes each, optional dynamic mode
    CompressorSection.h    six characters, peak or RMS, internal or external
                           sidechain, auto gain, auto release, PRE or POST
    DeEsser.h              split-band reduction with LISTEN
    OutputStage.h          DC blocker, M/S width, mono compatibility, limiter
    MacroEngine.h          CHARACTER and the six vocal macros
    SpectrumSource.h       the FFT behind the EQ curve
  UI/                      the panel: theme, look and feel, knobs, meters,
                           switches, selectors, the EQ display, the modules
Resources/                 Archivo and IBM Plex Mono, embedded in the binary
Tests/PluginTest.cpp       the verification target
Tests/Bench.cpp            the CPU benchmark
docs/                      the documents listed above
```

## Signal flow

```
in -> input gain -> EQ -> de-esser -> vocal attack -> [compressor, if PRE]
   -> [multiband split -> character engine -> multiband join, oversampled]
   -> modulation -> [compressor, if POST] -> dry/wet levels and MIX
   -> DC blocker -> stereo width and mono compatibility -> limiter -> safety
   -> output gain -> out
```

CHARACTER and the six macros are not DSP parameters of their own: every block
they are applied as offsets on top of the per-engine controls, so one macro
moves several real parameters at once.

## Interface notes

The window is 1200 x 720 by default and resizable from 900 x 600; SETTINGS has
75 / 100 / 125 / 150 % presets. The layout is written in design units and scaled
as one piece, so nothing is distorted.

At smaller sizes secondary rows are dropped first: the de-esser module below
1150 px wide, and the compressor's KNEE, MAKEUP, MIX and SIDECHAIN HPF row when
the lower strip is short. Those four are always available in the advanced
panel, so nothing becomes unreachable. The advanced panel itself scrolls, so a
group is never clipped however small the window is.

Every knob shares one set of gestures: drag, shift-drag for fine, wheel, arrow
keys, double-click to reset, right-click for reset, type-in, copy and paste.
Every control is focusable, reports its role and value to a screen reader, and
has a tooltip with its name, one sentence and its current value.

## Licence

See [LICENSE](LICENSE). JUCE is licensed separately; see the JUCE repository.
