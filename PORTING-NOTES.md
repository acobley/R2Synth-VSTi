# SimpleSynth — project notes

A monophonic VST3 + AUv2 synthesiser, written from scratch on the
`../vst3-port-template` scaffold. This file is the record that outlives the
sessions that wrote it.

| Date | What |
|---|---|
| 29 Sep 2026 | Blank scaffold, as **SimpleDrum** (`SiDr`) |
| 29 Sep 2026 | Renamed **SimpleSynth** (`SiSy`, `kInstrumentSynth`, `audio.simplesynth.*`) before any build existed. Class UIDs kept — never shipped, still unique |
| 29 Sep 2026 | The engine and the panel — everything below |

## The decisions

| Decision | Chosen | Why |
|---|---|---|
| Effect or instrument | **Instrument** — `PlugType::kInstrumentSynth`, AU `aumu`, event input (16 ch) | It is a synthesiser |
| Bus layout | **0 in / 2 out** | Said identically in `initialize`, `setBusArrangements` and the single `SupportedNumChannels` entry in `resource/au-info.plist` |
| Name and codes | `SimpleSynth` / `SiSy` / `AECo` | `SiSy` checked against FDrm, FTrn, Prj6, SmDr, SDub, SpyB, VcFl — no clash |
| Voices | **Mono, last-note priority**; every new note retriggers both envelopes; releasing the sounding key returns to the last one still held, *without* retriggering | Chosen by Andy — MS-20-like |
| LFO destinations | **Pitch, cutoff and VCA**, each with its own depth | Chosen by Andy |
| Tune | **Per VCO, continuous, ±12 semitones** | Chosen by Andy |
| Panel style | **FilterDrum's** ("the KorgDrum"): its control set lifted, its palette, group boxes and envelope display | The brief |
| Velocity | **Ignored**, like the MS-20's keyboard | Not asked for. `noteOn` still receives it, so adding a sensitivity control is a DSP change only |

## Permanent identity — never change once a build ships

| What | Value |
|---|---|
| Processor UID | `0x6D69A51B, 0xD84BE44A, 0x4927F783, 0x18477FAD` |
| Controller UID | `0x935E5313, 0x9EF72A9E, 0xA1A91CCB, 0x968140F1` |
| AU type / subtype / manufacturer | `aumu` / `SiSy` / `AECo` |
| VST3 bundle id | `audio.simplesynth.vst3` |
| AU bundle id | `audio.simplesynth.audiounit` |
| Vendor | `AE Cobley` (no full stops — REAPER cuts AU names at the first one); AU name `AE Cobley: SimpleSynth` |

**The parameter ids are now permanent too.** They were laid out in one go
before any build existed (the scaffold's Output Trim, id 0, became Output
Level); from the first build on, append only.

## The signal path

```
VCO 1 --\                                          LFO (MS-20 MG)
VCO 2 ---+-- mixer --> Moog ladder VCF --> VCA --> Output Level
VCO 3 --/               ^  ^  ^            ^  ^
                        |  |  key track    |  LFO (tremolo)
                        |  LFO             VCA ENV (ADSR)
                        VCF ENV (ADSR)
```

28 parameters plus bypass, all automatable:

| Section | Controls |
|---|---|
| VCO 1, 2, 3 | Wave (Saw / Square), Tune (±12 st), Level (0–100 %) — one implementation, addressed by stride (`vcoParam()`) |
| LFO | Rate (0.1–20 Hz, log), Shape (−100 falling saw … 0 triangle … +100 rising saw), Output (Shape / Square), Pitch depth (0–12 st) |
| VCF | Cutoff (20 Hz–20 kHz, log), Resonance (0–100 % → loop gain 0–1.1), Key Track (0–100 %), LFO depth (0–4 oct) |
| VCF ENV | Attack, Decay, Sustain, Release, Env Amount (±6 oct) |
| VCA / OUTPUT | LFO depth (tremolo, 0–100 %), Output Level (−48…+6 dB) |
| VCA ENV | Attack, Decay, Sustain, Release |

Times are 0.5 ms (attack) / 1 ms (decay, release) to 10 s, log.

### What each block is, and why

- **VCOs**: polyBLEP sawtooth and 50 % square. Free-running — not reset on a
  note — so detuned VCOs beat rather than starting identically. The three
  start a third of a cycle apart after a reset. Tune is smoothed (a pitch
  step through a resonant filter is a click).
- **The MS-20 LFO**: one Shape knob slides the peak of a triangle across the
  cycle — falling saw at one end, triangle in the middle, rising saw at the
  other. The square output's duty **follows the same knob** (50 % at the
  triangle, clamped at 5 %/95 % so it never goes DC), as the MS-20's pulse
  output does. The Output switch picks which one drives all three
  destinations. Free-running, bipolar, and slewed by 0.5 ms so a saw reset
  or a square edge on the VCA is not a click. VCA depth is tremolo
  *downwards* from the envelope — it can never lift the level above it.
