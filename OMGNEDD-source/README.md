# OMGNEDD

A vocal character processor, built as a VST3 plugin with JUCE 8 and drawn as a
piece of studio rack hardware.

Three character engines — **UNDERWATER**, **DISTORTION**, **SATURATION** — sit
under one CHARACTER macro, with a seven band equaliser, a compressor, a
de-esser, six intelligent vocal macros and a final mix stage around them.

## The workflow

1. Load OMGNEDD.
2. Pick an engine.
3. Turn CHARACTER.
4. Set MIX.
5. Refine with the EQ, the compressor and the macros.
6. Save a preset, automate anything, open ADV for the engineering controls.

## Building

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

JUCE 8.0.8 is expected as a sibling checkout at `JUCE/` (`git clone --depth 1
--branch 8.0.8 https://github.com/juce-framework/JUCE.git`). The VST3 lands in
`build/OMGNEDD_artefacts/Release/VST3/`.

On Windows, build with the Visual Studio generator and copy
`OMGNEDD.vst3` into `C:\Program Files\Common Files\VST3\`, then rescan plugins
in FL Studio. On Linux the build needs the ALSA, X11, Xrandr, Xinerama, Xcursor,
FreeType, Fontconfig and OpenGL development packages.

## Verifying

```bash
cmake --build build --target OMGNEDD_Tests
./build/OMGNEDD_Tests_artefacts/Release/OMGNEDD_Tests
```

The verification target runs the real processor and the real editor. It checks
that every parameter exists and carries a tooltip, that all three engines, all
eight distortion algorithms, all five saturation models and all four
oversampling settings stay finite at extreme settings, that the meters, the
analyser and the compressor report measured values, that POWER off passes the
signal through untouched, that session state round-trips, that the editor lays
out and paints at every supported size, and — through `omg::ControlRegistry` —
that **every control on the panel drives a real APVTS parameter and every
parameter is reachable from the interface**. It also writes
`omgnedd-ui.png`, `omgnedd-ui-small.png` and `omgnedd-advanced.png` so the
layout can be inspected without a host.

## Source layout

```
Source/
  Parameters.h/.cpp        the single parameter table: ids, ranges, choices,
                           tooltips. The APVTS layout, the right-click menus
                           and the advanced panel are all generated from it.
  PluginProcessor.*        chain, oversampling, metering, state
  PluginEditor.*           window, scaling, responsive layout, overlays
  StateManager.*           A/B slots and undo/redo over the whole state
  Presets/                 factory bank, user presets on disk, favourites
  DSP/
    Utils.h                smoothers, envelope followers, LFOs, meter source
    UnderwaterEngine.h     modulated resonant water filter, murk, pressure, bubble
    DistortionEngine.h     eight algorithms, bias, asymmetry, bit and rate crush
    SaturationEngine.h     five models, density, harmonics, tone, soft clip
    EqSection.h            seven bands, six shapes each, optional dynamic mode
    CompressorSection.h    six detector characters, filtered sidechain, auto
    DeEsser.h              split-band reduction with LISTEN
    OutputStage.h          M/S width, mono compatibility, limiter, safety
    MacroEngine.h          CHARACTER and the six vocal macros
    SpectrumSource.h       the FFT behind the EQ curve
  UI/
    Theme.h/.cpp           design tokens, type, brushed metal, recesses, screws
    OmgLookAndFeel.*       menus, alerts, the three-line parameter tooltip
    OmgKnob.*              the rotary control
    OmgButton.*            the raised metal push button
    OmgSwitch.h            the lever switch
    OmgLED.h               the lens
    OmgMeter.*             level meter and gain-reduction meter
    ModeSelector.*         choice selectors and the compact choice button
    EqGraphComponent.*     the EQ display and analyser
    PresetBrowserComponent.*  the modal preset overlay
    OmgPanel.h             the module chassis
    Wordmark.h TopBar.*    identity and the top strip
    InputSection.h OutputSection.h EffectEngineSection.h
    EqPanel.* DynamicsPanels.h MacroPanel.h AdvancedPanel.h
Resources/                 Archivo and IBM Plex Mono, embedded in the binary
Tests/PluginTest.cpp       the verification target
```

## Signal flow

```
in -> input gain -> EQ -> de-esser -> compressor -> [character engine, oversampled]
   -> global wet/dry -> stereo width and mono compatibility -> limiter -> safety
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
panel, so nothing becomes unreachable.

Every knob shares one set of gestures: drag, shift-drag for fine, wheel, arrow
keys, double-click to reset, right-click for reset, type-in, copy and paste.
Every control is focusable, reports its role and value to a screen reader, and
has a tooltip with its name, one sentence and its current value.

## Visual system

The panel's colours, type, spacing, depth and component rules live in the
OMGNEDD design system, which `Source/UI/Theme.h` mirrors token for token.
