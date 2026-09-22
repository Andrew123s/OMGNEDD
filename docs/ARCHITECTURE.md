# Architecture

How the plugin is put together. [DSP.md](DSP.md) covers what the audio
processing does; this document covers the structure around it.

---

## One parameter table

`Source/Parameters.cpp` holds a single array of `ParamDesc` rows. Each row is an
id, a display name, a type, a range, a default, a skew centre, a unit, a choice
list and one sentence of help.

Everything else is generated from it:

* the `AudioProcessorValueTreeState` layout;
* the tooltip on every control;
* the right-click menu on every knob, including the type-in;
* the reset value for a double-click;
* the preset system's "reset to defaults" pass;
* the advanced panel's controls.

The consequence is that a control cannot exist in the interface without a real
parameter behind it, and a parameter cannot be added without a tooltip. The
verification target checks both directions, so this is enforced rather than
merely intended.

Parameter ids are stable strings (`uwDepth`, `dsDrive`, `compThresh`) and are
versioned with `juce::ParameterID { id, 1 }`. Renaming a display name does not
break a host automation lane; changing an id would, so ids are treated as part
of the file format and are never changed. New choices are appended to the end of
a choice list for the same reason: adding SHAPER, BITCRUSH and ASYM to the
distortion algorithms left the existing eight at the indices they already had.

### The groups

Parameters are laid out in the table in the order a user would meet them:
global, output stage, underwater, distortion, saturation, compressor, compressor
detector and sidechain, transient, modulation, multiband, dry/wet, de-esser,
macros, signature switches, DSP and routing, then the seven EQ bands generated
by a macro.

---

## The processor

`Source/PluginProcessor.cpp`.

`processBlock` does five things in order: read the parameters, run the chain,
meter, and guard the output. The parameter read is a single `pullParameters()`
call at the top of the block, which also applies the macro offsets. Nothing
downstream reads the APVTS, so the whole block is processed against one coherent
set of values.

### Buses

A stereo main input, a stereo main output and an optional stereo sidechain
input. `isBusesLayoutSupported` accepts mono or stereo on the main pair provided
they match, and accepts the sidechain disabled, mono or stereo. FL Studio will
offer the sidechain once the plugin is loaded; until something is routed to it,
`isSidechainConnected()` reports false and the advanced panel says so.

### Placement

The compressor is instantiated once and called from one of two points in the
block, chosen by the PLACEMENT parameter. This is the module reordering the
specification asks for, done where it is actually useful: the other modules have
a musically correct order, and making them reorderable would add a great deal of
state for no benefit.

### Real-time discipline

Covered in [DSP.md](DSP.md#real-time-discipline). The short version: nothing in
`processBlock` allocates, takes a lock, or makes a system call, and every buffer
it uses was sized in `prepareToPlay` at the largest case it can be asked for.

---

## State

`Source/StateManager.*` sits on top of the APVTS and provides:

* **A / B** — two complete states, with copy in both directions. Switching slots
  writes the whole tree, so a comparison is exact rather than partial.
* **Undo / redo** — a bounded history of complete states, pushed on a change and
  coalesced so that dragging a knob is one step rather than two hundred.

`getStateInformation` writes the APVTS tree as XML. That tree also carries the
editor size and the RANDOM module locks as properties, so reopening a project
restores the window the user left and the locks they set.

---

## Presets

`Source/Presets/`.

A factory preset is a name, a category and a settings string. Loading one resets
every parameter to its default and then applies the string, so a preset is a
complete state rather than a difference from whatever was loaded before. Values
are written through the APVTS with `setValueNotifyingHost`, so the host sees the
change and any automation lane stays valid.

User presets are `.omgnedd` files in the user's application data directory.
Favourites are a plain text file beside them. Both are found by a rescan, so a
preset dropped into the folder by hand appears without a restart.

See [PRESETS.md](PRESETS.md).

---

## Randomisation

`Source/Randomizer.h`.

RANDOM draws one "attitude" figure first — how far the patch is allowed to go —
and then draws every parameter from a range that follows from it, keeping by
hand the relationships a person would keep in their head. Locks are per module
and live as properties on the APVTS tree, so they persist with the session.

The detail, and the list of relationships it holds, is in
[PRESETS.md](PRESETS.md#random).

---

## The editor

`Source/PluginEditor.cpp` and `Source/UI/`.

The whole interface lives inside one `content` component which is scaled by an
`AffineTransform`. The layout is therefore written once in design units and
scaled as a piece, so nothing is stretched, no font is distorted, and the 900 px
window is the 1200 px window at 75 % rather than a different design.

The layout drops secondary things first as the window shrinks:

| Below | What goes |
| --- | --- |
| 1150 px wide | the de-esser module (still in the advanced panel) |
| a short lower strip | the compressor's second row (still in the advanced panel) |
| 560 px high | the lower strip entirely |

Nothing becomes unreachable, because everything dropped is also in the advanced
panel, and the advanced panel scrolls.

### Components

* `Theme.*` — colour, type, spacing and the drawing primitives: brushed metal,
  recesses, screws, hairlines.
* `OmgLookAndFeel.*` — menus, alerts and the three-line parameter tooltip.
* `OmgKnob.*` — the rotary control, with the full gesture set and the
  right-click menu generated from the parameter table.
* `OmgButton.*`, `OmgSwitch.h`, `OmgLED.h` — the push button, the lever switch,
  the lens.
* `OmgMeter.*` — the segmented level meter with peak hold and clip lamp, the
  gain-reduction bar and the drive meter.
* `ModeSelector.*` — `OmgSelector` (a row or grid of buttons driving one choice
  parameter) and `OmgChoiceButton` (the same contract in one button, for dense
  rows).
* `EqGraphComponent.*` — the EQ curve and the analyser behind it.
* `OmgPanel.h` — the module chassis every section sits in.

### The control registry

Every control records the parameter it drives in `omg::ControlRegistry` as it is
constructed. The verification target builds the editor, walks the registry and
checks that every id is a real parameter and that every parameter appears. This
is what makes "no decorative controls, no fake meters" a build failure rather
than a promise.

### Metering

Meters read `std::atomic` values written by the audio thread and redraw on a
timer. Nothing is simulated: the input and output meters carry measured peak and
RMS with a 300 ms analogue fall and a latching clip lamp, the gain-reduction
meters carry the figure the compressor and the de-esser actually applied, and
the drive meter carries the level measured at the point the character engine is
handed the signal.

---

## Host integration

* Name `OMGNEDD`, manufacturer `Yvngboi Nedd`, codes `Ynde` / `Omgn`.
* VST3 categories: Fx, Distortion, Dynamics, EQ.
* Latency reported through `setLatencySamples` whenever the oversampling setting
  changes, and shown live in the advanced panel.
* POWER is a real parameter, so a host that automates it gets the same result as
  a click; the panel dims and audio passes through untouched, which the
  verification target checks sample by sample.
* FL Studio's own bypass is handled by the host and is separate from POWER.
* Offline rendering works because nothing in the chain depends on wall-clock
  time: every modulator advances per sample.
