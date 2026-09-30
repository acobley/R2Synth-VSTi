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

![Signal and modulation routing](docs/routing.png)

`docs/routing.svg` is the source (hand-drawn; `routing.png` is a 2× render
of it). Redraw it when a route or a depth range changes.

```
VCO 1 --\                                          LFO (MS-20 MG)
VCO 2 ---+-- mixer --> Moog ladder VCF --> VCA --> Output Level
VCO 3 --/               ^  ^  ^            ^  ^
                        |  |  key track    |  LFO (tremolo)
                        |  LFO             VCA ENV (ADSR)
                        VCF ENV (ADSR)
```

30 parameters plus bypass, all automatable:

| Section | Controls |
|---|---|
| VCO 1, 2, 3 | Wave (Saw / Square), Tune (±12 st), Level (0–100 %) — one implementation, addressed by stride (`vcoParam()`) |
| LFO (1) | Rate (0.1–20 Hz, log), Shape (−100 falling saw … 0 triangle … +100 rising saw), Output (Shape / Square / Sine), Pitch depth (0–12 st) |
| LFO 2 → SHAPE | Rate (0.02–20 Hz, log, default 0.5), Level (0–100 %, default 0) — ids `kLfo2Rate`, `kLfo2Level`, appended after `kVcaRelease` |
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
  destinations.
- **The LFO's Sine output** (added 29 Sep 2026) is made the way the EMS
  VCS3's oscillators make their sine: the **symmetrical triangle core**
  through a differential-pair shaper, `tanh (kSineDrive · (tri + balance))`,
  rescaled to swing exactly −1…+1 so the depth knobs mean the same on every
  output (`lfoSineValue()`).
  - `kSineDrive = 1.40` was **measured**, not chosen: a 0.01-step sweep for
    the least THD. It gives **1.36 %** (the triangle is 12.1 %). The suite
    checks that 1.30 and 1.50 are both worse.
  - **Shape moves the shaper's balance**, as the VCS3's Shape control moves
    its sine off centre: ±100 % puts the triangle's trough (or peak) on the
    steep middle of the curve and the other extreme into saturation, so one
    half-cycle becomes a broad dome and the other a sharp V — a rectified
    sine, domed on top at +100 % and underneath at −100 %. The same knob
    therefore means *asymmetry* on all three outputs.
  - **Not modelled from a schematic.** I could not find the VCS3's sine
    shaper circuit, so this is the differential-pair principle plus the
    Shape-control behaviour, not a component-level copy of its transfer
    curve.
  - **The Output switch went from two positions to three before any
    release.** Square moved from normalised 1.0 to 0.5, so a test project
    saved on Square opens on Sine. From the first release the positions
    (`LfoOutput`: Shape 0, Square 1, Sine 2) are as permanent as the ids.
- **LFO 2** (added 29 Sep 2026) is a plain sine (`SineLfo`, `std::sin` of
  the phase, free-running) whose only destination is **LFO 1's Shape
  knob**: `sweptLfoShape (knob, level, lfo2) = clamp (knob + level ·
  kMaxLfo2ShapeSweep · lfo2, −1, +1)`, computed every sample before LFO 1
  runs. So the triangle breathes between the two sawtooths, the square's
  duty sweeps (PWM of the LFO), and the sine rocks between its two
  rectified halves.
  - `kMaxLfo2ShapeSweep = 1.0`: full Level from the centre sweeps saw to
    saw. From an off-centre knob the far end is **clamped**, so the wave
    parks at a sawtooth for part of LFO 2's cycle (from +60 %, 37 % of it
    — measured by the suite). The panel's third readout line prints the
    range actually covered, from the same function.
  - **At Level 0 it is bit-identical to the synth before LFO 2** (knob + 0
    is the knob); the suite renders the same patch with two LFO 2 rates
    and compares every sample. The default patch is unchanged (−6.87 dBFS).
  - The Level is smoothed (20 ms); the Rate is not, like LFO 1's.
  - **The LFO display animates with the sweep.** The editor cannot see the
    DSP, so the processor publishes the Shape it is running at as a
    hidden, read-only OUTPUT parameter, `kLfoShapeLive` (id 1001), through
    `data.outputParameterChanges` — at most 60 times a second and only
    when it has moved (`publishLiveShape()`). The editor draws that value;
    when the Shape knob moves it draws the knob until the next live value,
    so the display still follows the knob with no audio running. It is not
    in the table or the state. The caption says "swept by LFO 2" when the
    Level is up. **Needs checking on the Mac**: that Logic (through the AU
    wrapper) and your DAW deliver output parameters to the controller.
  - "Wave shape" was read as LFO 1's Shape because it is the only
    continuous wave-shape control; the VCOs' Wave is a Saw/Square switch.
  - The display draws the **patched** output in green (`> SINE`) and, in
    red, the shape output underneath it, or the square when Shape itself is
    patched (`lfoDisplayData()`, shared with the panel picture). Free-running, bipolar, and slewed by 0.5 ms so a saw reset
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

