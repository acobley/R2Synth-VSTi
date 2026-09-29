# SimpleDrum — project notes

A **new, empty** VST3 + AUv2 instrument, scaffolded from
`../vst3-port-template` on 29 Sep 2026. Nothing has been ported, and there is
no DSP yet: it renders silence through a smoothed output trim. This file is
the record that outlives the session that wrote it.

## The four decisions

| Decision | Chosen | Why |
|---|---|---|
| Effect or instrument | **Instrument** — `PlugType::kInstrumentDrum`, AU type `aumu`, event input (16 ch) | It is a drum machine |
| Bus layout | **0 in / 2 out** (stereo out, no audio input) | Matches FilterDrum / SampleDrum. Said identically in `initialize`, `setBusArrangements` and the single `SupportedNumChannels` entry in `resource/au-info.plist` (template's 2/2 and 1/1 entries removed) |
| Name and codes | `SimpleDrum` / `SiDr` / `AECo` | `SmDr` is SampleDrum's; `SiDr` checked against FDrm, FTrn, Prj6, SmDr, SDub, SpyB, VcFl — no clash |
| Editor | **Later** | PORTING-GUIDE step 6: a silent plug-in validating with processor, controller and entry only. The host's generic editor is used meanwhile |

## Permanent identity — never change once a build ships

| What | Value |
|---|---|
| Processor UID | `0x6D69A51B, 0xD84BE44A, 0x4927F783, 0x18477FAD` |
| Controller UID | `0x935E5313, 0x9EF72A9E, 0xA1A91CCB, 0x968140F1` |
| AU type / subtype / manufacturer | `aumu` / `SiDr` / `AECo` |
| VST3 bundle id | `audio.simpledrum.vst3` |
| AU bundle id | `audio.simpledrum.audiounit` |
| Vendor | `AE Cobley` (no full stops — REAPER cuts AU names at the first one); AU name `AE Cobley: SimpleDrum` |

UIDs were generated fresh from `os.urandom` and checked against every other
`*IDs.h` in `~/DXi-DEv`. Hosts store them in project files: a changed UID
means every saved session silently loses the plug-in.

## Where the source came from

Adapted from FilterDrum's first commit (`b2e1890`, its own blank scaffold),
renamed, with the editor, the control set and the sample-rate message
removed. House style kept: banner comments that say *why*, `//---` rules.

## Why the DSP is SDK-free

`source/SimpleDrumDsp.{h,cpp}` include nothing from the SDK, for two reasons:

1. They compile with plain `c++ -std=c++17`, so `tests/DspTests.cpp` can run
   without a build — the only executable verification available to a session
   that reaches this Mac through a Linux VM.
2. VST3 splits processor from controller with no shared memory. Anything a
   panel displays that the DSP computes must come from **one shared
   function both call** (`dbToGain` / `gainToDb` today). Put new ones there.

`renderVoices()` is the one empty function — the drum machine goes in there.
Read `PORTING-GUIDE.md` section 5 before writing that loop.

## Measured default output level

- Default patch, rendered: **silence** (`-inf dBFS`), as expected with no voices.
- Trim stage at its default (0 dB), full-scale in: **0.00 dBFS out**,
  bit-identical to the input.

When voices arrive, change the silence assertion in `measureDefaultLevel()`
and insist on a default-patch peak comfortably **below 0 dBFS**. A default
that clips masks other faults.

## Traps already handled in the scaffold

- **`kBypass` is 1000**, far past the table; `paramDef()` is the only place
  `kParams` is indexed and it range-checks. Bypass on an instrument = silence.
- **Never a null title/units to `RangeParameter`** — it dereferences both;
  the symptom is the *validator* segfaulting in the post-build step. Bypass
  is added with `STR16 ("")` units for the same reason.
- **`setState` resets everything to defaults before reading**, so a short
  stream cannot inherit the previous project's values. Processor and
  controller (`setComponentState`) read the identical layout.
- **Append parameters, never insert** — the state stream is by index.
- **Parameter-only blocks** (`numSamples == 0`) apply parameters then return
  `kResultOk`. **64-bit hosts** are accepted; the DSP renders into a float
  scratch sized in `setupProcessing`, widened on the way out. The audio
  thread never allocates; an oversize block is refused rather than grown.
- **Silence flags** are set while the plug-in renders nothing. Clear them the
  moment there are voices.

## Deliberate omissions, and what bites when they are undone

| Omitted | The trap when you add it |
|---|---|
| `getProcessContextRequirements` | Since VST3 3.7 the ProcessContext is opt-in with **no flags by default**: tempo reads 120 in every host, and the validator prints `- None` instead of complaining. The moment anything reads tempo/transport, add the override — the exact code is in the banner of `SimpleDrumProcessor.h` |
| Processor ↔ controller messages | `sendMessage` from `process()` returns success and is **silently discarded** by the host's connection proxy. Send only from `setActive` / `setState` / `notify`; per-block values go out as read-only output parameters (ids 1001+). See `SimpleDrumIDs.h` |
| Editor | Five overrides in the controller; in `editorDestroyed`, `dynamic_cast` returns null inside `~EditorView()`, so **compare upcast pointers**. See the banner of `SimpleDrumController.h` |
| Note handling | `handleEvent` unpacks note on/off and does nothing. Use `event.sampleOffset` — starting every voice at offset 0 quantises hits to the block size |
| `getTailSamples` | Without it a host may stop calling `process()` the moment notes stop, truncating every decay. Add it with the first voice |

## Verification run (29 Sep 2026, from the Linux VM)

```sh
cd ~/DXi-DEv/SimpleDrum-VSTi

# DSP suite
c++ -std=c++17 -O2 -Wall -Wextra -Isource tests/DspTests.cpp source/SimpleDrumDsp.cpp -o /tmp/dsptests && /tmp/dsptests
#   -> 63 checks, 0 failures (incl. the half-gain negative control)

# Every .cpp against the SDK, to object files
SDK=~/DXi-DEv/FilterDrum-VSTi/external/vst3sdk     # compile check only
for f in source/*.cpp; do
  g++ -c -std=c++17 -DLINUX=1 -DRELEASE=1 -I$SDK -I$SDK/vstgui4 -Isource \
      -o /tmp/$(basename $f .cpp).o $f || echo "FAILED $f"
done
nm -C /tmp/*.o | grep " U " | grep "SimpleDrum::"
#   -> 5/5 compiled; all 9 referenced SimpleDrum symbols defined in another object
```

`-Wall` shows six harmless `-Wsign-compare` warnings (`int` vs the `Param`
enum) inherited from the FilterDrum scaffold.

**Not verified from here:** cmake, Xcode, the SDK validator, `auval`.

## SDK decision

**In-tree clone** (the default): the first `./setup-xcode.sh` clones the VST3
SDK (~250 MB) and Apple's AudioUnitSDK into `external/`, which is
`.gitignore`d. Chosen so the project is self-contained and can ship
separately. FilterDrum's checkout was used above only as a compile-check
include path, not by the build.

## Next steps on the Mac

```sh
cd ~/DXi-DEv/SimpleDrum-VSTi && ./setup-xcode.sh
# build, then:
auval -v aumu SiDr AECo
```

and run the SDK validator (it runs as a post-build step; watch its output).
If a source file is added later and the build says *"Bundle does not export
the required 'GetPluginFactory' function"*, re-run
`./setup-xcode.sh --no-open` and build again.
