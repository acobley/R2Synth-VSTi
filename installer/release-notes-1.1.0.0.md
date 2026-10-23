# R2Synth 1.1.0.0

A monophonic analogue-style synthesiser for macOS, as a **VST3** and an
**Audio Unit**. Free software under the GNU GPL, version 3 or later.

## What's new in 1.1

A new panel. The sound, the parameters and saved sessions are unchanged:
a project made with 1.0 opens and plays exactly as before.

- **A black hardware panel**, in the spirit of a 1980s Soviet synth: matte
  black, white printing, boxed sections, black knurled knobs with printed
  scales.
- **Every switch is a rotary selector** - a chicken-head knob with its
  positions printed round it. Click a position's name to choose it, click
  the knob to step round, or use the mouse wheel. The filter's type
  selector now sits beside the filter display.
- **Small colour OLED-style displays** for the filter response, both
  envelopes and the LFO.
- **All the lettering in a 70s sci-fi face**, drawn as shapes rather than a
  font, so it is sharp at any size.
- **A lived-in finish**: dust, fine scratches, knob tops polished and marked
  by turning (the marks turn with the knob), and dust and fingerprints on
  the displays' glass. Light enough that every label stays readable.
- **Twelve factory presets**: Init, Fat Bass, Acid Squelch, Sharp Lead,
  Slow Pad, Wobble Bass, Brass, Pluck, Sub Bass, MS-20 Scream, Siren and
  Tremolo Organ, for both the VST3 and the Audio Unit. They are an optional
  part of the installer, in `/Library/Audio/Presets/AE Cobley/R2Synth`,
  and show in your host's preset list.

## What it is

- **Three VCOs**, each sawtooth or square, each with its own Tune (±12
  semitones) and Level, band-limited (polyBLEP) against aliasing.
- **LFO 1**, after the Korg MS-20's modulation generator: one Shape knob
  slides from a falling sawtooth through a triangle to a rising one, with
  Square and Sine outputs that follow the same knob. Depths on pitch, cutoff,
  resonance and the VCA.
- **LFO 2**, a sine that turns LFO 1's Shape knob for you.
- **A switchable filter**: a Moog-style 4-pole ladder, or either of the two
  MS-20 filters (the early Korg35 with its diodes in the signal path, and the
  later OTA design with diodes in the feedback). Switching cross-fades, so it
  can be done while a note plays.
- **Two ADSRs** (filter and amplifier), Key Track, envelope amount,
  **portamento**, and a live display of the filter's response.

## Installing

Download `R2Synth-1.1.0.0.pkg` and open it. The installer offers each format
separately:

    /Library/Audio/Plug-Ins/VST3/R2Synth.vst3
    /Library/Audio/Plug-Ins/Components/R2Synth.component
    /Library/Audio/Presets/AE Cobley/R2Synth        (the factory presets)

Quit your DAW first - a running host holds the old copy open. Signed with a
Developer ID and notarised by Apple, so it installs without any
Privacy & Security detour.

Requires macOS 10.13 or later; runs natively on Apple silicon and Intel Macs.

## Evaluation and activation

R2Synth works in full from the start. After a **7-day evaluation** it shows a
reminder now and then. A **free activation code** stops it: click the
**Evaluation** line at the top of the panel, then **Get activation code**,
and paste the name and code the page gives you. The code is yours: it works
on any computer you use, so please don't share it.

The source is free software, so you are free to build R2Synth yourself; the
activation code is a way of supporting the official signed build.

## Things that will look like bugs and are not

* **It is monophonic.** Last note wins, every new note retriggers both
  envelopes, and releasing a key returns to the last one still held - the
  MS-20's keyboard behaviour, chosen deliberately.
* **Velocity is ignored**, again like the MS-20.

## Known gaps

* **No pitch bend or mod wheel.** Neither is mapped to a parameter yet.

## Uninstalling

A `.pkg` never will, so:

```sh
sudo rm -rf /Library/Audio/Plug-Ins/VST3/R2Synth.vst3
sudo rm -rf /Library/Audio/Plug-Ins/Components/R2Synth.component
sudo rm -rf "/Library/Audio/Presets/AE Cobley/R2Synth"
```

Your activation code lives in
`~/Library/Application Support/R2Project/licence.json`, shared with other
R2Project plug-ins; leave it if you might reinstall.

## Checksum

```
SHA-256: <paste from: shasum -a 256 installer/R2Synth-1.1.0.0.pkg>
```

---

Copyright 2026 A. E. Cobley. GNU GPL v3 or later; see `LICENSE` and `NOTICE.md`.
VST is a registered trademark of Steinberg Media Technologies GmbH.
