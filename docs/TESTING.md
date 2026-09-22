# Testing

## Running it

```bash
cmake --build build --target OMGNEDD_Tests
./build/OMGNEDD_Tests_artefacts/Release/OMGNEDD_Tests
```

It prints one line per check and exits non-zero if any failed, so it drops
straight into CI. On a headless Linux machine it needs a display, because it
builds and paints the real editor:

```bash
xvfb-run -a ./build/OMGNEDD_Tests_artefacts/Release/OMGNEDD_Tests
```

It also writes `omgnedd-ui.png`, `omgnedd-ui-small.png` and
`omgnedd-advanced.png` into the working directory, so the layout can be looked
at without a host.

---

## What it checks

It instantiates the real `OmgnedProcessor` and the real `OmgnedEditor`. There
are no mocks and no test doubles; a check that passes here is a statement about
the plugin a host will load.

### Parameters

* Every row in the parameter table exists in the APVTS.
* Every row has a tooltip.
* The host sees exactly as many parameters as the table declares, and no more.

### Audio, per engine

For each of UNDERWATER, DISTORTION and SATURATION, at CHARACTER 85 with DAMAGE
and MOTION pushed: the output stays finite, passes signal, and never leaves the
safety ceiling.

Then, individually: all **eleven** distortion algorithms, all **six** saturation
models, all **five** modulation modes and all **four** oversampling settings.

### The modules

* **Multiband** — the pre/post pair stays finite, and pushing the band gains
  measurably changes the result rather than cancelling out.
* **Transient shaper** — finite at its extremes, and the largest step between
  neighbouring samples stays below the threshold a click would show as.
* **Compressor** — reduces and stays finite both PRE and POST the character
  engine, and under both peak and RMS detection, with auto release on.

### Signal conditions

| Condition | Check |
| --- | --- |
| Silence | silence in produces silence out; nothing self-oscillates |
| ×0.0005 (very quiet) | finite, inside the ceiling |
| ×0.05 | finite, inside the ceiling |
| ×0.4 (normal vocal) | finite, inside the ceiling |
| ×4 (far too hot) | finite, inside the ceiling |
| DC | measured over two seconds of output, against two seconds of the same source |
| Denormals | a tail fed silence for 200 blocks reaches true zero rather than crawling |
| 44.1 / 48 / 88.2 / 96 kHz | at 16, 64, 512 and 2048 samples per block, all finite |
| Mono in, mono out | finite, and passes signal |

The DC check is measured over a long stretch deliberately. A single 512-sample
block of a 165 Hz vocal is not a whole number of periods, so its own mean is not
zero either; comparing one block against zero would fail for the wrong reason.
Measured properly, the output's DC sits at the same figure as the source's, with
an asymmetric curve and BIAS in use. That is the DC blocker in `OutputStage`
doing its job — before it was added, this check failed, which is why it exists.

### Clicks and smoothing

* Switching engines on *every block* for thirty blocks does not produce a step
  a click would show as.
* A 36 dB output gain jump is smoothed rather than stepped.
* Each engine keeps its own settings when you switch away and back.

### Randomisation

250 draws from a fixed seed sequence — so a bad patch can be reproduced from its
number rather than merely reported — must all stay finite and inside the
ceiling. A locked module must come back untouched. RANDOM must never have turned
the safety clip off.

### The factory bank

Every factory preset is loaded in turn, fed a vocal-like signal, and must stay
finite and inside the ceiling. A preset that produces something unusable is a
build failure, not something a user discovers.

### State

* A session state round-trips through `getStateInformation` /
  `setStateInformation`.
* A factory preset loads, and the bank is present.
* POWER off passes the signal through **sample for sample** untouched, checked
  to within 1e-6.

### The editor

* It constructs, lays out and paints at 900×600, 1200×720, 1500×900 and
  1800×1080.
* Every control on the panel drives a real APVTS parameter.
* Every parameter is reachable from the interface.

