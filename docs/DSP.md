# DSP

What each module actually does. Every figure here is in the source; nothing in
this document describes an intention rather than code.

All modules are sample-rate independent: every coefficient is derived from the
`ProcessSpec` handed to `prepare()`, and `prepareToPlay` is re-run by the host
whenever the rate or the block size changes.

---

## The chain

```
in
 │
 ├─ input gain ....................... smoothed, 20 ms
 │
 ├─ EqSection ........................ 7 bands, optional dynamic mode
 ├─ DeEsser .......................... split-band, LISTEN
 ├─ TransientShaper .................. vocal attack and body
 ├─ CompressorSection ................ if PLACEMENT = PRE
 │
 ├─ oversampler up (OFF / 2x / 4x / 8x)
 │   ├─ MultibandDrive::applyPre ..... low / mid / high gains
 │   ├─ UnderwaterEngine | DistortionEngine | SaturationEngine
 │   └─ MultibandDrive::applyPost .... the inverse gains
 ├─ oversampler down
 │
 ├─ ModulationEngine ................. chorus / micro pitch / vibrato / wow / flutter
 ├─ CompressorSection ................ if PLACEMENT = POST
 │
 ├─ dry level, wet level, MIX ........ all three smoothed, 20 ms
 │
 ├─ OutputStage
 │   ├─ DC blocker ................... one pole at 10 Hz, always on
 │   ├─ M/S encode, width, side level
 │   ├─ mono compatibility ........... progressive low narrowing of the side
 │   ├─ M/S decode, air, phase, mono
 │   ├─ limiter ..................... optional, with true-peak mode
 │   └─ safety clip .................. optional hard guard at 0 dBFS
 │
 ├─ output gain ...................... smoothed, 20 ms
 └─ non-finite guard ................. anything not finite is zeroed
out
```

