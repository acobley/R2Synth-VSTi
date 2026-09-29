# SimpleSynth

A monophonic analogue-style synthesiser. VST3 and Audio Unit, for macOS.

![The panel](docs/panel.png)

## What it is

* **Three VCOs.** Each one is sawtooth or square, with its own Tune
  (±1 octave, continuous) and Level. The waves are band-limited with
  polyBLEP.
* **An MS-20-style LFO.** One **Shape** knob sweeps the wave from a falling
  sawtooth, through a triangle, to a rising sawtooth. The **square** output's
  duty cycle follows the same knob, as the MS-20's does. A third output,
  **Sine**, rounds the triangle with a differential-pair (tanh) shaper, the
  way the EMS VCS3's oscillators make theirs. As with the VCS3's Shape
  control, the same knob moves the shaper off balance, towards a
  rectified-sine shape at either end. The **Output** switch picks which of
  the three waves is used. The LFO's depth is set
  separately for **pitch** (vibrato), **cutoff** and the **VCA** (tremolo).
* **SpaceDub's Moog ladder filter**, using the same per-sample arithmetic,
  with two additions:
  * It is **tuned**: the resonant peak lands on the Cutoff frequency to
    within about a cent at every sample rate.
  * The Resonance knob **self-oscillates at the same point at every
    cutoff**. A lamp on the knob lights when it does.
  
  The filter also has Key Track and an envelope amount of ±6 octaves.
* **Two ADSRs**, one for the filter and one for the VCA. Their decay and
  release times are the time to −60 dB. Each envelope restarts from its
  current level, so a retrigger never clicks.
* **Mono, last-note priority.** Every new note retriggers both envelopes.
  Letting go of a key returns to the last key still held.
* **Output Level**, −48 to +6 dB.

28 parameters, all automatable. The default patch peaks at about −7 dBFS.

## Building

You need macOS, the Xcode command-line tools and CMake.

```sh
./setup-xcode.sh
```

The first configure downloads the VST3 SDK and Apple's AudioUnitSDK into
`external/`. The build installs the VST3 and the AU into
`~/Library/Audio/Plug-Ins/`. Then check the AU:

```sh
auval -v aumu SiSy AECo
```

## Tests

```sh
tools/run-tests.sh            # the DSP suite, then the layout and panel-text suite
python3 tools/render-panel.py # redraws docs/panel.png from the code the editor runs
```

`tools/linux-validate/` builds the plug-in on Linux with the editor stubbed
out. It runs the SDK validator and `tests/ProcessorTests.cpp`. See
`PORTING-NOTES.md` for what was measured, what was found, and why.