Four rows in signal order — VCOs, LFO 1 + LFO 2, VCF + VCF ENV,
VCA/OUTPUT + VCA ENV — with an LFO display (both outputs, the patched one marked) and an envelope
display (both ADSRs on one time axis, key-up marked with dots) in a strip on
the right. 1270 × 302 (was 1176 until LFO 1 → resonance).

- **`SimpleSynthControls.*` is FilterDrum's control set**, namespace renamed,
  plus four opt-in additions — `SpyEnvelopeView::setShowAmounts` and
  `setGateMarker`, `SpySelector::setCompact` and `SpyGroupBox::setTitleBand`
  — see its banner. `diff` it against FilterDrum's to see
  exactly that.
- **The layout is data** in `SimpleSynthLayout.h`, with no VSTGUI in it.
- **LFO 2 went in without growing the panel.** Its box ("LFO 2 -> SHAPE")
  took columns 4–5 of row 1, which had been readout text. The readouts
  are now three short lines at `columnX (6, 2)` — LFO 1's period and
  output, its depths, LFO 2's period and Shape range; the engine line
  moved to the title row (`kEngineLineX`) and the retrigger note into the
  flow line. The title and flow text live in `SimpleSynthReadouts.cpp`
  (`panelTitleText()`, `panelFlowText()`) so the picture prints them too.
- **LFO 1 → resonance grew the panel by one column.** "LFO Res"
  (`kVcfLfoRes`, appended after `kLfo2Level`) sits in the VCF box next to
  the cutoff depth, so the VCF and VCA/OUTPUT boxes are five columns and
  both ADSR boxes moved one column right; rows 2–3 are now the widest,
  and `kRowRight` takes the larger of the two candidates. The depth is
  **bipolar round the Resonance knob in the knob's own units** (per cent
  of its travel; loop gain 0..`kMaxResonanceK` internally), clamped to
  0..`kMaxResonanceK` by `lfoResonance()` — so full depth from a knob at 0
  swings into self-oscillation on the LFO's peaks. The lamp still shows
  the knob only. Exactly the knob at depth 0 (tested bit-for-bit).
- **Every string that depends on a value** is made in
  `SimpleSynthReadouts.cpp`, also VSTGUI-free.
- Both, plus the DSP's trace functions, feed **`tools/dump-layout.cpp`**,
  whose JSON `tools/render-panel.py` draws into `docs/panel.png`. So the
  picture is drawn from the code the editor runs, not from a parsed copy.
- **Box titles are inside the box's view** (`setTitleBand`). FilterDrum's
  SpyGroupBox draws the title 5 px *above* its view, and VSTGUI clips a
  view to its rectangle, so the top half of every title was cut off in the
  plug-in (Andy saw it; `render-panel.py` did not clip, so the picture
  hid it — it clips now). The view now starts at the top of the lettering
  and the frame line runs through it; boxes are 53 px tall round a 44 px
  slider (were 58) with 2 px between rows. **FilterDrum has the same
  clipped titles.**