Those last two are the ones that matter most. `omg::ControlRegistry` records the
parameter each control binds to as the control is built; the test walks it in
both directions. A decorative knob, a meter wired to nothing, or a parameter
with no control anywhere all fail the build.

---

## The test signal

`fillVocalLike` generates a 165 Hz fundamental with nine harmonics at 1/n
amplitude, under a syllable envelope at 3 Hz with a compressed shape. It is not
a vocal, but it has the two properties that matter for testing this plugin: a
harmonic stack dense enough that a nonlinearity has something to fold, and a
level that moves, so the dynamics and the transient shaper have something to
work on. White noise, which the engine checks also use, is the harder case for
finiteness but tells you nothing about the dynamics.

---

## CPU

`Tests/Bench.cpp`, built with `-DOMGNEDD_BUILD_BENCH=ON`, runs 4000 blocks of
256 samples at 48 kHz per configuration and reports the fraction of one core
used.

Measured on one core of a 2-core cloud VM (GCC 13, `-O3`), which is a
deliberately modest machine:

| Configuration | Fraction of one core |
| --- | --- |
| Underwater, no oversampling | 3.5 % |
| Underwater, 2x | 5.1 % |
| Distortion, 2x | 3.5 % |
| Distortion, 4x | 4.5 % |
| Distortion, 8x | 6.8 % |
| Saturation, 2x | 5.1 % |
| Distortion, 4x + modulation + multiband | 5.4 % |
| Everything on, 8x | 9.6 % |

A modern desktop will be well under half these figures. The practical reading:

* 2x is the default and costs about 1.5 % over no oversampling.
* Going 2x → 8x roughly doubles the total. It is worth it for extreme CRUSH and
  FOLD settings, where the curve generates content well above the signal, and
  is not worth it otherwise.
* The modulation section and the multiband split together cost under 1 %.
* The multiband module reports itself inactive when all three bands are at 0 dB
  and is skipped entirely, so it is free when it is not in use. The same is true
  of the modulation section when it is switched off and of the transient shaper
  when both its controls are centred.

Re-run the benchmark on your own machine rather than trusting these figures for
capacity planning.

---

## Testing in FL Studio

The automated suite covers the DSP, the state and the interface. It cannot cover
the host, so these are done by hand:

1. Build the VST3 and copy it to `C:\Program Files\Common Files\VST3\`.
2. Rescan in **Options → Manage plugins → Find more plugins**.
3. Load OMGNEDD on a recorded vocal.
4. Work through UNDERWATER, DISTORTION and SATURATION. Switching between them
   should not click.
5. Automate CHARACTER and MIX across a section. Both should move smoothly.
6. Toggle FL Studio's own bypass, and separately the plugin's POWER.
7. Load several factory presets, then save a user preset and recall it.
8. Save the project, close FL Studio, reopen it. The settings, the window size
   and the RANDOM locks should come back.
9. Render the section offline and compare against what you heard.
10. Route another track into the sidechain and check that EXTERNAL SC responds.

The build instructions for each of these steps are in [BUILD.md](BUILD.md).

---

## What is not covered

Stated plainly rather than left to be discovered:

* **No automated host testing.** The suite runs the processor directly. It does
  not run FL Studio, a VST3 validator or a plugin host, so host-specific
  behaviour — parameter gesture reporting, bypass ramping, PDC in a particular
  host — is verified by hand, not by the build.
* **No measured aliasing figure.** The oversampling settings are checked for
  finiteness, not against a spectral threshold. The choice of 2x as the default
  is a judgement, not a measurement this suite makes.
* **No listening test in the build.** "Musically useful" is not something a
  build can assert. The suite checks that every preset is *safe*; whether it is
  *good* is a human question.
* **The editor is painted, not driven.** Layout and painting are checked at four
  sizes and the control-to-parameter binding is checked exhaustively, but mouse
  gestures, drag behaviour and keyboard focus order are not simulated.
* **Windows and macOS are not built in CI here.** The suite is portable and the
  code has no platform-specific paths beyond JUCE's own, but the figures and the
  runs in this document are from Linux.
