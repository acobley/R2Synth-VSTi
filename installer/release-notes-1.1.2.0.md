# R2Synth 1.1.2.0

A monophonic analogue-style synthesiser for macOS, as a **VST3** and an
**Audio Unit**. Free software under the GNU GPL, version 3 or later.

## What changed since 1.1.1.0

- **No evaluation and no activation code.** R2Synth now works in full with
  no time limit: there is no reminder, no "Evaluation" line on the panel and
  nothing to activate. The installer's last page says so too.

Sound, parameters, the twelve factory presets and saved projects are exactly
as in 1.1.1.0.

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

Download `R2Synth-1.1.2.0.pkg` and open it. The installer offers each format
separately:

    /Library/Audio/Plug-Ins/VST3/R2Synth.vst3
    /Library/Audio/Plug-Ins/Components/R2Synth.component
    /Library/Audio/Presets/AE Cobley/R2Synth        (the factory presets)

Quit your DAW first - a running host holds the old copy open. Signed with a
Developer ID and notarised by Apple, so it installs without any
Privacy & Security detour.

Requires macOS 10.13 or later; runs natively on Apple silicon and Intel Macs.

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

## Checksum

```
SHA-256: 7ec978f3ffbe8f893511bed4c5e1bbfea96e6d0c81805d42821543ce3d3f63ce
```

---

Copyright 2026 A. E. Cobley. GNU GPL v3 or later; see `LICENSE` and `NOTICE.md`.
VST is a registered trademark of Steinberg Media Technologies GmbH.
