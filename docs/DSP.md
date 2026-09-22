# DSP

What each module actually does. Every figure here is in the source. Nothing in
this document describes an intention rather than code.

---

## Why the second version exists

The first version compiled, every knob was attached to a parameter, and it
still sounded flat. Measuring it showed why.

* **The engines ran at the wrong sample rate.** The three character engines
  were prepared once at eight times the session rate, so that switching
  oversampling never had to reallocate, and then run at whatever the
  oversampler actually delivered, which is two times by default. Every filter
  cutoff and every LFO inside them sat at a quarter of its intended frequency.
  The underwater filter at "3 kHz" was at 750 Hz, and the spectral centroid of
  the same patch moved from 193 Hz to 275 Hz depending on the oversampling
  setting. Engines now allocate for the fastest rate once and are *told the
  rate they run at* (`setSampleRate`) whenever the setting changes.
* **Macros clamped the panel dead.** A macro offset was added and the result
  clamped, so a knob at 70 with a +35 offset sat at the ceiling and the top
  third of its travel did nothing. Macros now push proportionally (see
  [MacroEngine](#macroengine)).
* **Sixteen of seventy-four knobs moved the sound by less than 1 dB** in any
  octave band. Each one was rebuilt until it clears a measured threshold; the
  build now checks all of them, every time (see [TESTING.md](TESTING.md)).
* **No space.** The underground vocal lives in a dark wash that blooms between
  words. Without a reverb and a delay every engine sounded dry however it was
  set. SPACE and the WATER wash fix that.

---

## The chain

```
in
 |
 |- input gain ....................... smoothed, 20 ms
 |- dry copy ......................... delayed by the reported latency
 |
 |- EqSection ........................ 7 bands, slopes, dynamic mode
 |- DeEsser .......................... split band, LISTEN
 |- TransientShaper .................. vocal attack and body
 |- CompressorSection ................ if PLACEMENT = PRE
 |
 |- M/S split ........................ only with M/S on: the engine hears the mid
 |- oversampler up (OFF / 2x / 4x / 8x, linear phase FIR)
 |   |- MultibandDrive pre ........... low / mid / high gains
 |   |- Underwater | Distortion | Saturation, 30 ms equal-power switch
 |   '- MultibandDrive post .......... the inverse gains
 |- oversampler down
 |- M/S join ......................... side delayed to match
 |
 |- ModulationEngine ................. pitch layer, chorus, micro pitch, vibrato, wow, flutter
 |- FilterFx ......................... auto wah, wobble, LFO wah, talk, phaser
 |- CompressorSection ................ if PLACEMENT = POST
 |- SpaceEngine ...................... reverb and delay, plus the WATER wash
 |
 |- dry level, wet level, MIX ........ all smoothed, 20 ms
 |- output gain ..................... smoothed, 20 ms
 |
 |- OutputStage
 |   |- DC blocker, width, mono compatibility, air, phase, mono
 |   |- lookahead limiter ............ 1.5 ms, optional true peak
 |   '- safety clip .................. hard guard at 0 dBFS
 |
 '- non-finite guard ................. anything not finite is zeroed
out
```

OUTPUT GAIN is in front of the limiter and the safety clip, not after them, so
nothing on the panel can push the plugin past its ceiling.

The oversampler wraps only the nonlinear block. Filtering, dynamics, movement
and space run at the session rate, because oversampling them buys nothing.

---

## Filters.h: the shared building blocks

Every module uses the same small library, written for this plugin so that
coefficients can be redesigned per block without allocating:

* `BiquadCoeffs` with the RBJ cookbook shapes (low and high pass, band pass,
  notch, all pass, peak, shelves, first order low and high pass) and a
  `magnitudeDb` used by the EQ display.
* `Biquad`, transposed direct form II in double precision, so low cutoffs at
  8x oversampling stay stable.
* `SlopeFilter`, Butterworth cascades for 12, 24, 36 and 48 dB per octave.
* `Svf`, a topology preserving state variable filter, used wherever a cutoff
  moves every sample (underwater, wah, wobble, talk).
* `Lr4Crossover`, fourth order Linkwitz-Riley for the multiband and the
  de-esser.
* `DelayLine`, fractional with linear interpolation, sized once. A read with a
  non-finite or negative delay is treated as zero delay and a read before
  allocation returns silence, so a bad modulation value can never index
  outside the buffer.
* `AutoGain`, a slow level follower that brings a processed signal back to the
  level of the signal that fed it. The engines use it so that DEPTH, DRIVE and
  the algorithm change the character, not the loudness, and an A/B is not won
  by whichever setting is louder.

Every module that uses randomness seeds it with a fixed value on `reset`, so
two bounces of the same session are identical.

---

## UnderwaterEngine

`Source/DSP/UnderwaterEngine.h`

Voiced for the underground and rage vocal: a voice pushed into a dark,
resonant filter with grit in front of it, a pitch that will not sit still, a
filter that breathes and gurgles, a squashed pumping density, and a dark wash
that blooms between the words. Per channel, per sample, inside the
oversampler:

| Stage | What it does |
| --- | --- |
| MURK | a +8 dB peak at 280 Hz for chest weight and a shelf above 2.2 kHz cutting up to 14 dB |
| PRESSURE, drive | `tanh` drive into the filter, 1x to 6x, plus up to 8x more from the DAMAGE macro |
| DEPTH | the filter cutoff, 16 kHz at 0 %, 2.5 kHz at 50 %, 400 Hz at 100 %, exponential |
| WATER | extra resonance on the filter (Q up to 2.4 higher), and the send to the wash |
| SLOPE | one to four cascaded SVF low pass stages, 12 to 48 dB per octave; only the first resonates |
| WAVE | a slow LFO, 0.07 to 1.2 Hz, sweeping the cutoff by up to 2.2 octaves; sine, triangle, random or square |
| BUBBLE | a smoothed random gurgle on the cutoff at 6 to 16 Hz, up to 1.6 octaves, which also lifts the resonance |
| RIPPLE | pitch warble: a short modulated delay at 4.5 to 7.5 Hz, up to 45 cents |
| PRESSURE, pump | a compressor after the filter, threshold -10 to -32 dB, ratio 1:1 to 10:1 |
| PRESSURE RESPONSE | pump attack 40 to 1 ms and release 500 to 60 ms |
| MOD PHASE | offsets the right channel's LFOs, which gives the movement width |

After the pump a 60 Hz high pass takes out the rumble the drive produces, and
auto gain matches the level to the input.

With CHAOS on, a slow bounded random walk joins the cutoff and the warble, so
repeated passes drift rather than loop.

### The WATER wash

The wash has to run at the session rate, so it is not inside the engine.
`getWashAmount()` reports WATER, and the processor follows it over about 80 ms
(so a preset change does not step it) and composes a wash on the SPACE module:

* a dark, ducked reverb: size 72, decay 1.8 s rising to 6.2 s, damping 64 to
  94, 16 ms pre-delay, 58 % ducking, mix pushed up by up to 62 %;
* a dark ping-pong delay on 1/8 dotted: feedback 28 to 56 %, tone 28, duck 62,
  warp 30, mix pushed up by up to 24 %.

If the SPACE page's own reverb or delay is switched on, the wash adds to its
level and leaves its settings alone.

---

## DistortionEngine

`Source/DSP/DistortionEngine.h`

```
in -> BODY low shelf -> BITE presence peak -> pre-emphasis
   -> drive, bias, asymmetry -> curve -> bias removal
   -> CRUSH (bits and rate) -> de-emphasis -> BODY restore
   -> SMOOTH / post low pass -> auto gain -> mix
```

* **BODY** is a low shelf at 200 Hz, -6 to +9 dB, in front of the curve, and
  45 % of it is taken back afterwards, so turning it up makes the low end more
  saturated rather than simply louder.
* **BITE** is a peak at 2.8 kHz, -3 to +12 dB, in front of the curve.
* **PRE-EMPHASIS** lifts above 2.6 kHz by up to 12 dB going in and takes the
  same back out afterwards, so the curve works hardest on the consonants.
* **DRIVE** is up to +40 dB into the curve.
* **BIAS** shifts the signal before the curve and is removed afterwards, which
  makes even harmonics without leaving DC.
* **CRUSH** runs after the curve: 16 bits down to 3, and sample-and-hold
  decimation up to 16x, counted in session-rate samples so it sounds the same
  at any oversampling setting.
* **SMOOTH** closes a low pass from the POST FILTER setting down to 1.8 kHz,
  exponentially, so the whole travel is heard.

| Algorithm | Curve |
| --- | --- |
| SOFT | `tanh`, EDGE steepens it |
| HARD | hard clip, EDGE raises the pre-gain |
| TUBE | `tanh` with a softer, quieter negative half |
| TAPE | `tanh` with a cubic term removed, so it rounds rather than squares |
| FUZZ | exponential saturation, very compressed |
| FOLD | six-pass wavefolder |
| DIGITAL | coarse quantiser, EDGE sets the step count |
| RECTIFY | mostly rectified, a third of the original mixed back |
| SHAPER | third and fifth Chebyshev terms over the fundamental |
| BITCRUSH | pure quantiser, 24 down to 2 levels |
| ASYM | `tanh` above zero, a soft rational curve below |

---

## SaturationEngine

`Source/DSP/SaturationEngine.h`

```
in -> WARMTH -> THICKNESS -> DENSITY -> drive -> model curve -> HARMONICS
   -> transformer high pass -> model tone -> TONE tilt -> SOFT CLIP
   -> auto gain -> mix
```

* **WARMTH** is +8 dB of low shelf at 220 Hz with up to 5 dB off above 6 kHz.
* **THICKNESS** is up to +7 dB at 350 Hz, and moves the transformer high pass
  from 25 to 60 Hz.
* **DENSITY** lifts quiet passages by up to 12 dB into the curve, so the
  saturation follows the performance rather than the fader.
* **DRIVE** is up to +30 dB.
* **HARMONICS** adds an even-order term, strongest where the curve is working.
* **TONE** is a tilt of up to 8 dB about 450 Hz and 3.8 kHz.
* **SOFT CLIP** is a normalised `tanh` stage blended in.

Each model has its own curve and its own tone, so the choice is audible before
the curve does anything:

| Model | Curve | Tone |
| --- | --- | --- |
| TAPE | `tanh(1.3x)` less a cubic term | top rolls off as it is driven, down to -7 dB at 9 kHz |
| TUBE | asymmetric `tanh` | +1.5 dB at 1.4 kHz |
| CONSOLE | `x / (1 + 0.85|x|)` | +2.5 dB at 4.5 kHz |
| TRANSFORMER | `tanh` plus a squared term, low band saturated hardest | +3 dB below 110 Hz |
| WARM | `atan`, normalised | -5 dB above 3.5 kHz |
| CLEAN | cubic, near the top only | none |

---

## MultibandDrive

`Source/DSP/MultibandDrive.h`

Two LR4 crossovers split low, mid and high. Each band gets its own gain, the
engine runs on the sum, and after the engine the same split applies the
inverse gains. A band pushed 8 dB harder is distorted considerably more than
one left alone, and the tonal balance comes back afterwards. The low band goes
through an all pass at the upper crossover so the three bands sum flat. With
all three at 0 dB the module is skipped.

---

## ModulationEngine

`Source/DSP/ModulationEngine.h`

**The pitch layer** (SHIFT and SHIFT MIX, on the FX page) is independent of the
modulation switch. A two-tap shifter, the taps half a 55 ms window apart and
raised-cosine crossfaded, moves the vocal up to an octave either way and blends
it back. -12 under the vocal is the demon layer; +5 on top is the high,
pinched voice.

**The five modes** are all a short interpolated delay whose read position moves:

| Mode | Read position |
| --- | --- |
| CHORUS | two voices around 14 ms moved by a slow sine in opposite directions |
| MICRO PITCH | two crossfaded taps sliding at a constant rate: a fixed offset in cents, up and down at once |
| VIBRATO | one voice, fully wet, sine modulated |
| TAPE WOW | very slow drift with a second incommensurate component |
| TAPE FLUTTER | fast shallow modulation plus a filtered random walk |

MOTION adds a 2.1 Hz random wander to the read position and detunes the two
channels' LFOs, so the movement is never quite regular. WIDTH is the phase
offset between the channels.

---

## FilterFx

`Source/DSP/FilterFx.h`

The moving filters, for wobble and wah vocals:

| Mode | What it does |
| --- | --- |
| AUTO WAH | an envelope follower on the vocal opens a resonant band pass as the voice gets louder; SENS sets how easily |
| WOBBLE | a 24 dB resonant low pass swept by the LFO, with `tanh` drive in front |
| LFO WAH | the wah band pass swept by the LFO instead of the voice |
| TALK | three formant filters morphing through A E I O U: the talk box, yoi-yoi effect |
| PHASER | six first order all passes swept by the LFO, with feedback |

* **FREQ** is the bottom of the sweep, 60 Hz to 8 kHz; **DEPTH** is the range
  above it, up to 5 octaves. In TALK mode FREQ scales all three formants.
* **RESO** sets Q from 0.6 to about 15.
* **SHAPE**: sine, triangle, saw up, saw down, square, sample and hold.
* **SYNC** locks the LFO to the host. When the host is playing, the LFO phase is
  computed from the song position, so a 1/4 wobble lands on the beat every time
  instead of drifting against it. Stopped, it free-runs at the synced rate.
  Divisions run from 1/1 to 1/32, with dotted and triplet values.
* **STEREO** offsets the right channel's LFO by up to 180 degrees.
* **MIX** fades the module in and out over 30 ms, so switching it never clicks.

---

## SpaceEngine

`Source/DSP/SpaceEngine.h`

**Reverb.** An eight line feedback delay network with a Householder matrix.
Line lengths run from 37 to 81 ms scaled by SIZE, and are slowly modulated so
the tail does not ring metallically. Four input diffusers smear the onset.
DECAY is a real RT60: each line's gain is computed from its own length, so the
whole network decays at the same rate. DAMP closes a low pass inside the loop
from 16 kHz down to 1.4 kHz, and a matching one on the input. PRE-DELAY is up
to 250 ms. The output is high passed at 140 Hz so the wash never muddies the
low end.

**Delay.** Stereo, synced to a division or free from 1 ms to 3 s. FEEDBACK goes
to 95 % and is gently saturated so it can never run away. TONE filters the
output as well as the feedback, from dark (1.2 kHz low pass, 240 Hz high pass)
to open. PING PONG feeds the mono sum into the left line only and crosses the
feedback. WARP adds tape wow (0.55 Hz) and flutter to the repeats. The repeats
are also fed into the reverb, so echoes smear into the wash.

**Duck.** Both follow the dry vocal and pull their own level down by up to
20 dB while it is loud, so the wash swells in the gaps.

**Mix.** Dry stays at full level up to 50 % while the wet comes in; past 50 %
the dry fades out, so 100 % is fully wet.

---

## TransientShaper

`Source/DSP/TransientShaper.h`

Two envelope followers on the same signal, one fast and one slow. Where the
fast one is above the slow one a word is starting, and ATTACK acts on it;
where they have converged a note is holding, and BODY acts on it. The gain is
limited to +/-9 dB, smoothed over 3 ms, and the detector is shared across the
pair so the stereo image stays intact.

---

## CompressorSection

`Source/DSP/CompressorSection.h`

Feed-forward, one detector for the pair.

* **Sidechain**: a high pass from 20 to 400 Hz on the detector. With EXTERNAL
  SC on, the detector takes the plugin's sidechain bus instead, blended by SC
  AMOUNT.
* **Detection**: PEAK or RMS (12 ms).
* **Knee**: quadratic, 0 to 24 dB.
* **Characters** change timing, and two of them change more than timing:

| Mode | Attack | Release | Other |
| --- | --- | --- | --- |
| CLEAN | as set | as set | |
| VOCAL | x0.8 | x1.1 | knee at least 6 dB |
| PUNCH | x2.4 | x0.55 | knee x0.4 |
| SMOOTH | x1.6 | x1.8 | knee at least 12 dB, RMS detection |
| AGGRESSIVE | x0.25 | x0.45 | ratio x1.8, knee x0.2, and a soft clip after the gain stage: the slammed sound |
| OPTO | x3 | x2.6 | ratio x0.8, knee at least 10 dB, RMS detection |

* **Auto gain** sets makeup from the threshold and the ratio.
* **Auto release** compares a 40 ms and a 600 ms envelope; the further apart
  they are, the more transient the material, and the release is set between
  70 and 520 ms from that.
* **Placement**: PRE controls what the engine is fed; POST controls what it
  produced.
* **Mix**: parallel blend.

---

## EqSection

`Source/DSP/EqSection.h`

Seven bands, each any of six shapes (high pass, low shelf, bell, high shelf,
low pass, notch). High and low pass bands have SLOPE: 12, 24, 36 or 48 dB per
octave, built as Butterworth cascades, with Q shaping the resonance of the
sharpest section.

DYNAMIC makes a band's gain follow the energy in its own region, recomputed
every 16 samples, so a band set to cut 6 dB at 350 Hz only cuts while the vocal
is pushing 350 Hz. It is a real dynamic EQ, not a crossfade between a filtered
and an unfiltered copy, which would comb filter.

The display reads an atomic snapshot of the settings the audio thread is using,
including the macro offsets, so the picture cannot drift from the sound.

---

## DeEsser

`Source/DSP/DeEsser.h`

Split band. An LR4 crossover at three quarters of FREQ splits the vocal; a
high pass at 0.8 x FREQ feeds the detector, and the whole band above the split
is pulled down, which is where an S actually lives. The gain is smoothed over
2 ms and RANGE is a hard limit on how far it will go. LISTEN outputs the band on
its own so it can be aimed.

---

## OutputStage

`Source/DSP/OutputStage.h`

* **DC blocker**, first and not optional.
* **Width and side level**, M/S.
* **Mono compatibility**: a high pass on the side only, moving from 20 to
  320 Hz, so the low end narrows progressively.
* **Air**: a high shelf at 11 kHz.
* **Limiter**: 1.5 ms lookahead against CEILING. The gain has already come
  down by the time the peak arrives, so it limits rather than clips. TRUE PEAK
  also estimates the peaks between samples with a Catmull-Rom interpolator at
  three intermediate points and limits against those. The lookahead is a
  constant delay whether the limiter is on or off, so switching it never moves
  the audio in time.
* **Safety**: a hard clip at 0 dBFS, last, defeatable in the advanced panel.

---

## MacroEngine

`Source/DSP/MacroEngine.h`

CHARACTER and the six macros are not DSP parameters. Every block they are
applied on top of the panel values, so one macro moves several real
parameters. They *push* proportionally towards the end of each range:

```
push (v, +a) = v + (hi - v) * a       push (v, -a) = v - (v - lo) * a
```

so the panel knob underneath stays live over its whole travel whatever the
macros are doing, and moving a macro never overwrites what was dialled.

CHARACTER is centred at 50. BODY and COLOR are centred at 50; DAMAGE, DEPTH,
MOTION and SPACE are neutral at 0, so a fresh instance does exactly what its
panel says. What they reach:

| Macro | Reaches |
| --- | --- |
| CHARACTER | the main controls of whichever engine is running, and the compressor threshold |
| BODY | low shelf and low mid on the EQ, saturation warmth, thickness and density, distortion body, underwater pressure, compressor threshold, transient body |
| COLOR | the EQ high band, underwater murk, distortion bite and smooth, saturation tone and warmth |
| DAMAGE | drive and crush, harmonics, underwater grit and pressure, compressor ratio |
| DEPTH | underwater depth and water, a darker top on the EQ, a reverb that is further back |
| MOTION | underwater ripple, wave and bubble; switches on modulation and takes its depth, rate, motion and blend |
| SPACE | underwater water, a reverb (created if the SPACE page is off), delay level, modulation width, stereo width |

---

## Oversampling

OFF / 2x / 4x / 8x, using JUCE's linear phase FIR equiripple half-band filters.
Their latency is a constant delay, so the dry path, the M/S side path and the
bypass path are all delayed by exactly the same amount and MIX never comb
filters. The reported latency is the oversampler's plus the limiter's 1.5 ms,
and it is updated whenever the setting changes.

Engines allocate for 8x once. When the setting changes, `applyEngineRate` tells
every module inside the oversampler its new rate and resets it, so every
frequency in them is right at every setting. The build checks that the engine's
tonal response differs by less than 1 dB between OFF and 8x on the linear
path.

---

## Real-time discipline

* No allocation in `processBlock`. Every buffer and delay line is sized in
  `prepareToPlay`.
* No locks and no system calls on the audio thread. Values reach the editor
  through `std::atomic`.
* `ScopedNoDenormals` for the whole block.
* Every user gain is smoothed; engine switches crossfade over 30 ms at equal
  power; FX and SPACE fade in and out rather than switching.
* POWER off and host bypass both pass the input through delayed by the reported
  latency, so bypassing never moves the vocal in time.
* A final pass zeroes anything not finite.
* All coefficients derive from the rate the module actually runs at, so
  44.1 kHz and 96 kHz sessions shape the vocal the same way; the build checks it.