- **The switches are the height of a slider's bar** (`setCompact`): an
  outlined box exactly where a slider's bar sits, the position's name
  where a slider's reading is, the label ("Wave", "Output") under it. They
  were FilterDrum's full-height selector box until 29 Sep; Andy asked for
  them to match the sliders.

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
| `tools/run-tests.sh` — DSP suite | **249 checks, 0 failures** with LFO 2, on arm64 GCC 11 (the Mac's VM) and x86-64 GCC 13 (201 before LFO 2, 184 before the sine) |
| `tools/run-tests.sh` — layout and panel text | **914 checks, 0 failures** with LFO 2 (812 before) |
| Every `source/*.cpp` compiled against the SDK and VSTGUI headers, `-Wall -Wextra` | 8/8, no warnings in our code; `nm -C`: all 57 referenced `SimpleSynth::` symbols defined |
| `tools/linux-validate/` — the plug-in built on Linux (editor stubbed), **SDK validator** | **47 tests passed, 0 failed** (re-run after the sine and after LFO 2: still 47 / 0) |
| `tests/ProcessorTests.cpp` against the real processor | **30 checks, 0 failures** with the live-Shape output (25 before) |

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

## Licensing (step 1: key format, dev keypair, Mac tools)

Implements "Licence key format" from the Nag-ware Licensing Design doc.

- Crypto: Monocypher 4.0.2 in `third_party/monocypher/`, using the OPTIONAL
  `monocypher-ed25519.c` = standard RFC 8032 Ed25519 (SHA-512), so Java /
  OpenSSL on the server can sign compatible keys. Monocypher's default
  `crypto_eddsa_*` (BLAKE2b) is NOT compatible. Compile its .c files as C.
- `source/LicenceKey.{h,cpp}` - 81-byte payload, Crockford Base32 `BNDL1-`
  text, name hash (SHA-256 of trimmed, ASCII-lower-cased name; own
  `LicenceSha256.cpp` since Monocypher has no SHA-256), `verifyKey()`.
  Order: format, version, signature, name, blocklist, product (revoked
  before out-of-scope, so a revoked key is never offered an upgrade).
- `tools/licence/LicenceIssue.{h,cpp}` - signing; tools and tests only. Kept OUT of
  `source/` because CMake globs `source/*.cpp` into the plug-in.
- `source/LicenceConfig.h` - per-build knobs: `kEvaluationDays` (7),
  `kAcceptedProducts`, `kRevokedLicenceIds`. None is in the key or the
  licence file, so a rebuild never invalidates a licence or restarts a trial.
- `source/LicencePublicKey.h` - GENERATED, committed, currently the
  DEVELOPMENT key (`kPublicKeyIsDevelopment = true`). Its private seed is
  `licence-dev/dev-signing-seed.hex` (gitignored, 0600). Replace both with
  the production keypair before release; dev-signed keys then stop working.
- Mac tools: `tools/licence/build.sh` -> `build-licence/r2-licence-keygen`,
  `r2-licence-issue --seed licence-dev/dev-signing-seed.hex --name "..." [--id N]`,
  `r2-licence-check "name" "key"` (same checks as the plugin build).
- Tests: `tests/LicenceTests.cpp` in `tools/run-tests.sh` - RFC 8032 and
  SHA-256 vectors, every single-bit tamper, wrong name/product, blocklist,
  and a GOLDEN KEY made by `tools/licence/reference_key.py` (independent
  Python, standing in for the Java server) that the C++ must match exactly.

### Step 2: the plug-in checks for a licence

- `source/LicenceState.{h,cpp}` (pure, tested): the licence file as JSON
  (`firstRun` date, `name`, `key`), the nag decision, the badge text and the
  activation messages, and pulling a name + key out of a pasted email.
- `source/LicenceStore.{h,cpp}` (file I/O, tested): the shared file
  `~/Library/Application Support/R2Project/licence.json` (Windows %APPDATA%,
  Linux XDG), re-read on every call, written via temp file + rename. A missing
  or corrupt file is a fresh evaluation; an unwritable one never errors (an
  activation then lasts until the editor closes).
- `source/LicenceViews.{h,cpp}` (VSTGUI): `LicenceBadge`, one tiny line in the
  bottom margin under the display strip (Layout::kBadge*; the panel did not
  grow), and `LicenceDialog`, drawn inside the panel as a VSTGUI MODAL VIEW
  SESSION (not a native window): Reminder / Enter / Licensed pages; "Paste from
  email" reads the clipboard. CMD+V IS HANDLED BY THE DIALOG
  (LicenceDialog::onKeyboardEvent), not the text field: a macOS text field
  pastes via the host's Edit menu, and Reaper's Edit > Paste is Reaper's own,
  so Cmd+V pasted nothing there (Andy, 29 Sep). VSTGUI offers Cmd keys to the
  frame first (performKeyEquivalent); consuming the event keeps it from the
  host. A key on the clipboard fills both fields from any page; other text
  goes into the focused field. Cmd+C/X/A are still left to the host.
- Editor: `open()` calls `editorOpened()` (writes firstRun the first time any
  editor opens - never at load, so scans and auval see nothing), adds the
  badge, and shows the reminder if due and `claimSessionReminder()` (once per
  process). Dialog buttons close it via `CFrame::doAfterEventProcessing` -
  removing the dialog inside its own button's mouse-up would free the button
  mid-event. `close()` ends any open session first.
- Rules: licensed -> no nag; day 0..kEvaluationDays -> badge counts down;
  after that the reminder on editor open, "Continue evaluating" for ever.
  Buy licence is hidden until LicenceConfig.h gets a buy URL (none yet).
- Testing the clock: `build-licence/r2-licence-trial show | days-ago 8 |
  reset | unlicense`, then restart the host (the reminder is once per session).
- CMake: Monocypher's two C files added to the plug-in with target_sources;
  linux-validate excludes LicenceViews and adds Monocypher. Linux build +
  validator + ProcessorTests pass with the licence code in (29 Sep).
- VERIFIED on the Mac (Andy, 29 Sep): Xcode build; Reaper (VST3) incl. Cmd+V;
  Logic Pro (AU). A licence entered in Reaper was already active in Logic, so
  the sandboxed AU reads the same ~/Library/Application Support/R2Project/
  licence.json - the design doc's sandbox risk is closed, no app-group
  fallback needed. Not yet: auval (should show no UI); docs/panel.png has
  no badge.


### Step 3 (part): free activation codes from the website (branch licence-server, 30 Sep)

- The site (WebPages, branch licence-server) now issues FREE codes at
  https://www.r2-dvd.net/activate.jsp: name + email in, a signed key out,
  recorded in MySQL. Product code **0x0101 `kProductSimpleSynthFree`**
  (LicenceKey.h), so it never counts as a paid Bundle v1 (0x0001) licence.
  WebPages/LICENSING.md has the server side.
- LicenceConfig.h: `kAcceptedProducts = {kProductBundleV1,
  kProductSimpleSynthFree}`; `kActivationUrl` (empty = no button).
- Reminder page: new first button **Get activation code** opens
  kActivationUrl in the browser - `openInBrowser()` in LicenceViews.cpp,
  LSOpenCFURLRef (CoreServices, linked in CMakeLists.txt; the deprecation
  warning is silenced), ShellExecuteA on Windows. If it fails the reminder
  shows the URL instead. AssertMacros' un-prefixed macros are switched off
  before CoreServices is included.
- Wording is "activation code" throughout: buttons Enter code / Paste /
  Change code; badge "... - activate..."; activation messages say "code".
  The Licensed page thanks for registering when the key is 0x0101.
- The site's result page offers "Copy name and code", which copies
  "Name: ...\nKey: BNDL1-..." - exactly what extractFromPaste reads, so
  Paste fills both fields.
- Tests (167): this build accepts a 0x0101 key MADE BY THE JAVA SERVER
  (golden string, also in WebPages' LicenceKeyTest), C++ makes the identical
  key, a bundle-only build would refuse it, kActivationUrl is https.
- NOT YET CHECKED on the Mac: the Xcode build (the Apple branch of
  openInBrowser was not compiled here; the Linux branch was), the button
  opening Safari from Reaper and from Logic's sandboxed AU host, and the
  whole round trip against the test server. Codes from a server signing
  with the DEVELOPMENT seed work in this build; production needs the real
  keypair first (LICENSING.md).