- **The filter is SpaceDub's Moog ladder** ("Moog VCF, variation 2",
  musicdsp.org), `DelayLine::process`'s per-sample arithmetic unchanged. At
  zero resonance it is **bit-identical** to SpaceDub's loop — a test holds it
  to that against a verbatim copy. Four deliberate differences, all in
  `SimpleSynthDsp.{h,cpp}`:
  1. **Tuning.** SpaceDub drives it with coefficient = Hz / (fs/4). Measured,
     that puts a 440 Hz request at **332 Hz** and runs sharp above fs/4, and
     differently at every sample rate. `ladderTuning()` builds a 256-point
     table from the filter's own transfer function (the frequency where the
     loop's phase is −180°) so the resonant peak lands on the Cutoff: worst
     error **1.1 cents** at 44.1/48/96 k, measured.
  2. **Resonance as loop gain.** The self-oscillation threshold in
     SpaceDub's terms moves with cutoff (a feedback of 4.01 at 180 Hz, 4.29
     at 12.7 kHz). The same table stores the feedback at loop gain 1, so the
     knob is loop gain: **1.0 sings at every cutoff**, the top ninth of the
     knob self-oscillates, and the panel's lamp lights from the same
     `selfOscillating()` predicate.
  3. **`limitFeedback()`** on the feedback term: linear to 1.5 (so a quiet
     signal sees SpaceDub's filter to the bit — also tested), bending to 3.0.
     It gives self-oscillation an amplitude (≈ −3 dBFS before the VCA);
     SpaceDub's only bound was clamping the state at ±16, which above loop
     gain 1 would be +24 dBFS of sine.
  4. **The state floor is 1e-30, not 1e-15.** At a 30 Hz cutoff on a 192 k
     engine the first pole runs at ~1e-15 of the input and SpaceDub's floor
     zeroed it — the filter could not self-oscillate there. Tested both ways.
  
  Plus a **−100 dBFS noise floor** into the ladder (SpaceDub injected noise
  too): a digital filter started from exact zero never sings, so with every
  VCO off full resonance was silent. The Moog's bass loss at high resonance
  (DC gain 1/(1+fb)) is kept — it is the sound.
- **The cutoff ceiling is 0.40 × fs** (17.6 kHz at 44.1 k, 19.2 kHz at 48 k),
  where the ladder's tuning is still honest; the panel prints it beside the
  engine rate.
- **ADSRs** are RC segments: the attack charges towards 1.3 and stops at 1.0;
  Decay and Release are the time to **−60 dB** of the distance left; the
  envelope runs on to −100 dB before going idle, which takes 5/3 of the
  knob (`releaseTailSeconds()`, which `getTailSamples` reports). Sustain is
  tracked while the key is held. **Retrigger starts from the current level** —
  on a mono synth that retriggers every note, a reset to zero would click on
  every legato phrase.

## The panel

![The panel](docs/panel.png)

Four rows in signal order — VCOs, LFO, VCF + VCF ENV, VCA/OUTPUT + VCA ENV —
with an LFO display (both outputs, the patched one marked) and an envelope
display (both ADSRs on one time axis, key-up marked with dots) in a strip on
the right. 1176 × 318.

- **`SimpleSynthControls.*` is FilterDrum's control set**, namespace renamed,
  plus two opt-in additions to `SpyEnvelopeView` (`setShowAmounts`,
  `setGateMarker`) — see its banner. `diff` it against FilterDrum's to see
  exactly that.
- **The layout is data** in `SimpleSynthLayout.h`, with no VSTGUI in it.
- **Every string that depends on a value** is made in
  `SimpleSynthReadouts.cpp`, also VSTGUI-free.
- Both, plus the DSP's trace functions, feed **`tools/dump-layout.cpp`**,
  whose JSON `tools/render-panel.py` draws into `docs/panel.png`. So the
  picture is drawn from the code the editor runs, not from a parsed copy.
- `SpySelector` draws no label of its own, so the two switches read
  "Wave: Saw" / "Output: Shape" (`selectorText()`).

## Bugs found by the Linux validator, and fixed

The SDK validator (see below) found two things a Mac build would have shown
only as odd host behaviour:

1. **Every Log parameter had the wrong default in the host.**
   `RangeParameter`'s constructor computes the default with `toNormalized()`,
   but a virtual call from a base-class constructor reaches the *base's*
   linear version — so LFO Rate's 4 Hz default was published as 0.196
   instead of 0.696 (a host "reset to default" would play 0.28 Hz). Fixed by
   setting `info.defaultNormalizedValue` again in `SimpleSynthParameter`.
   **FilterDrum has the same bug** — its controller never overrides this, and
   its Cutoff, Attack and Release rows are Log.
2. **Bypass would not parse its own "Off"** — the plain `addParameter`
   bypass FilterDrum uses. Now a `StringListParameter`.

## Traps handled

- **`kBypass` is 1000**, far past the table; `paramDef()` is the only place
  `kParams` is indexed and it range-checks. An instrument's bypass is silence.
- **Never a null title/units to `RangeParameter`.**
- **`setState` resets everything to defaults before reading**; processor and
  controller read the identical layout. The scaffold's one-parameter stream
  still loads (tested) — "append, never insert" at work.
- **Notes are sample-accurate**: `process()` sorts the block's events and
  renders the segments between them. A velocity-0 note-on is a note-off.
- **Parameter-only blocks** still apply their notes. **64-bit hosts** get the
  same samples as 32-bit (tested). The audio thread never allocates; an
  oversize block is refused.
- **Silence flags** are set only when bypassed or the VCA envelope is idle.
- **`getTailSamples`** = VCA release × 5/3 + 0.5 s.
- **The sample-rate message** goes processor → controller from `setActive`
  (UI thread), never from `process()`. The panel falls back to 44.1 k if a
  host never connects the two.
- **`editorDestroyed` compares upcast pointers** — `dynamic_cast` is null
  inside `~EditorView()`.

## Deliberate omissions

| Omitted | The trap when you add it |
|---|---|
| `getProcessContextRequirements` | The LFO is free-running in Hz. The moment anything reads tempo, add the override — code in the banner of `SimpleSynthProcessor.h` — or the tempo reads 120 in every host and the validator prints `- None` |
| Velocity, pitch bend, mod wheel | VST3 delivers bend and CCs only through `IMidiMapping` to parameters; none is mapped |
| Glide / legato envelopes | Every note retriggers, by decision |
| Sample-accurate automation | The last point of each queue is applied per block; the 20 ms smoothers hide the rest |

## Measured levels

From `tests/DspTests.cpp`, printed on every run:

| Case | Peak |
|---|---|
| Default patch, notes C2–C6, at 44.1 / 48 / 96 k | **−6.9 dBFS** |
| Worst case — three squares in unison, full resonance — at 0 dB Output Level | +0.1 dBFS (+6.1 at the +6 dB maximum) |
| Self-oscillation alone, all VCOs off | −5.7 dBFS |

`kMixerGain` (0.5) and `kVoiceGain` (0.9) were set from these numbers.

## Verification

Run on 29 Sep 2026 in the cloud workspace (Linux), against VST3 SDK
v3.8.1_build_84 — the tag the template pins:

| What | Result |
|---|---|
| `tools/run-tests.sh` — DSP suite | **184 checks, 0 failures** (x86-64 GCC 13, and arm64 GCC 11 in the Mac's VM) |
| `tools/run-tests.sh` — layout and panel text | **804 checks, 0 failures** |
| Every `source/*.cpp` compiled against the SDK and VSTGUI headers, `-Wall -Wextra` | 8/8, no warnings in our code; `nm -C`: all 57 referenced `SimpleSynth::` symbols defined |
| `tools/linux-validate/` — the plug-in built on Linux (editor stubbed), **SDK validator** | **47 tests passed, 0 failed** |
| `tests/ProcessorTests.cpp` against the real processor | **25 checks, 0 failures** |

Run again in the Mac's own Linux VM (aarch64, GCC 11), which found one
thing: the "quiet signal at resonance 2.5 is SpaceDub's filter to the bit"
check failed at `-O2`. On arm64 GCC fuses SpaceDub's `x -= out4 * fb` into
an FMA, and ours cannot be (it goes through `limitFeedback()`), so the
last bit differs. Codegen, not arithmetic — the suite is now built with
`-ffp-contract=off` and passes there too: **184 / 0 and 804 / 0**. The
plug-in itself is built with the compiler's default contraction; a
last-bit difference from SpaceDub is inaudible.

The negative controls fail when they should: half gain is not bit-identical;
a loud signal at high resonance is limited; with every note's offset forced
to 0 the processor test fails; with SpaceDub's 1e-15 floor the 30 Hz/192 k
self-oscillation test fails.

**Not verified:** the Mac build, VSTGUI actually drawing the panel (the
picture is a reconstruction from the same numbers), the AU wrapper,
`auval`, and listening.

```sh
cd ~/DXi-DEv/SimpleSynth-VSTi
tools/run-tests.sh                                   # needs external/vst3sdk for the layout suite
python3 tools/render-panel.py                        # redraws docs/panel.png
cmake -S tools/linux-validate -B /tmp/ss-linux -G Ninja \
      -DVST3_SDK_ROOT=$PWD/external/vst3sdk -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/ss-linux && /tmp/ss-linux/bin/Release/processortests   # Linux only
```

## SDK decision

**In-tree clone**: the first `./setup-xcode.sh` clones the VST3 SDK (~250 MB)
and Apple's AudioUnitSDK into `external/`, which is `.gitignore`d. Chosen so
the project is self-contained and can ship separately.

## Next steps on the Mac

```sh
cd ~/DXi-DEv/SimpleSynth-VSTi && ./setup-xcode.sh     # new source files: re-run it
# build, watch the validator's post-build output, then:
auval -v aumu SiSy AECo
```

New files were added since the last configure, so **re-run `./setup-xcode.sh`
before building** — otherwise the build compiles the old file list and fails
with *"Bundle does not export the required 'GetPluginFactory' function"* or
the source-manifest guard stops it.