The oversampler wraps only the nonlinear block. Filtering, dynamics and
modulation run at the session rate, because oversampling them buys nothing and
costs a great deal. See [Oversampling](#oversampling).

---

## UnderwaterEngine

`Source/DSP/UnderwaterEngine.h`

Not a low-pass filter with a nice name. Per channel, per sample:

1. **Murk** — a low shelf at 220 Hz lifting up to +9 dB and a high shelf at
   3.2 kHz cutting up to −14 dB, moved together by one control. This is the
   part that makes the vocal sound like it is behind something rather than just
   quiet at the top.
2. **Ripple and wave** — two LFOs (sine, triangle, random or square) at
   2.5–11 Hz and 0.05–0.9 Hz. They sum into one figure that moves the filter
   cutoff exponentially, up to about ±2.2 octaves at full depth. The right
   channel's LFOs are offset by MOD PHASE, which is what gives the movement
   width rather than making it a mono wobble.
3. **The water filter** — one to four cascaded state-variable low-pass stages
   (12 / 24 / 36 / 48 dB per octave). The first stage carries the resonance;
   the rest are at 0.707 so the cascade does not ring four times over. The
   cutoff runs from 18 kHz down to about 180 Hz as DEPTH and WATER close, and
   the target is smoothed over 4 ms so a fast modulator cannot step it.
4. **Pressure** — downward compression with a soft knee whose threshold and
   ratio both follow one control, then a `tanh` density stage with matched
   makeup. PRESSURE RESPONSE sets the attack and release together, 60 ms/600 ms
   at the slow end down to 2 ms/60 ms at the fast end.
5. **Bubble** — a resonant band-pass around 5–9 kHz mixed back in, amplitude
   modulated by the ripple LFO, which is the watery shimmer on top.
6. **Engine mix** — UW MIX blends the result against what fed it, before the
   plugin's global MIX.

With CHAOS on, a small bounded random term joins the modulation figure before
the cutoff smoother, so repeated passes are never identical. It goes in ahead of
the smoother deliberately: the smoother is what stops it becoming noise.

---

## DistortionEngine

`Source/DSP/DistortionEngine.h`

A multi-stage architecture, not a waveshaper on its own.

```
in -> BODY low shelf -> BITE peak -> pre-emphasis high shelf
   -> drive, bias, asymmetry -> curve -> bias removal
   -> bit and rate reduction -> de-emphasis -> SMOOTH low pass -> makeup -> mix
```

* **BODY** is a low shelf at 190 Hz, ±4 dB, applied before the curve, so it
  changes what gets distorted rather than what comes out.
* **BITE** is a peak at 3.1 kHz, up to +8 dB, likewise before the curve.
* **Pre-emphasis** lifts above 2.6 kHz going in and takes the same amount back
  out afterwards. The curve therefore works harder on the top end than on the
  bottom, which is the classic way of keeping a distorted vocal intelligible.
* **Drive** is up to +34 dB into the curve.
* **Bias** shifts the signal off centre before the curve and is subtracted
  afterwards, which generates even harmonics without leaving DC behind.
* **Asymmetry** scales the two halves of the waveform differently.

The eleven curves:

| Algorithm | What it is |
| --- | --- |
| SOFT | `tanh`, the safe default |
| HARD | hard clip with a variable pre-gain from EDGE |
| TUBE | `tanh` with a different slope below zero |
| TAPE | `tanh` with a cubic term removed, so it rounds rather than squares |
| FUZZ | exponential saturation, very compressed |
| FOLD | four-pass wavefolder |
| DIGITAL | coarse quantiser, EDGE sets the step count |
| RECTIFY | mostly-rectified, mixed back with a fifth of the original |
| SHAPER | third and fifth Chebyshev terms over the fundamental |
| BITCRUSH | pure quantiser, as an algorithm rather than a blend |
| ASYM | `tanh` above zero, a soft rational curve below |

* **CRUSH** is separate from the algorithms and runs after the curve: bit depth
  16 down to 3 and decimation 1x down to 12x, blended rather than switched, so
  it can be dialled in rather than only turned on.
* **SMOOTH** and POST FILTER set a low pass after the curve, down to 900 Hz.
  This is what makes heavy settings usable on a vocal rather than only loud.
* **Makeup** divides by `1 + drive * 2.2`, so DRIVE changes the character and
  not the level. That matters more than it sounds: without it, every A/B
  comparison is won by whichever setting is louder.

---

## SaturationEngine

`Source/DSP/SaturationEngine.h`

```
in -> warmth low shelf -> density -> drive -> curve -> harmonics
   -> transformer high pass -> tone tilt -> soft clip -> makeup -> thickness -> mix
```

* **Warmth** is a low shelf at 260 Hz, up to +6 dB.
* **Density** is a gentle upward leveller ahead of the curve, so a quiet phrase
  reaches the nonlinearity as readily as a loud one. This is what makes the
  saturation respond to the performance rather than to the fader.
* **Harmonics** blends in a squared term with the sign of the signal, which is
  even-order content; at zero the curve's own odd harmonics are all there is.
* **Transformer** is a high pass from 22 to 52 Hz that moves with THICKNESS,
  which is what an output transformer does to the bottom end.
* **Tone** is a ±7 dB tilt about 500 Hz / 4.2 kHz.
* **Soft clip** is an independent `tanh` stage with normalised gain, so it
  rounds peaks without adding level.

The six models:

| Model | Curve |
| --- | --- |
| TAPE | `tanh(1.3x)` with a cubic term removed |
| TUBE | asymmetric `tanh`, softer and quieter below zero |
| CONSOLE | `x / (1 + 0.85|x|)`, a gentle rational saturation |
| TRANSFORMER | `tanh` plus a squared and a cubic term, even and odd together |
| WARM | `atan`, normalised |
| CLEAN | third order only, and only near the top: level and glue, almost no colour |

---

## MultibandDrive

`Source/DSP/MultibandDrive.h`

Two Linkwitz-Riley crossovers split the signal into low, mid and high. Each
band is given its own gain and the three are summed again *before* the engine
runs; after the engine the same split is applied with the inverse gains.

Because every curve in the plugin is level dependent, a band pushed 8 dB harder
is distorted considerably more than one left alone. The inverse pass brings the
tonal balance back, so what differs between bands is the *amount of character*,
not the EQ. One engine instance still does the work, so this costs two crossover
pairs rather than three times the nonlinear processing.

This is an approximation and is meant to be: the nonlinearity spreads energy
across the crossover points, so the post split does not see exactly what the pre
split produced. With all three bands at 0 dB the module reports itself inactive
and is skipped entirely, so it costs nothing when it is not in use.

---

## ModulationEngine

`Source/DSP/ModulationEngine.h`

All five modes are built from one primitive: a short interpolated delay line
whose read position is moved. That is what the machines they are named after
did. The line is fed a high-passed copy of the signal (90 Hz) so modulation
never smears the bottom of the voice.

| Mode | Read position |
| --- | --- |
| CHORUS | 14 ms base, a 0.12–3.2 Hz sine moving it up to ±7.5 ms, opposite directions in the two channels, plus a detuned voice |
| MICRO PITCH | two crossfaded taps sliding at a constant rate, which is a fixed pitch offset in cents, up and down at once |
| VIBRATO | 6 ms base, a 0.8–8 Hz sine |
| TAPE WOW | 9 ms base, a 0.15–1.6 Hz sine plus a second incommensurate component so it never repeats obviously |
| TAPE FLUTTER | 5 ms base, a 5–18 Hz sine plus a bounded filtered random walk |

The pitch voices use the two-head approach: the read position slides at
`1 − ratio` samples per sample, and is wrapped with a pair of raised-cosine
windows whose gains sum to one at every point, over a 25 ms window. That is long
enough that the crossover is not heard as tremolo and short enough that the
doubling is not heard as an echo. DETUNE reaches about ±22 cents.

MOTION adds drift on the tape modes and detunes the two channels' LFO rates
slightly, so the movement is never quite regular. WIDTH is the phase offset
between the channels.

The MOTION macro owns this section: past a whisper it switches it on and takes
the depth, the width and the blend with it.

---

## TransientShaper

`Source/DSP/TransientShaper.h`

Two envelope followers watch the same detector signal: one fast (0.5 ms attack,
45 ms release), one slow (28 ms, 320 ms). Where the fast one is above the slow
one the vocal is starting a word; where they have converged it is holding a
note. ATTACK acts on the first, BODY on the second.

It is deliberately gentler than a drum transient designer:

* the gain is bounded to ±9 dB;
* both envelopes are level relative, so it behaves the same on a quiet ad-lib
  and a shouted hook;
* the applied gain is smoothed over 3 ms, so consonants do not click;
* the detector is shared across the pair, so the stereo image stays intact.

---

## CompressorSection

`Source/DSP/CompressorSection.h`

Feed-forward, one detector for the pair so the stereo image is never pulled
apart.

* **Sidechain** — a high pass from 20 to 400 Hz on the detector, so plosives do
  not pump the whole vocal. With EXTERNAL SC on, the detector is fed from the
  plugin's sidechain bus instead, blended by SC AMOUNT.
* **Detection** — PEAK follows the rectified signal; RMS follows a 12 ms mean
  square. Peak catches consonants, RMS follows loudness.
* **Knee** — quadratic interpolation across the knee width, 0 to 24 dB.
* **Characters** — CLEAN, VOCAL, PUNCH, SMOOTH, AGGRESSIVE and OPTO scale the
  attack, the release and the knee. OPTO, for instance, is three times the
  attack, 2.6 times the release and a minimum 10 dB knee.
* **Auto gain** — sets makeup from the threshold and the ratio.
* **Auto release** — two envelopes on the detector, 40 ms and 600 ms. How far
  apart they are says whether the material is transient or sustained, and the
  release time is set from that between 30 and 900 ms. The coefficients are
  only recomputed when the figure has moved more than 2 ms, so this is not
  expensive.
* **Placement** — PRE puts the compressor in front of the character engine, so
  it controls what the engine is fed. POST puts it after, so it controls what
  the engine produced. On a heavy distortion patch POST is usually what you
  want; on a saturation patch PRE usually is.
* **Mix** — parallel blend, per sample.

---

## EqSection

`Source/DSP/EqSection.h`

Seven bands, each able to be any of six shapes (high pass, low shelf, bell,
high shelf, low pass, notch), with frequency, gain, Q, an on switch and a
dynamic switch. Defaults are laid out for a vocal: HPF at 80 Hz, LOW at 150,
LOW MID at 420, MID at 1.2 k, HIGH MID at 3.5 k, HIGH at 8 k, LPF at 18 k.

Dynamic mode makes a band's gain follow the level in its own region, so a band
set to cut only cuts when the vocal pushes into it — mud control, harshness
control and sibilance reduction are all the same mechanism aimed at different
frequencies. Presence and air are the same thing with the sign reversed.

The displayed curve is the summed magnitude of the coefficients the audio
thread is actually running, including the macro offsets, so the picture cannot
drift from the sound.

---

## DeEsser

`Source/DSP/DeEsser.h`

A band-pass isolates the sibilant band and a high pass at 0.8 × the centre
frequency feeds the detector. The reduction is applied by subtracting the part
of the band being removed, rather than by ducking the whole signal, so the vocal
below the band is untouched. RANGE is a hard limit on how far it will ever go.

LISTEN replaces the output with the isolated band, which is how you aim it.

---

## OutputStage

`Source/DSP/OutputStage.h`

* **DC blocker** — a one-pole at 10 Hz, first and not optional. An asymmetric
  or rectifying curve genuinely produces DC, and BIAS is there to be used; left
  in, it costs headroom the limiter then has to take back, and it shows up as a
  thump on a full-range system rather than as anything audible in the vocal.
* **Width and side level** — M/S encode, scale the side, decode.
* **Mono compatibility** — a high pass on the side signal only, moving from 20
  to 320 Hz, which narrows the low end progressively rather than in a step.
* **Air** — a high shelf at 11 kHz, driven by the AIR switch.
* **Limiter** — peak detector with a 50 ms release and a 1.5 ms gain smoother,
  against a set ceiling. TRUE PEAK also tests the midpoint between neighbouring
  samples, which is a cheap approximation of an inter-sample peak and is enough
  to keep a lossy encode from clipping.
* **Safety** — a hard clip at 0 dBFS, independent of the limiter, defeatable in
  the advanced panel. It is the last thing in the stage, so nothing can get past
  it however hard the engine is driven.

---

## MacroEngine

`Source/DSP/MacroEngine.h`

CHARACTER and the six macros are not DSP parameters. Every block they are
applied as offsets on top of the values pulled from the panel, before those
values reach the engines. CHARACTER is centred: at 50 % it leaves the panel
alone, below it pulls back, above it pushes.

Each macro reaches several real parameters. CHARACTER in distortion mode, for
instance, moves DRIVE by up to ±45, BITE by ±25, CRUSH upward by 15, EDGE by
±30 and the compressor threshold by 5 dB at once. DAMAGE reaches the drive, the
crush, the harmonics and the compressor ratio. MOTION reaches the underwater
modulators *and* switches the modulation section on and takes its depth, width
and blend with it. BODY reaches a low shelf, the saturation thickness, the
compressor threshold and the transient shaper's body control.

Because the macros are applied as offsets rather than written back to the
parameters, moving a macro never destroys what the user dialled underneath it.

---

## Oversampling

OFF / 2x / 4x / 8x, using JUCE's polyphase IIR half-band filters. Only the
multiband split, the character engine and the multiband join run inside it.

The three nonlinear engines are prepared at 8 × the session rate at
`prepareToPlay`, so switching the setting mid-session never reallocates and
never asks the allocator for anything on the audio thread. The upsampled
scratch buffer is allocated once at the widest case for the same reason.

Latency is reported to the host through `setLatencySamples`, so a host that
compensates will. The figure is shown live at the bottom of the advanced panel.

2x is the default. It is the point where the aliasing from the vocal-range
content that matters is already below the noise floor of the recording; 4x and
8x are there for extreme CRUSH and FOLD settings, where the curve generates
content an octave or more above the signal.

---

## Real-time discipline

* No allocation in `processBlock`. Every buffer is sized in `prepareToPlay`,
  including the upsampled scratch space and the sidechain copy.
* No locks and no system calls on the audio thread. Values reach the editor
  through `std::atomic`.
* `ScopedNoDenormals` for the whole block, and the modules that hold a decaying
  state are built so a tail reaches zero rather than crawling.
* Every gain the user can move is smoothed over 20 ms; the limiter's gain over
  1.5 ms; the underwater cutoff over 4 ms; the transient gain over 3 ms.
* A final pass zeroes anything not finite, so a pathological host buffer cannot
  propagate a NaN into the session.
* All coefficients are recomputed from the sample rate in `prepare`, and
  frequency-dependent values are clamped against Nyquist, so 96 kHz behaves the
  same as 44.1 kHz.
