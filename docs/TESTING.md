# Testing

## Running it

```bash
cmake --build build --target OMGNEDD_Tests
./build/OMGNEDD_Tests_artefacts/Release/OMGNEDD_Tests            # everything
./build/OMGNEDD_Tests_artefacts/Release/OMGNEDD_Tests --quick    # skips the knob sweep
./build/OMGNEDD_Tests_artefacts/Release/OMGNEDD_Tests --report   # prints every knob's figure
```

It prints one line per check and exits non-zero if any failed, so it drops
straight into CI. On a headless Linux machine it needs a display, because it
builds, paints and clicks the real editor:

```bash
xvfb-run -a ./build/OMGNEDD_Tests_artefacts/Release/OMGNEDD_Tests
```

It also writes `omgnedd-ui.png`, `omgnedd-ui-small.png`, `omgnedd-advanced.png`,
`omgnedd-fx.png` and `omgnedd-space.png` into the working directory, so the
layout can be looked at without a host.

The full run takes a few minutes, most of it the knob sweep, which renders
every parameter twice through the whole plugin.

---

## Measuring the sound

The first version of this suite checked that audio stayed finite and under the
ceiling, that every control was bound to a parameter, and that the editor
painted. A plugin whose knobs do nothing passes all of that, and one did: it
shipped with engines running at the wrong sample rate and sixteen knobs that
moved the sound by less than 1 dB. So the suite now measures the sound itself.
The code is in `Tests/SoundProbe.h` and `Tests/SoundTests.h`.

### The probe voice

`probe::Voice` synthesises a stereo vocal: a glottal pulse train through three
moving formants, a pitch contour, syllable envelopes, sibilant bursts above
5.2 kHz and plosive clicks. The right channel has different harmonic phases and
its own breath noise, so width and M/S controls have side content to act on.
It is not a singer, but it has what every module needs: harmonics for a curve to
fold, formants for a filter to move against, level changes for dynamics, and
S sounds for the de-esser.

### What is measured

1.2 seconds of output after the plugin has settled, analysed three ways:

* **Octave bands**, 63 Hz to 16 kHz, averaged over the whole render, for the
  mid and the side separately.
* **A spectrogram**, band levels frame by frame, so anything that moves over
  time (a wobble, a wave LFO, ducking) shows up even when its average does not.
* **Fine bins** for pitch and time controls (detune, ripple, pitch shift, delay
  time, LFO rates), where the change is in where the energy sits rather than how
  much of it there is.

The difference between two renders is the largest band difference, capped at
40 dB, ignoring bands more than 40 dB below the loudest so noise floors cannot
count.

### Every parameter changes the sound

Each parameter is rendered at two points of its range in a context where it
can be heard (its section switched on, its engine selected, an EQ band made a
bell before its Q is moved, and so on), and must move the sound by at least:

* **2 dB** for musical controls;
* **1 dB** for engineering controls whose job is subtle by design: EQ Q,
  compressor timing, knee, ratio, detector and placement, sidechain filter,
  de-esser timing, limiter, crossover points, pressure response, chaos.

Eight parameters are exempt from the sweep because they must not change a
normal vocal, and each has its own test instead: POWER, PHASE, OVERSAMPLING,
TRUE PEAK, SAFETY, LISTEN, EXTERNAL SC and SC AMOUNT.

`--report` prints every figure.

### Invariance and repeatability

* **Oversampling must not change the sound.** The engine's octave-band
  response at OFF, 2x and 4x is compared with 8x, on the linear path and with
  drive on: under 1 dB everywhere within 40 dB of the loudest band. This is the
  check that would have caught the original sample rate bug, which moved the
  spectral centroid of one patch from 193 Hz to 275 Hz across the settings.
* **44.1 kHz and 96 kHz sessions shape the vocal the same way**, within 1 dB
  after removing a uniform level offset.
* **Two instances with the same settings render identically**, random
  modulation included, because every random source is seeded.

