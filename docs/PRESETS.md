# Presets

## How a preset works

A factory preset is a name, a category and a settings string:

```cpp
{ "Deep Dive", "UNDERWATER",
  "engine=0;character=58;uwDepth=62;uwWater=70;uwMurk=45;..." }
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

54 presets in eight categories. Names describe the sound, not an artist.

### UNDERGROUND

The underground and rage vocal: sunk into a dark resonant filter, warbling,
squashed, and sitting in a wash that blooms between the words. WATER brings the
wash (a dark ducked reverb and a ping-pong 1/8 dotted delay) with it, so none
of these need the SPACE page switched on.

| Preset | What it is for |
| --- | --- |
| **Rage Sub** | The reference. Half sunk, pumping, an octave-down layer at 18 %, aggressive compression, a little DAMAGE grit. |
| **Vamp Haze** | Darker and wider: more water and murk, a slow chorus, 140 % width. The hazy, sleepy lead. |
| **Night Swim** | Deep and moving: wave on a triangle, bubble up, and a slow free-running phaser over the top. |
| **Baby Voice** | The pinched high voice: a +5 semitone layer at 78 % over a lightly sunk vocal, de-esser on. |
| **Demon Layer** | An octave-down layer at 46 % under a heavy, murky, pressured vocal. |
| **Blown Out Water** | Pressure at 80 with fast response and DAMAGE at 75: sunk and blown out at once, 4x oversampled. |
| **Sunk Adlibs** | Water at 90, strong ripple, 155 % width, a low layer. For the ad-libs behind the lead. |
| **Warble Lead** | Ripple at 78 plus tape wow with MOTION: the pitch never settles. Lightly sunk so the words stay clear. |

### UNDERWATER

| Preset | What it is for |
| --- | --- |
| **Deep Dive** | The reference underwater sound. Deep, dark, resonant, vocal compressor. |
| **Submerged** | Less far down, engine mix at 90 %, so the words stay clear. |
| **Ocean Voice** | Wide and moving: strong wave, 150 degree modulator offset, chorus, 140 % width. |
| **Drowned Radio** | Submerged and band limited: a 36 dB high pass at 320 Hz, a mid lift, and tape flutter. |
| **Dark Water** | Heavy murk, little movement, COLOR pulled dark, BODY up. Still and cold. |
| **Pressure** | The pressure stage doing the work: fast response, punch compressor, PUNCH on. |
| **Abyss** | Everything at the bottom: 48 dB slope, high resonance, an octave-down layer, DEPTH macro at 50. |
| **Liquid Vocal** | Lighter and fluid: bubble at 55, triangle wave, micro pitch detune. |

### WOBBLE

The moving filters. The synced ones lock to the host tempo, so they land on the
beat.

| Preset | What it is for |
| --- | --- |
| **Auto Wah Funk** | Envelope wah: the filter opens as the vocal gets louder. |
| **Wobble 1/4** | A resonant 24 dB wobble on quarter notes, some drive into it. |
| **Wobble 1/8 Grit** | Eighth-note wobble after light soft-clip distortion, with more drive. |
| **Triplet Wub** | Eighth-note triplets on a triangle, very resonant, a little stereo offset. |
| **Square Chop** | Sixteenth-note square wave: the filter jumps rather than sweeps. |
| **LFO Wah Sweep** | A slow wah sweep on half notes. |
| **Wah Wah Adlib** | Eighth-note wah with a stereo offset and a ducked ping-pong delay behind it. |
| **Talkbox Yoi** | The TALK mode sweeping through the vowels on quarter notes: the yoi-yoi effect. |
| **Robot Talk** | TALK on sixteenths with sample-and-hold, so the vowels jump at random, over bit-crushed distortion. |
| **Phaser Drift** | A slow free-running phaser with a short, damped reverb. |
| **Underwater Wobble** | A sunk vocal with a quarter-note wobble on top. |

### DISTORTION

| Preset | What it is for |
| --- | --- |
| **Destroyed** | Fuzz at 80 % drive with heavy crush, 8x oversampling, post filter at 7 kHz. |
| **Radio Damage** | The digital algorithm, band limited top and bottom with 36 dB slopes. A phone, not a destroyed speaker. |
| **Aggressive** | Hard clip, aggressive compressor, transient attack lifted. For a rap hook. |
| **Digital Rage** | BITCRUSH as the algorithm plus CRUSH at 75 %, chaos on, 8x. |
| **Broken Speaker** | Fold with asymmetry and negative bias, so the cone is torn on one side. |
| **Dirty Vocal** | ASYM at moderate drive, de-esser working, **62 % mix**. The one to reach for first. |
| **Fuzz Voice** | Fuzz with multiband: low band pulled back 6 dB, mid pushed 5. |
| **Industrial** | SHAPER at high drive with flutter and chaos, 8x. |

### SATURATION

| Preset | What it is for |
| --- | --- |
| **Warm Tape** | The reference. Tape model, smooth compressor, tone tilted dark. |
| **Analog Vocal** | Tube model, vocal compressor, de-esser. A mix-ready lead. |
| **Tube Glow** | Harmonics at 64, air on, a presence lift. Forward and bright. |
| **Vintage** | Transformer model, tone well dark, opto compressor, a cut at the top. |
| **Thick Vocal** | Thickness at 76 with the BODY macro at 70 and transient body lifted. |
| **Console** | Console model, light drive, clean compressor. Glue rather than colour. |
| **Warm Presence** | Warm model, tone tilted bright, air on, de-esser up to catch what that costs. |
| **Final Polish** | The CLEAN model, auto release, ceiling at -0.5 dB. The last thing in the chain. |

### VOCAL

| Preset | What it is for |
| --- | --- |
| **Init** | Neutral: the underwater engine with its mix at zero, so the vocal passes through and you build from nothing. |
| **Lead Vocal Polish** | Light saturation, vocal compressor with auto release, de-esser, air, a short ducked reverb. |
| **Close And Dry** | Punch compressor, mud cut, transient attack up and body down. Intimate. |
| **Ad-Lib Doubler** | Micro pitch detune, 150 % width, an eighth-note ping-pong delay. A double without a second take. |
| **Trap Ad-Lib** | Tube distortion, aggressive compression, wide, a dotted-eighth delay, **85 % mix**. |
| **Clean Space** | A clean vocal in a ducked reverb and a quarter-note delay. |

### HYBRID and EXTREME

| Preset | What it is for |
| --- | --- |
| **Half Sunk Grit** | Underwater at 75 % engine mix with DAMAGE at 45 and multiband pushing the mids. |
| **Wide Whisper** | Warm saturation, SPACE macro at 60, chorus, 140 % width. |
| **Drowned Machine** | Deep underwater with random-shape modulation, flutter, chaos and MOTION. |
| **Parallel Damage** | Tape distortion at 82 % drive, compressor **POST** the engine, **46 % mix**. A parallel texture. |
| **Total Collapse** | Everything at once: fuzz, crush, chaos, 8x, multiband at +/-8 dB, a sixteenth-note wobble. |

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
*attitude* figure first, how far this patch is allowed to go, between 0.2 and
0.95, and then draws everything else from a range that follows from it, holding
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
  blend is always subtle, capped at 65 %. An octave-down layer turns up under
  about a third of underwater patches and rarely elsewhere.
* **The moving filters are a statement**, so FX is on in about a third of
  draws, synced to a musical division most of the time.
* **Space follows the engine too.** Underwater brings its own wash through
  WATER, so it gets the SPACE reverb a quarter of the time; the other engines
  get it more often than not. Delays are always synced and ducked.
* **Macros stay near neutral.** DAMAGE, DEPTH and MOTION are drawn low and
  scaled by the attitude, so a random patch is not buried under its macros.
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
| MOD | the pitch and time modulation section, and the pitch layer |
| MACROS | the six vocal macros and the three signature switches |
| OUTPUT | width and the final mix |
| FX | the moving filters: wah, wobble, talk, phaser |
| SPACE | the reverb and the delay |

So: dial an EQ you like, lock it, and roll the character engine underneath it
until something lands. The menu also has *Unlock everything* and *Draw a new
patch now*.

Locks live as properties on the plugin's state tree, so they are saved with the
session and come back when the project reopens.
