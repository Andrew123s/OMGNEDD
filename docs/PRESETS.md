# Presets

## How a preset works

A factory preset is a name, a category and a settings string:

```cpp
{ "Deep Dive", "UNDERWATER",
  "engine=0;character=72;uwDepth=70;uwWater=66;uwMurk=48;..." }
```

Loading one resets **every** parameter to its default and then applies the
string. A preset is therefore a complete state, not a difference from whatever
happened to be loaded before, and loading the same preset twice from different
starting points gives the same sound both times.

Values are written through the APVTS with `setValueNotifyingHost`, so the host
sees the change and any automation lane pointing at those parameters stays
valid.

Every preset in the bank is checked by the build: it is loaded, fed a
vocal-like signal, and must stay finite and inside the safety ceiling. See
[TESTING.md](TESTING.md#the-factory-bank).

---

## The factory bank

### UNDERWATER

| Preset | What it is for |
| --- | --- |
| **Deep Dive** | The reference underwater sound. Deep, dark, slow tape wow underneath. |
| **Submerged** | Half as far down, engine mix at 85 %, so the words stay clear. |
| **Ocean Voice** | Wide and moving: strong wave and ripple, 140° modulator offset, chorus over the top. |
| **Drowned Radio** | Band-limited as well as submerged — an HPF at 260 Hz and an LPF at 3.4 kHz, with flutter. |
| **Dark Water** | Heavy murk, almost no movement, colour pulled dark. Still and cold. |
| **Pressure** | The pressure stage doing the work: fast response, hard compression, transient attack lifted. |
| **Abyss** | Everything at the bottom of the range, 4x oversampling, deep slow wow. |
| **Liquid Vocal** | Lighter and more fluid: bubble and ripple up, micro pitch detune, 88 % mix. |

### DISTORTION

| Preset | What it is for |
| --- | --- |
| **Destroyed** | Fuzz at 88 % drive with heavy crush, 8x oversampling, post filter down at 6.5 kHz. |
| **Radio Damage** | The digital algorithm, band-limited top and bottom. A phone, not a destroyed speaker. |
| **Aggressive** | Hard clip, aggressive compressor, transient attack lifted. For a rap hook. |
| **Digital Rage** | BITCRUSH as the algorithm plus CRUSH at 82 %, chaos on, 8x. |
| **Broken Speaker** | Fold with heavy asymmetry and negative bias, so the cone is torn on one side. |
| **Dirty Vocal** | ASYM at moderate drive, de-esser working, **62 % mix**. The one to reach for first. |
| **Fuzz Voice** | Fuzz with multiband: low band pulled back 6 dB, mid pushed 4. |
| **Industrial** | SHAPER at high drive with flutter and chaos, 8x. |

### SATURATION

| Preset | What it is for |
| --- | --- |
| **Warm Tape** | The reference. Tape model, smooth compressor, tone tilted dark. |
| **Analog Vocal** | Tube model, vocal compressor, de-esser. A mix-ready lead. |
| **Tube Glow** | Harmonics at 64, air on, a lift at 3.5 kHz. Forward and bright. |
| **Vintage** | Transformer model, tone well dark, opto compressor, a cut at the top. |
| **Thick Vocal** | Thickness at 76 with the body macro at 72 and transient body lifted. |
| **Console** | Console model, light drive, clean compressor. Glue rather than colour. |
| **Warm Presence** | Warm model, tone tilted bright, air on, de-esser up to catch what that costs. |
| **Final Polish** | The CLEAN model, auto release, limiter at −0.5 dB. The last thing in the chain. |

### VOCAL

| Preset | What it is for |
| --- | --- |
| **Lead Vocal Polish** | Light saturation, vocal compressor with auto release, de-esser, air, a touch of attack. |
| **Close And Dry** | Punch compressor, mud cut at 420 Hz, transient attack up and body down. Intimate. |
| **Ad-Lib Doubler** | Micro pitch at 58 % detune, width 148 %. A double without a second take. |
| **Trap Ad-Lib** | Tube distortion, aggressive compression, wide, **78 % mix**. |

### HYBRID and EXTREME

| Preset | What it is for |
| --- | --- |
| **Half Sunk Grit** | Underwater at 70 % engine mix with the damage macro at 45 and multiband pushing the mids. |
| **Wide Whisper** | Warm saturation, space macro at 70, chorus, 140 % width. |
| **Drowned Machine** | Deep underwater with random-shape modulation, flutter, chaos, motion macro at 75. |
| **Parallel Damage** | Tape distortion at 86 % drive, compressor **POST** the engine, **46 % mix**. The parallel texture the specification asks for. |
| **Underwater Tube** | Underwater with the tube saturation model, multiband scooping the mids, body lifted. |
| **Total Collapse** | Everything at once: fuzz, crush, chaos, 8x, multiband at ±8 dB. |

### Init

Resets to a neutral state with the underwater engine selected and its mix at
zero, so the plugin passes the vocal through and you can build from nothing.

---

## User presets

**SETTINGS → Open user preset folder** opens the directory:

| | |
| --- | --- |
| Windows | `%APPDATA%\OMGNEDD\Presets\` |
| macOS | `~/Library/Application Support/OMGNEDD/Presets/` |
| Linux | `~/.config/OMGNEDD/Presets/` |

User presets are `.omgnedd` files there. They are found by a rescan, so a preset
someone sends you can be dropped into the folder and will appear without a
restart. Favourites are a plain text file beside the folder, listing preset
names one per line.

The preset browser filters by **Favourites**, **Factory**, **User** and
**Recently used**.

---

## RANDOM

The specification is explicit that RANDOM must not be a loop over every
parameter with a uniform draw. It is not. `Source/Randomizer.h` draws one
*attitude* figure first — how far this patch is allowed to go, between 0.2 and
0.95 — and then draws everything else from a range that follows from it, holding
by hand the relationships a person would keep in their head:

* **DRIVE and CRUSH move together, but CRUSH lags.** Crush only starts once the
  attitude passes 0.45, so a heavy patch is dirty before it is broken.
* **SMOOTH and the post filter come down as DRIVE goes up.** This is the single
  thing that stops a random distortion patch from being unusable on a vocal.
* **MIX comes down as the character goes up.** An extreme patch lands as a
  parallel texture rather than a wall.
* **DEPTH, WATER and MURK share one "sink" figure**, so the underwater patches
  are coherently deep rather than deep in one control and dry in another.
* **The compressor threshold and ratio are drawn against each other**, so the
  pair never adds up to thirty decibels of reduction.
* **The EQ is nudged, not replaced.** Small moves around whatever is already
  there, and only on the five bands a vocal engineer would actually reach for.
* **Modulation follows the engine.** Underwater gets movement 85 % of the time
  and only chorus or wow; the other engines get it less than half the time. The
  blend is always subtle, capped at 65 %.
* **Frequencies, slopes, oversampling, the limiter and the safety clip are never
  touched.** A random patch is still a safe one.

The build draws 250 patches from a fixed seed sequence and checks that every one
of them stays finite and inside the ceiling.

### Locks

**LOCK** in the top bar opens the module list. A ticked module is left exactly
as it is by the next draw:

| Module | What it covers |
| --- | --- |
| ENGINE | the engine choice and all of its controls, plus CHARACTER |
| EQ | the seven bands |
| DYNAMICS | compressor, de-esser and the transient shaper |
| MOD | the pitch and time modulation section |
| MACROS | the six vocal macros and the three signature switches |
| OUTPUT | width and the final mix |

So: dial an EQ you like, lock it, and roll the character engine underneath it
until something lands. The menu also has *Unlock everything* and *Draw a new
patch now*.

Locks live as properties on the plugin's state tree, so they are saved with the
session and come back when the project reopens.