### Tempo and time

* A synced 1/4 wobble repeats every beat at 120 and 90 BPM, within 4 %, with the
  host transport supplied through `setTransportOverride`.
* A 1/8 dotted delay at 120 BPM echoes at 375 ms, within 3 ms.
* REVERB DECAY sets the tail: 1.2 s after an impulse, the 6 s setting is more
  than 15 dB louder than the 1 s setting.
* Maximum delay feedback with a 12 s reverb dies away rather than running away:
  more than 30 dB down 36 s after the input stops.

### Specific jobs

| Check | Pass condition |
| --- | --- |
| De-esser | more than 3 dB off the band above the split |
| EXTERNAL SC | a loud key on the sidechain bus compresses a quiet vocal by more than 6 dB |
| EXTERNAL SC off | the same key is ignored, under 0.5 dB |
| PHASE | the output is an exact inversion |
| SAFETY | holds 0 dBFS with the limiter off and +24 dB of input gain, and again with +24 dB of output gain |
| TRUE PEAK | peaks reconstructed at 8x stay at the -1 dB ceiling |
| Engine switch | switching mid-phrase adds no sample step larger than 1.6x the vocal's own largest |

### Knobs respond to the mouse

Every knob visible on the panel is sent a synthesised 60 pixel drag and a mouse
wheel movement, and the parameter must move both times. The drag is also
checked to have written through to the value the DSP reads. This is the
difference between "every knob is bound to a parameter" and "every knob can be
turned".

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
The meters, the analyser and the gain reduction readout must report real
figures, not zeros.

### The modules

* **Multiband**: the pre/post pair stays finite, and pushing the band gains
  measurably changes the result rather than cancelling out.
* **Transient shaper**: finite at its extremes, and the largest step between
  neighbouring samples stays below the threshold a click would show as.
* **Compressor**: reduces and stays finite both PRE and POST the character
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
doing its job. Before it was added, this check failed, which is why it exists.

### Clicks and smoothing

* Switching engines on *every block* for thirty blocks does not produce a step
  a click would show as.
* A 36 dB output gain jump is smoothed rather than stepped.
* Each engine keeps its own settings when you switch away and back.

### Randomisation

250 draws from a fixed seed sequence (so a bad patch can be reproduced from its
number rather than merely reported) must all stay finite and inside the
ceiling. A locked module must come back untouched. RANDOM must never have turned
the safety clip off.

### The factory bank

Every factory preset is loaded in turn, fed a vocal-like signal, and must stay
finite and inside the ceiling. The bank must also cover every engine, the
UNDERGROUND set, the WOBBLE set and the hybrids. A preset that produces
something unusable is a build failure, not something a user discovers.

### State

* A session state round-trips through `getStateInformation` /
  `setStateInformation`.
* A factory preset loads, and the bank is present.
* POWER off passes the signal through untouched, delayed by exactly the latency
  the plugin reports, checked to within 1e-6. Host bypass does the same.

### The editor

* It constructs, lays out and paints at 900x600, 1200x720, 1500x900 and
  1800x1080, and the render is checked for *content*, not just dimensions:
  at least 90 % of the image must be non-transparent and it must contain at
  least 24 distinct colours.
* The TONE, FX and SPACE pages of the rack each paint.
* Every control on the panel drives a real APVTS parameter.
* Every parameter is reachable from the interface.

Those last two are the ones that matter most. `omg::ControlRegistry` records the
parameter each control binds to as the control is built; the test walks it in
both directions. A decorative knob, a meter wired to nothing, or a parameter
with no control anywhere all fail the build.

---

## The test signals

The engine, module and safety checks use `fillVocalLike`, a 165 Hz harmonic
stack under a 3 Hz syllable envelope, and white noise, which is the harder case
for finiteness. Everything that measures the sound uses the probe voice
described above.

---

## CPU

`Tests/Bench.cpp`, built with `-DOMGNEDD_BUILD_BENCH=ON`, runs 4000 blocks of
256 samples at 48 kHz per configuration and reports the fraction of one core
used.

Measured on one core of a 2-core cloud VM (GCC 13, `-O3`), which is a
deliberately modest machine:

| Configuration | Fraction of one core |
| --- | --- |
| Underwater with its WATER wash, no oversampling | 6.0 % |
| Underwater with its WATER wash, 2x | 8.3 % |
| Distortion, 2x | 4.7 % |
| Distortion, 4x | 7.1 % |
| Distortion, 8x | 11.7 % |
| Saturation, 2x | 6.6 % |
| Saturation, 2x, plus a wobble | 7.4 % |
| Saturation, 2x, plus reverb and delay | 8.3 % |
| Distortion, 4x, plus modulation, pitch layer, multiband and transient | 10.2 % |
| Underwater, everything on, 8x | 27.5 % |

A modern desktop core is roughly two to three times faster than this one. The
practical reading:

* 2x is the default. The sound does not change with the setting (the build
  checks that), only the aliasing does, so 2x is right unless heavy CRUSH or
  FOLD is producing audible aliasing.
* The underwater engine is the most expensive per oversampled sample, because
  its filter cutoff moves every sample. At 8x with everything else on it is the
  heaviest configuration the plugin has.
* The reverb and delay together cost about 2 % of this core.
* Modules that are switched off are skipped: multiband at 0 dB on all bands,
  FX and SPACE once their fade-out has finished, the transient shaper when both
  controls are centred.

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

## Sanitizers

The suite is also built with AddressSanitizer and UndefinedBehaviorSanitizer
and run in full:

```bash
cmake -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined,float-cast-overflow -fno-omit-frame-pointer"
cmake --build build-asan --target OMGNEDD_Tests
ASAN_OPTIONS=detect_leaks=0 xvfb-run -a ./build-asan/OMGNEDD_Tests_artefacts/RelWithDebInfo/OMGNEDD_Tests
```

Leak detection is off because the X11 and font libraries hold allocations for
the life of the process. This run found a real bug: a delay line read that
could land one sample past the end of its buffer (see
[DSP.md](DSP.md#filtersh-the-shared-building-blocks)). In the normal suite
it most likely showed up only as an occasional runaway reverb tail, depending
on what happened to sit in memory after the buffer.

---

## What is not covered

Stated plainly rather than left to be discovered:

* **No automated host testing.** The suite runs the processor directly. It does
  not run FL Studio, a VST3 validator or a plugin host, so host-specific
  behaviour (parameter gesture reporting, PDC in a particular host) is verified
  by hand.
* **No measured aliasing figure.** Oversampling is checked for not changing the
  tone and for finiteness, not against an aliasing threshold. 2x as the default
  is a judgement.
* **No listening test in the build.** The suite proves that every knob moves
  the sound by a measurable amount and that every preset is safe. Whether a
  preset is *good* on a real vocal is a human question, and the probe voice is
  a synthesiser, not a singer.
* **Keyboard focus order** is not simulated. Drags and the wheel are.
* **macOS is not built here.** Windows and Linux are both built and run.

## Two platform notes, both learned the hard way

**Render into a software image.** `juce::Image (format, w, h, clear)` asks for
the platform's native image type. Under JUCE 8 on Windows that is Direct2D
backed, and painting a component into one off-screen then reading it back
through `BitmapData` yields nothing at all: every pixel transparent. The suite
therefore builds its render targets with `SoftwareImageType`, which behaves the
same on every platform. This only surfaced because the paint check looks at
pixels; a check that asserted image dimensions passed happily against a
completely blank panel, which is the reason the content check exists.

**Log to stdout explicitly.** JUCE's default logger writes to the platform
debugger on Windows, so a redirected run captured an empty file while still
exiting 0. The suite installs its own `Logger` that writes to `std::cout`, so
the same command produces the same report everywhere.
