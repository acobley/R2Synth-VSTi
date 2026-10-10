//------------------------------------------------------------------------
// R2Synth - the audio engine
//
// NO SDK HEADER MAY ENTER THIS FILE OR ITS .cpp. That is deliberate, and
// there are two reasons for it.
//
//   1. It compiles and runs standalone with plain `c++ -std=c++17`, so
//      its numbers can be tested before anything is built - which
//      matters on this project, because the session that writes the code
//      reaches the Mac through a Linux VM and cannot run Xcode, the SDK
//      validator or auval. tests/DspTests.cpp is the thing here that
//      actually executes during development.
//
//   2. VST3 splits the processor from the controller, and they do not
//      share memory. Anything the EDITOR displays that the DSP COMPUTES
//      must therefore come from ONE SHARED FUNCTION that both call - not
//      from two copies of the arithmetic that will drift apart. This
//      header is where those functions live: ladderTuning(),
//      selfOscillating(), cutoffCeilingHz(), lfoShapeValue(),
//      lfoSquareValue(), lfoSineValue(), sweptLfoShape(),
//      lfo2ShapeRange(), traceEnvelopes() and traceLfo() are all called
//      by the audio path AND by the panel.
//
// THE SIGNAL PATH - one monophonic voice, last-note priority:
//
//                                              LFO 2 (sine) --> Shape
//   VCO 1 --\                                          LFO (MS-20 MG)
//   VCO 2 ---+-- mixer --> Moog ladder VCF --> VCA --> output level
//   VCO 3 --/               ^  ^  ^            ^  ^
//                           |  |  key track    |  LFO (tremolo)
//                           |  LFO             VCA ENV (ADSR)
//                           VCF ENV (ADSR)
//
//   * Three VCOs, each sawtooth or square, band-limited with polyBLEP,
//     each with its own Tune (+/- 12 semitones, continuous) and Level.
//     The LFO's Pitch depth moves all three together (vibrato).
//   * The LFO is the Korg MS-20's modulation generator: ONE Shape knob
//     sweeps the wave from a falling sawtooth through a triangle to a
//     rising one, and the square output's duty cycle follows the same
//     knob. A third output, SINE, is the triangle rounded by a
//     differential-pair (tanh) shaper, the way the EMS VCS3's oscillators
//     make their sine - and, as on the VCS3, the Shape knob moves the
//     shaper's balance, so the sine leans towards a rectified shape at
//     either end. An Output switch picks which of the three drives the
//     destinations - pitch, cutoff and VCA.
//   * LFO 2 is a plain sine with a Rate and a Level, and its one
//     destination is LFO 1's SHAPE: it turns the Shape knob for you,
//     so the wave leans one way and then the other - a triangle
//     breathing between the two sawtooths, a square whose duty sweeps
//     (PWM of the LFO), or a sine rocking between its two rectified
//     halves. See sweptLfoShape().
//   * The filter is SWITCHABLE (VCF Type, 7 Oct 2026):
//       MOOG  - SpaceDub's Moog ladder ("Moog VCF, variation 2",
//               musicdsp.org), its per-sample arithmetic unchanged - but
//               TUNED: see MoogLadder and ladderTuning() for the two
//               things that had to be added to make a delay-line filter
//               a synth filter. 4-pole, 24 dB/oct, bass loss with
//               resonance.
//       MS-20 I  - the EARLY MS-20's filter (the Korg35 chip): a 2-pole,
//               12 dB/oct Sallen-Key low-pass with a diode pair in its
//               FORWARD path - everything heard is bent, more as it gets
//               louder. No bass loss. See Ms20Filter.
//       MS-20 II - the LATER MS-20's filter (LM13600 OTAs): the same
//               slope, buffered stages, and strings of three diodes in
//               the FEEDBACK path - only the resonance is bent. See
//               Ms20LateFilter. (Diode placements from T. Stinchcombe's
//               study of the two filters.)
//     All three take the same Cutoff and the same Resonance in loop gain
//     (1.0 = self-oscillation at every cutoff, for all), so the knobs,
//     the lamp and the modulation mean the same thing whichever is
//     chosen. A switch cross-fades over kFilterCrossfadeSeconds.
//   * PORTAMENTO: the pitch the VCOs play SLIDES from the last note to
//     the new one, in a straight line in semitones taking the knob's time
//     whatever the interval. ALWAYS, not only legato: a key struck after
//     the previous one was released glides too, because the pitch is
//     held between notes the way an analogue keyboard's CV is. See
//     R2SynthDsp::noteOn() and portamentoActive().
//   * Two ADSRs, one on the cutoff (in octaves, signed) and one on the
//     VCA. Retriggered on every new note, FROM WHEREVER THEY ARE, so a
//     retrigger never clicks.
//
// Before changing a processing loop, read ../PORTING-GUIDE.md section 5 -
// the rules about not allocating on the audio thread, denormals and
// 64-bit hosts apply to new DSP exactly as they do to a port.
//------------------------------------------------------------------------

#pragma once

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace R2Synth {

//------------------------------------------------------------------------
/** Stereo, and only stereo. It agrees with setBusArrangements in the
    processor and with the single 0-in / 2-out entry in
    resource/au-info.plist; all three have to say the same thing or auval
    rejects the Audio Unit. The voice is mono and is written to both. */
constexpr int kChannelCount = 2;

constexpr int kNumVcos = 3;

/** How long a smoothed control takes to reach a new value, in seconds.

    IT IS NOT A TASTE SETTING. It is what keeps a parameter move from
    clicking, and the tests assert that no single sample steps the output
    gain by more than 1 % - which at the lowest sample rate anyone runs is
    the binding constraint on how short this may be. */
constexpr double kTrimSmoothingSeconds = 0.020;

/** The floor below which a smoother's own state is flushed to zero -
    the recursive element, never the signal. */
constexpr float kDenormalFloor = 1e-25f;

//------------------------------------------------------------------------
// Gain staging
//
// MEASURED, not guessed: tests/DspTests.cpp renders the default patch and
// prints its peak, and the constants below are what put it comfortably
// under 0 dBFS. Change one and re-run the suite before believing the
// result.
//------------------------------------------------------------------------

/** What the three VCO levels are summed through. Three full-level
    oscillators in phase would otherwise be +9.5 dB into the filter. */
constexpr double kMixerGain = 0.5;

/** The voice's gain after the VCA, before the Output Level. */
constexpr double kVoiceGain = 0.9;

/** THE MS-20 FILTERS' LEVEL TRIMS, -5 dB each, applied to their outputs
    in the voice (not part of their responses - the screen draws the
    filters themselves).

    MEASURED (7 Oct 2026, tests/DspTests.cpp prints it), together with
    the diode knees: the MS-20s have no bass loss, so untrimmed they
    played the default patch about 5 dB louder than the ladder. At 0.56
    each one's default-patch RMS is within 0.1 dB of the ladder's, so
    flipping the switch compares character, not loudness; the worst
    cases are -7 dBFS (I) and -5 dBFS (II), self-oscillation alone about
    -9.5 dBFS for both. */
constexpr double kMs20OutputGain     = 0.56;   // MS-20 I
constexpr double kMs20LateOutputGain = 0.56;   // MS-20 II

/** THE FILTER'S NOISE FLOOR, -100 dBFS of white noise into the ladder.

    A digital filter started from exact zero stays at exact zero however
    much gain its loop has, so with every VCO turned down a
    self-oscillating ladder would be silent - which is not what turning
    the resonance up past the lamp should do. An analogue one starts from
    its own thermal noise; this is that. SpaceDub added a noise table to
    its filter input for a related reason. At -100 dBFS it is inaudible
    in everything except the one case it exists for. */
constexpr double kFilterNoiseLevel = 1.0e-5;

//------------------------------------------------------------------------
// Ranges the panel quotes and the DSP enforces - one copy of each.
//------------------------------------------------------------------------

/** The VCF envelope's reach, in octaves either way. */
constexpr double kMaxEnvOctaves = 6.0;

/** The lowest cutoff the ladder is asked for, whatever the modulation. */
constexpr double kMinCutoffHz = 20.0;

/** THE CUTOFF CEILING, as a fraction of the sample rate.

    The ladder's resonant peak reaches Nyquist at a coefficient of about
    1.0 and the tuning curve bends hard above 0.4 fs, so 0.40 is where the
    tuning is still honest. That is 17.6 kHz at 44.1 k and 19.2 kHz at 48 k,
    so the top of the Cutoff knob (20 kHz) is unreachable at the common
    rates - the panel says so rather than letting it look like a bug. */
constexpr double kMaxCutoffFraction = 0.40;

/** The resonance knob's top, as LOOP GAIN. 1.0 is the self-oscillation
    threshold at EVERY cutoff - see ladderTuning() - so the top tenth of
    the knob self-oscillates and the lamp on the panel says so. */
constexpr double kMaxResonanceK = 1.1;

/** WHICH FILTER. The values are the VCF Type switch's positions and
    are stored in projects, so they are APPEND-ONLY, like parameter ids.
    Moog is 0, so every project saved before the switch existed opens
    with the filter it was made with. Ms20 is the EARLY (Korg35) filter,
    Ms20Late the later OTA one. */
enum class FilterType { Moog = 0, Ms20 = 1, Ms20Late = 2 };
constexpr int kNumFilterTypes = 3;

/** The VCF Type parameter's internal value (0, 1, 2) as a FilterType -
    rounded and clamped. ONE COPY, for the processor and the panel. */
inline FilterType filterTypeFromInternal (double v)
{
	const int i = static_cast<int> (std::lround (v));
	return (i <= 0) ? FilterType::Moog : (i == 1) ? FilterType::Ms20 : FilterType::Ms20Late;
}

/** How long a change of VCF Type takes: the filters involved run and the
    output cross-fades linearly from one to the other. Long enough that a
    switch mid-note is a quick blend rather than a click; short enough to
    sound like a switch. */
constexpr double kFilterCrossfadeSeconds = 0.010;

/** THE MS-20 DIODES (7 Oct 2026) - after T. Stinchcombe, "A Study of the
    Korg MS10 & MS20 Filters" (2006), which settles where each revision
    puts them:

      * EARLY (Korg35, MS-20 I): ONE back-to-back pair, in the FORWARD
        path, inside the gain element whose output is both the filter's
        output and the source of its feedback. Every frequency passes
        through them: the sound gets richer as the input gets louder,
        resonance or not.
      * LATER (LM13600 OTA, MS-20 II): STRINGS OF THREE back-to-back
        diodes in the gain element of the FEEDBACK path. Only what is fed
        back - frequencies near the cutoff - is bent; a loud note well
        below the cutoff passes almost clean.

    THE DIODE LAW. A diode's current is exponential in its voltage, so a
    resistor feeding a back-to-back pair (or pair of strings) obeys

        x = v + eps * sinh (v / Vd)      (x driven in, v across the diodes)

    - effectively linear while they are off, then a SHARP knee, then v
    still rising, but only logarithmically: no flat ceiling, unlike a
    tanh. The knee is where conduction sets in, in this engine's signal
    units; the SHARPNESS is knee / Vd in thermal voltages: a silicon
    diode's 0.6 V over n Vt = 1.8 x 26 mV is 12.8 - and a string of three
    is three times both, so the SAME 12.8; only the knee moves. From the
    two, Vd = knee / sharpness and eps = knee / sinh (sharpness): at the
    knee the diodes carry as much as the resistor alone would, and below
    it almost nothing (small-signal gain 1 - 1.5e-4, so the responses the
    screen draws and the self-oscillation thresholds are unmoved).

    THE KNEES were SET BY MEASUREMENT with the output trims, not derived:
    this engine's signal units are not volts, and the gains round each
    circuit's diodes are not modelled. MS-20 I's 1.0 leaves a single VCO
    at the default patch clean (0.7 % THD for a 0.75 sine) and bites
    hard on three VCOs (12 % at 1.5); MS-20 II's 1.8 puts its
    self-oscillation at the same level as MS-20 I's. (Not 3 x 1.0: the
    string has three drops, but the gain in front of it is not the gain
    in front of the early pair, and neither is modelled.) */
constexpr double kMs20DiodeSharpness = 12.8;
constexpr double kMs20DiodeKnee      = 1.0;   // early: one pair, forward path
constexpr double kMs20LateDiodeKnee  = 1.8;   // later: strings of three, feedback

/** THE KORG35'S DAMPING: Stinchcombe's analysis gives the early filter as
    a true Sallen-Key,

        H(s) = 1 / (s^2 + (7/3 - k) s + 1)

    - the 7/3 rather than 2 is the first stage loaded by the second - so
    it self-oscillates at k = 7/3, and at no resonance it is a little more
    damped than the OTA version (-7.4 dB at the cutoff, not -6). The OTA
    revision buffers its stages, so its constant is exactly 2. */
constexpr double kKorg35Damping = 7.0 / 3.0;
constexpr double kOtaDamping    = 2.0;

/** THE LATE MS-20'S INPUT OTA: the signal into the first pole passes an
    OTA, whose transfer is tanh; scaled by this before the tanh and back
    after, so a quiet signal sees the linear filter and a full-level one
    is rounded only slightly - the real circuit attenuates into its OTAs
    to keep them nearly linear. SET BY MEASUREMENT (7 Oct 2026) with the
    diode knee and the trim. */
constexpr double kOtaDrive = 0.2;

/** The LFO's reach on the VCOs, in semitones. */
constexpr double kMaxLfoPitchSemis = 12.0;

/** The LFO's reach on the cutoff, in octaves. */
constexpr double kMaxLfoCutoffOctaves = 4.0;

/** The LFO's reach on the resonance, in LOOP GAIN - the whole of the
    Resonance knob's range, so at full depth a knob at 0 swings up to
    self-oscillation on the LFO's peaks. */
constexpr double kMaxLfoResonanceK = kMaxResonanceK;

/** PORTAMENTO'S RANGE, in seconds: the knob runs logarithmically from
    kMinPortamentoSeconds to kMaxPortamentoSeconds, and the BOTTOM OF THE
    KNOB IS OFF - a 1 ms slide is inaudible, and an exact jump is what
    "off" must mean. Anything up to kPortamentoOffSeconds (a hair above
    the bottom, so a knob parked at the stop is off) jumps. */
constexpr double kMinPortamentoSeconds = 0.001;
constexpr double kMaxPortamentoSeconds = 5.0;
constexpr double kPortamentoOffSeconds = 0.00105;

/** Envelope calibration: the Decay and Release knobs mean the time to
    fall to -60 dB of the distance remaining, which is what "release time"
    means on every synth that states it. The envelope then runs on to
    -100 dB before it calls itself idle. */
constexpr double kReleaseTargetLevel = 0.001;   // -60 dB: what the knob means
constexpr double kEnvelopeIdleLevel  = 1e-5;    // -100 dB: where it stops

/** The attack aims past 1.0 and stops when it gets there, like an RC
    charging towards a rail above the comparator's threshold. This is the
    rail; 1.3 gives the familiar slightly-convex analogue attack. */
constexpr double kAttackOvershoot = 1.3;

/** The LFO's output is slewed this much before it reaches anything, so
    a sawtooth's reset or a square's edge on the VCA is a 0.5 ms ramp and
    not a click. At the fastest rate (20 Hz, 50 ms a cycle) it changes
    nothing you can see on the display. */
constexpr double kLfoSlewSeconds = 0.0005;

/** The square output's duty cycle never quite reaches 0 or 1, which
    would make it DC. */
constexpr double kMinLfoDuty = 0.05;

/** THE SINE SHAPER'S DRIVE: how hard the triangle is pushed into the
    tanh curve.

    MEASURED, not chosen. A differential pair's transfer curve is tanh,
    and rounding a triangle with it is a sine only at one drive: too
    little and the corners stay pointed, too much and the top flattens
    towards a square. Swept in 0.01 steps, 1.40 gives the least total
    harmonic distortion - 1.4 %, against 12 % for the triangle it starts
    from - and tests/DspTests.cpp measures it on every run. */
constexpr double kSineDrive = 1.40;

/** How far the Shape knob unbalances the shaper at either end, in
    triangle amplitudes. At 1.0 the triangle's trough sits exactly on the
    shaper's centre, where it is steepest, and its peak deep in
    saturation - so one half-cycle is a sharp V and the other a broad
    dome: the rectified-sine look the VCS3's Shape control gives its
    sine. */
constexpr double kMaxSineBalance = 1.0;

//------------------------------------------------------------------------
// The shared functions
//
// Called by the DSP, by the controller's parameter text, and by the
// editor's readouts. One copy of the arithmetic, so the number on screen
// and the number in the audio path cannot disagree.
//------------------------------------------------------------------------

/** Decibels to a linear gain. Exactly 1.0 at 0 dB, which is what makes
    the output stage bit-identical at unity rather than nearly so. */
inline double dbToGain (double db)
{
	return (db == 0.0) ? 1.0 : std::pow (10.0, db / 20.0);
}

/** And back, with a floor so silence does not return -inf. */
inline double gainToDb (double gain)
{
	return (gain <= 1e-9) ? -180.0 : 20.0 * std::log10 (gain);
}

/** MIDI note to Hz, A4 = 440. Fractional notes are fine - the tune and
    vibrato are added in semitones before this is called. */
inline double noteToHz (double note)
{
	return 440.0 * std::exp2 ((note - 69.0) / 12.0);
}

/** The highest cutoff the filter is driven to at this sample rate. The
    panel prints it next to the engine rate. */
inline double cutoffCeilingHz (double sampleRate)
{
	return sampleRate * kMaxCutoffFraction;
}

/** The self-oscillation predicate, in loop gain. The resonance lamp and
    the filter both read it here. */
inline bool selfOscillating (double resonanceK)
{
	return resonanceK >= 1.0;
}

/** THE RESONANCE LFO 1 LEAVES THE FILTER AT: the knob, plus `depth`
    times the LFO (-1..+1), clamped to the knob's own range. BIPOLAR,
    round the knob, so the knob stays the centre of the wobble; the
    clamp is what the ear hears as the wobble flattening at either end.
    EXACTLY the knob at depth 0 - the tests hold it to that. All three
    in loop gain. */
inline double lfoResonance (double knobK, double depthK, double lfo)
{
	if (depthK <= 0.0)
		return knobK;
	const double k = knobK + depthK * lfo;
	return (k < 0.0) ? 0.0 : (k > kMaxResonanceK) ? kMaxResonanceK : k;
}

/** Whether a Portamento setting (seconds) glides at all. The DSP and the
    panel's "off" both read it here. */
inline bool portamentoActive (double seconds)
{
	return seconds > kPortamentoOffSeconds;
}

/** The lowest and highest resonance lfoResonance() reaches over a
    cycle, for the panel's readout. */
inline void lfoResonanceRange (double knobK, double depthK, double& lo, double& hi)
{
	lo = lfoResonance (knobK, depthK, -1.0);
	hi = lfoResonance (knobK, depthK, +1.0);
}

/** THE MS-20 SHAPE. `shape` runs -1..+1; phase 0..1.

      -1  falling sawtooth  (+1 down to -1)
       0  triangle
      +1  rising sawtooth   (-1 up to +1)

    It is a triangle whose peak slides across the cycle: the peak sits at
    (shape + 1) / 2 of the way through. Every value in between is a
    skewed triangle, which is what the MS-20's single Waveform knob does -
    one control, not a switch between three waves. */
inline double lfoShapeValue (double phase, double shape)
{
	if (shape < -1.0) shape = -1.0;
	if (shape >  1.0) shape =  1.0;
	const double peak = 0.5 * (shape + 1.0);

	if (phase < peak)
		return -1.0 + 2.0 * phase / peak;               // rising part

	const double fall = 1.0 - peak;
	if (fall <= 0.0)
		return 1.0;                                     // shape = +1 exactly
	return 1.0 - 2.0 * (phase - peak) / fall;           // falling part
}

/** The duty cycle of the square output at this Shape - it FOLLOWS THE
    SHAPE KNOB, as the MS-20's pulse output does: 50 % at the triangle,
    narrowing towards either sawtooth. Clamped at kMinLfoDuty. */
inline double lfoSquareDuty (double shape)
{
	if (shape < -1.0) shape = -1.0;
	if (shape >  1.0) shape =  1.0;
	const double duty = 0.5 * (shape + 1.0);
	if (duty < kMinLfoDuty)       return kMinLfoDuty;
	if (duty > 1.0 - kMinLfoDuty) return 1.0 - kMinLfoDuty;
	return duty;
}

/** The square output: +1 for the first `duty` of the cycle, -1 after -
    so its rising edge lines up with the shape output's lowest point. */
inline double lfoSquareValue (double phase, double shape)
{
	return (phase < lfoSquareDuty (shape)) ? 1.0 : -1.0;
}

/** THE SINE OUTPUT, the EMS VCS3 way: a SYMMETRICAL triangle (the core,
    not the skewed shape output) into a differential-pair shaper -
    tanh (kSineDrive * (triangle + balance)) - rescaled so it always
    swings exactly -1..+1.

    At Shape 0 the balance is 0 and this is the best sine a tanh can
    make of a triangle (1.4 % THD). The Shape knob moves the balance, as
    the VCS3's Shape control moves its sine shaper off centre: the half
    of the cycle pushed into saturation broadens into a dome, the other
    half sharpens into a V, and at either end the wave looks like a
    rectified sine - positive-domed one way, negative-domed the other.
    Same knob, same gesture - asymmetry - on all three outputs. */
inline double lfoSineValue (double phase, double shape)
{
	if (shape < -1.0) shape = -1.0;
	if (shape >  1.0) shape =  1.0;
	const double balance = shape * kMaxSineBalance;

	const double tri = lfoShapeValue (phase, 0.0);
	const double lo  = std::tanh (kSineDrive * (-1.0 + balance));
	const double hi  = std::tanh (kSineDrive * ( 1.0 + balance));
	const double y   = std::tanh (kSineDrive * (tri + balance));

	// The rescale is what keeps the depth knobs meaning the same thing
	// on every output: whatever the balance, the wave spans -1..+1.
	return (2.0 * y - (hi + lo)) / (hi - lo);
}

/** LFO 2's reach on LFO 1's Shape, in Shape units (the Shape knob runs
    -1..+1): at a Level of 100 % the sine adds +/- this much to the
    knob. 1.0 means that from the centre (a triangle) full Level sweeps
    from one sawtooth to the other; from any other setting the far end
    is clamped, so the wave dwells at that sawtooth for part of LFO 2's
    cycle, as a knob turned against its end stop would. */
constexpr double kMaxLfo2ShapeSweep = 1.0;

/** THE SHAPE LFO 1 ACTUALLY RUNS AT: the knob plus LFO 2, clamped to
    the knob's own range. `level` 0..1, `lfo2` -1..+1. At level 0 it is
    the knob EXACTLY - adding zero - so a patch that does not use LFO 2
    is bit-identical to one made before it existed; the tests hold it
    to that. */
inline double sweptLfoShape (double knob, double level, double lfo2)
{
	double s = knob + level * kMaxLfo2ShapeSweep * lfo2;
	if (s < -1.0) s = -1.0;
	if (s >  1.0) s =  1.0;
	return s;
}

/** The range of Shape that LFO 2 sweeps across - for the panel's LFO 2
    line, from the same sweptLfoShape() the DSP runs. */
inline void lfo2ShapeRange (double knob, double level, double& lo, double& hi)
{
	lo = sweptLfoShape (knob, level, -1.0);
	hi = sweptLfoShape (knob, level,  1.0);
}

/** Which of the three LFO outputs drives the destinations. The values
    are the Output switch's positions and are stored in projects, so they
    are APPEND-ONLY, like parameter ids. */
enum class LfoOutput { Shape = 0, Square = 1, Sine = 2 };

inline double lfoOutputValue (LfoOutput output, double phase, double shape)
{
	switch (output)
	{
		case LfoOutput::Square: return lfoSquareValue (phase, shape);
		case LfoOutput::Sine:   return lfoSineValue (phase, shape);
		case LfoOutput::Shape:
		default:                return lfoShapeValue (phase, shape);
	}
}

//------------------------------------------------------------------------
/** A one-pole glide towards a target.

    The coefficient is recomputed from the sample rate, so the glide takes
    the same number of MILLISECONDS at 44.1 k and at 192 k. */
class Smoother
{
public:
	void setSampleRate (double sampleRate, double seconds);

	/** Jump straight to the target, no glide. Used at reset and at every
	    sample-rate change: a smoother left at zero would fade the whole
	    instrument in every time the host restarted it. */
	void snap (float target);

	void setTarget (float target) { mTarget = target; }
	float target () const { return mTarget; }
	float value () const { return mValue; }

	/** One sample of glide, returning the value to use for it. */
	float next ();

	float coefficient () const { return mCoeff; }

private:
	float mValue  = 0.f;
	float mTarget = 0.f;
	float mCoeff  = 1.f;
};

//------------------------------------------------------------------------
/** One VCO: sawtooth or square, band-limited with polyBLEP.

    POLYBLEP because a naive sawtooth at 2 kHz is mostly aliases, and the
    polyBLEP correction costs two branches a sample. It is not perfect -
    the residual aliasing is about -40 dB at the top of the keyboard - but
    it is the right cost for a mono voice.

    FREE-RUNNING: the phase is not reset on a note, which is what an
    analogue VCO does and why three detuned ones beat against each other
    rather than starting every note identically. */
class Vco
{
public:
	enum class Wave { Saw, Square };

	void setWave (Wave w) { mWave = w; }
	Wave wave () const { return mWave; }

	/** Where the phase starts at reset. The three VCOs start a third of
	    a cycle apart so that three identical ones are not one loud one on
	    the first note after a reset. */
	void reset (double startPhase) { mPhase = startPhase; }

	/** One sample. `increment` is Hz / sample rate, already clamped by
	    the caller to below Nyquist. */
	double next (double increment);

	double phase () const { return mPhase; }

private:
	Wave   mWave  = Wave::Saw;
	double mPhase = 0.0;
};

//------------------------------------------------------------------------
/** The Korg MS-20 modulation generator.

    The MS-20's two outputs, a VCS3-style third, and an Output switch
    that picks which one drives the destinations:

      * SHAPE  - the skewed triangle, lfoShapeValue()
      * SQUARE - lfoSquareValue(), whose duty follows the same knob
      * SINE   - lfoSineValue(), the triangle core through a tanh shaper
                 whose balance follows the same knob

    Bipolar, -1..+1. FREE-RUNNING - not reset by notes - because the
    MS-20's is not either. */
class MsLfo
{
public:
	void setSampleRate (double sampleRate);
	void setRate (double hz) { mRateHz = hz; }
	void setShape (double shape) { mShape = shape; }
	void setOutput (LfoOutput output) { mOutput = output; }

	void reset ();

	/** One sample of the selected output, slewed - see kLfoSlewSeconds. */
	double next ();

	double phase () const { return mPhase; }

private:
	double mSampleRate = 44100.0;
	double mRateHz = 4.0;
	double mShape  = 0.0;
	LfoOutput mOutput = LfoOutput::Shape;
	double mPhase  = 0.0;
	double mSlewed = 0.0;
	double mSlewCoeff = 1.0;
};

//------------------------------------------------------------------------
/** LFO 2: a sine, and nothing else.

    A sine because its job is to TURN A KNOB, and a hand turning a knob
    back and forth slows down at each end - which is what a sine does
    and a triangle does not. std::sin on the phase, so it is a true
    sine, not a shaped one; its output needs no slew because it has no
    corners.

    FREE-RUNNING, like LFO 1. Starts at phase 0 - the knob's own
    setting, rising - on reset. */
class SineLfo
{
public:
	void setSampleRate (double sampleRate);
	void setRate (double hz) { mRateHz = hz; }
	void reset () { mPhase = 0.0; }

	/** One sample, -1..+1. */
	double next ();

	double phase () const { return mPhase; }

private:
	double mSampleRate = 44100.0;
	double mRateHz = 0.5;
	double mPhase  = 0.0;
};

//------------------------------------------------------------------------
/** What the ADSR is set to. Times in SECONDS; sustain 0..1. */
struct AdsrSpec
{
	double attack  = 0.005;
	double decay   = 0.300;
	double sustain = 0.5;
	double release = 0.300;
};

/** How long an envelope's release really runs, from its knob setting.
    The knob is calibrated to -60 dB; the envelope runs to -100 dB, which
    takes 5/3 of the setting. getTailSamples reports THIS, not the knob. */
inline double releaseTailSeconds (double releaseSeconds)
{
	return releaseSeconds * std::log (kEnvelopeIdleLevel) / std::log (kReleaseTargetLevel);
}

/** An analogue-style ADSR: RC segments.

    ATTACK charges towards kAttackOvershoot and ends when it passes 1.0;
    DECAY and RELEASE fall exponentially, calibrated so the knob is the
    time to -60 dB of the distance to go. SUSTAIN is tracked
    continuously, so moving the knob during a held note moves the level.

    RETRIGGER STARTS FROM WHERE IT IS. gateOn() never sets the level to
    zero first. On a mono synth every new note retriggers both envelopes,
    and resetting to zero would put a click on every legato phrase. */
class Adsr
{
public:
	enum class Stage { Idle, Attack, Decay, Sustain, Release };

	void setSampleRate (double sampleRate);
	void setSpec (const AdsrSpec& spec);
	const AdsrSpec& spec () const { return mSpec; }

	void gateOn ();
	void gateOff ();
	void reset ();

	/** One sample, 0..1. */
	float next ();

	Stage stage () const { return mStage; }
	double level () const { return mLevel; }
	bool active () const { return mStage != Stage::Idle; }

private:
	void recompute ();

	AdsrSpec mSpec;
	double mSampleRate = 44100.0;
	Stage  mStage = Stage::Idle;
	double mLevel = 0.0;
	double mAttackCoeff  = 1.0;
	double mDecayCoeff   = 1.0;
	double mReleaseCoeff = 1.0;
};

//------------------------------------------------------------------------
/** THE LADDER'S TUNING, which SpaceDub never needed.

    SpaceDub drives its ladder with coefficient = Hz / (fs/4), and that is
    fine in a delay line's feedback path where nothing is played against
    it. It is not fine on a synth's cutoff knob: measured, the resonant
    peak lands at about 0.73 of the requested frequency low down and
    runs SHARP above a quarter of the sample rate, so the number on the
    knob would be wrong everywhere and wrong differently at 48 k and 96 k.

    And the self-oscillation threshold moves with the coefficient (a
    feedback of 4.01 at 180 Hz, 4.29 at 12.7 kHz), so a fixed resonance
    setting would sing at some cutoffs and not others.

    This table fixes both, from the filter's own transfer function rather
    than from measurement: for each normalised frequency it holds the
    coefficient that puts the resonant peak THERE (where the loop's phase
    is -180 degrees) and the feedback at which the loop gain there is
    exactly 1. The tests then MEASURE the result against it.

    Built once, on first use, from the constructor - never on the audio
    thread. Sample-rate independent: it is indexed by Hz / fs. */
struct LadderTuning
{
	static constexpr int    kPoints = 256;
	static constexpr double kMinNorm = 1.0e-4;               // 4.4 Hz at 44.1 k
	static constexpr double kMaxNorm = kMaxCutoffFraction;   // the ceiling

	double coefficient[kPoints];
	double unityFeedback[kPoints];   // SpaceDub's `resonance` at loop gain 1

	/** Interpolated lookup. `normFreq` is Hz / sample rate, clamped. */
	void lookup (double normFreq, double& coeff, double& feedbackAtUnity) const;
};

/** The one table. Thread-safe static initialisation; call it once from a
    constructor so the first call is not on the audio thread. */
const LadderTuning& ladderTuning ();

/** The loop's phase lag at `w` radians/sample for a SpaceDub coefficient,
    and the loop gain magnitude there - exposed for the tests. */
double ladderLoopPhase (double w, double coefficient);
double ladderLoopGain (double w, double coefficient, double resonance);

/** THE LADDER'S LINEAR RESPONSE: output / input magnitude at `w`
    radians/sample, for a SpaceDub coefficient and feedback - the same
    stage response the two functions above use, closed round the loop:

        H = G / (1 + G fb z^-1),   G = 0.35013 F^4 S(w)^4

    which is MoogLadder::tick with limitFeedback() in its linear region
    (any signal below the knee). The tests hold it to a measured ladder.
    At loop gain 1 and over the peak is unbounded - the filter is then an
    oscillator, and it is limitFeedback() that sets the level. */
double ladderResponseGain (double w, double coefficient, double resonance);

/** THE FILTER DISPLAY'S DATA, in dB: `count` points log-spaced from
    minHz to maxHz of the ladder as process() sets it up for a cutoff
    and a resonance (loop gain) - the same ladderTuning() lookup, the
    same clamp to the ceiling, the same scaling of resonance to this
    cutoff's unity feedback. Points at or above Nyquist read -120 dB.
    Not clamped above: a self-oscillating peak is large, and the panel
    decides how much of it to draw. */
void traceFilterResponse (double cutoffHz, double resonanceK, double sampleRate,
                          double minHz, double maxHz, float* dbOut, int count);

/** The feedback term's soft limiter. Linear up to 1.5 - so a quiet
    signal sees SpaceDub's filter exactly - then bending smoothly to a
    ceiling of 3.0. This is what gives self-oscillation an amplitude; the
    original's only bound was a hard clamp of the state at +/-16, which
    with the loop gain over 1 is +24 dBFS of sine. */
inline double limitFeedback (double x)
{
	constexpr double kKnee = 1.5;
	const double a = std::fabs (x);
	if (a <= kKnee)
		return x;
	const double bent = kKnee + kKnee * std::tanh ((a - kKnee) / kKnee);
	return (x < 0.0) ? -bent : bent;
}

//------------------------------------------------------------------------
/** SpaceDub's Moog ladder - "Moog VCF, variation 2" (musicdsp.org) - with
    the per-sample arithmetic of DelayLine::process unchanged:

        f  = coeff * 1.16
        fb = resonance * (1 - 0.15 f^2)
        x -= out4 * fb;   x *= 0.35013 f^4
        four one-poles: out = x + 0.3 in + (1 - f) out

    TWO DIFFERENCES, both documented where they are:
      * the coefficient and feedback come from ladderTuning(), not from
        Hz / (fs/4) and a raw resonance - see LadderTuning;
      * the feedback term passes through limitFeedback() - see there.
    At zero resonance neither applies, and the output is bit-identical to
    SpaceDub's arithmetic; a test holds it to that.

    DC gain is unity at zero resonance and falls as 1 / (1 + fb) as the
    resonance rises - the Moog's bass loss, kept because it is the sound. */
class MoogLadder
{
public:
	void reset ();

	/** coeff and resonance in SpaceDub's own terms. */
	double tick (double input, double coeff, double resonance);

	double lastOutput () const { return mOut4; }

private:
	double mIn1 = 0.0, mIn2 = 0.0, mIn3 = 0.0, mIn4 = 0.0;
	double mOut1 = 0.0, mOut2 = 0.0, mOut3 = 0.0, mOut4 = 0.0;
};

//------------------------------------------------------------------------
/** THE DIODES IN A LOOP: solves  (1 - b) v + eps sinh (v / Vd) = a
    for v - the voltage across the diodes when the drive into them is
    itself a function of v (a + b v, b < 1), as it is inside a
    zero-delay loop. b = 0 is the plain clipper, ms20DiodeClip(). `knee`
    picks the diodes (kMs20DiodeKnee or kMs20LateDiodeKnee); the
    sharpness is kMs20DiodeSharpness for both.

    The left side is odd, increasing and convex for v > 0, so the root is
    unique, and Newton started ABOVE it - at the smaller of the
    diodes-off answer a / (1 - b) and the diodes-only answer Vd asinh
    (a / eps), both of which are - falls monotonically onto it. A few
    iterations; never more than 16, no allocation. */
double ms20DiodeSolve (double a, double b, double knee);

/** The back-to-back diode clipper on its own: the v for a drive x. */
inline double ms20DiodeClip (double x, double knee) { return ms20DiodeSolve (x, 0.0, knee); }

/** THE MS-20 FILTER'S PREWARPED GAIN for a cutoff: g = tan (pi fc / fs).
    The structure is topology-preserving (zero-delay feedback), i.e. the
    bilinear transform of the analogue circuit with the cutoff prewarped,
    so the corner lands EXACTLY on the knob at every rate and the
    self-oscillation threshold is exactly k = 2 at every cutoff - no
    tuning table needed, unlike the ladder. */
inline double ms20Prewarp (double cutoffHz, double sampleRate)
{
	constexpr double kPiLocal = 3.14159265358979323846;
	return std::tan (kPiLocal * cutoffHz / sampleRate);
}

/** The MS-20 filters' feedback k for a resonance in LOOP GAIN: each
    self-oscillates when k reaches its damping constant, so k = damping x
    loop gain - and 1.0 on the shared scale is the threshold for both, as
    for the ladder. ms20Feedback() is the OTA (MS-20 II) one;
    korg35Feedback() the early one. */
inline double ms20Feedback (double resonanceK)
{
	return kOtaDamping * resonanceK;
}

inline double korg35Feedback (double resonanceK)
{
	return kKorg35Damping * resonanceK;
}

/** THE MS-20 FILTERS' LINEAR RESPONSE: |H| at `w` radians/sample. The
    analogue prototype, normalised to the cutoff, is

        H(s) = 1 / (s^2 + (damping - k) s + 1)

    with damping kOtaDamping (2, MS-20 II) or kKorg35Damping (7/3,
    MS-20 I), and the digital filters are exactly its bilinear transform
    with s = j tan (w / 2) / g. Unity at DC whatever the resonance - no
    bass loss. The tests hold it to both measured filters. At k >=
    damping the peak is unbounded: the filter is an oscillator and its
    diodes set the level. */
double ms20ResponseGain (double w, double g, double k, double damping = kOtaDamping);

/** THE EARLY MS-20 FILTER (Korg35): a Sallen-Key low-pass with its
    diode pair in the FORWARD path - see the MS-20 DIODES note above.

        x -> LP1 -> (+) -> LP2 --p--> diodes --v--+--> out
                     ^                             |
                     +---- HP <-- (k v - c p) <----+

    Three trapezoidal one-poles with zero-delay feedback (Zavalishin, "The
    Art of VA Filter Design"). p is LP2's output, the drive into the gain
    element; v is the gain element's output across its diodes - the
    filter's output AND the source of the feedback, so the clipping is in
    everything heard and in the resonance alike. The term c p, c = 7/3 - 2,
    is the Sallen-Key's first stage loaded by the second: it makes the
    damping kKorg35Damping and the threshold k = 7/3.

    Each sample is solved EXACTLY. With G = g/(1+g), a = 1 - G, LP1's
    output y1 and the states s2 (LP2), s3 (the HP's):

        p (1 + G a c) = A + B v,   A = G y1 - G a s3 + a s2,  B = G a k
        p = v + eps sinh (v / Vd)                         (the diodes)

    so  (1 - B') v + eps sinh (v / Vd) = A'  with A' = A / (1 + G a c),
    B' = B / (1 + G a c) <= 7/3 x 1/4 < 1 - ms20DiodeSolve()'s equation.
    Then u = y1 + a (k v - c p - s3) and the states advance. With the
    diodes off this is the linear filter ms20ResponseGain (..,
    kKorg35Damping) describes; the output is normalised to unity at DC.

    NOT MODELLED: the Korg35's transistors also make its resonance
    asymmetric (stronger on the negative half-cycle); this model is
    symmetric. */
class Ms20Filter
{
public:
	void reset () { mS1 = mS2 = mS3 = 0.0; }

	/** `g` from ms20Prewarp(), `k` from korg35Feedback(). */
	double tick (double input, double g, double k);

private:
	double mS1 = 0.0, mS2 = 0.0, mS3 = 0.0;
};

/** The input OTA's transfer, scaled - see kOtaDrive. Slope 1 at 0. */
inline double otaSaturate (double x)
{
	return std::tanh (kOtaDrive * x) / kOtaDrive;
}

/** THE LATE MS-20 FILTER (LM13600 OTA revision): two buffered
    first-order sections with positive feedback, and STRINGS OF THREE
    back-to-back diodes in the FEEDBACK path's gain element - see the
    MS-20 DIODES note above.

        x -> OTA -> LP1 -> (+) -> LP2 -----------------> out
                            ^              |
                            +-- HP <-- diodes <-- k <--+

    The diodes sit inside the zero-delay loop and are solved EXACTLY each
    sample: the drive into them is

        k LP2 = A + B v,  A = k (G y1 - G a s3 + a s2),  B = k G a

    which ms20DiodeSolve() solves (B <= 2.2 / 4 < 1); then u = y1 +
    a (v - s3), the HP is driven by v, and the states advance. The output
    is LP2, before the diodes, so only the RESONANCE is bent: a loud note
    well below the cutoff passes nearly clean (tested), one at the cutoff
    does not. Damping kOtaDamping (2): self-oscillation at k = 2.

    The input OTA (otaSaturate) is the one other non-linearity modelled;
    the OTAs inside the loop are taken as linear. */
class Ms20LateFilter
{
public:
	void reset () { mS1 = mS2 = mS3 = 0.0; }

	/** `g` from ms20Prewarp(), `k` from ms20Feedback(). */
	double tick (double input, double g, double k);

private:
	double mS1 = 0.0, mS2 = 0.0, mS3 = 0.0;
};

/** traceFilterResponse() for ANY filter: the Moog branch is the function
    above; the two MS-20 branches the same clamp, through
    ms20ResponseGain() with each one's own damping and feedback scaling
    - korg35Feedback / kKorg35Damping for MS-20 I, ms20Feedback /
    kOtaDamping for MS-20 II. */
void traceFilterResponse (FilterType type, double cutoffHz, double resonanceK, double sampleRate,
                          double minHz, double maxHz, float* dbOut, int count);

//------------------------------------------------------------------------
/** The envelope display's data: both ADSRs through one simulated note,
    ON A SHARED TIME AXIS, key held and then released at the same moment
    for both - which is how they are heard.

    Returns the span in seconds; `gateOffSeconds` receives when the key
    went up. The curves are 0..1. Drives real Adsr objects, so the picture
    is the envelope and not a drawing of one. */
double traceEnvelopes (const AdsrSpec& vcf, const AdsrSpec& vca,
                       float* vcfOut, float* vcaOut, int count,
                       double* gateOffSeconds);

/** The LFO display's data: `cycles` cycles of one output, -1..+1, from
    the same lfoOutputValue() the MsLfo runs. */
void traceLfo (LfoOutput output, double shape, float* out, int count, double cycles);

//------------------------------------------------------------------------
class R2SynthDsp
{
public:
	R2SynthDsp ();

	/** Recomputes EVERY rate-dependent coefficient and resets state.
	    Call it from setupProcessing, never from process(). */
	void setSampleRate (double sampleRate);
	double sampleRate () const { return mSampleRate; }

	/** The largest block the host will ask for. Called from
	    setupProcessing only; nothing in render() allocates. */
	void setMaxBlockSize (int maxSamples);
	int maxBlockSize () const { return mMaxBlockSize; }

	/** Silence, envelopes idle, notes forgotten, smoothers snapped. */
	void reset ();

	//--------------------------------------------------------------------
	// Parameters, in INTERNAL units - the parameter table's toInternal().
	//--------------------------------------------------------------------
	void setOutputLevelDb (double db);
	double outputLevelDb () const { return mOutputDb; }

	void setVcoWave (int vco, Vco::Wave wave);
	void setVcoTune (int vco, double semitones);
	void setVcoLevel (int vco, double level);          // 0..1

	void setLfoRate (double hz);
	void setLfoShape (double shape);                   // -1..+1
	void setLfoOutput (LfoOutput output);
	void setLfoPitchDepth (double semitones);
	void setLfoCutoffDepth (double octaves);
	void setLfoVcaDepth (double depth);                // 0..1
	void setLfoResonanceDepth (double loopGain);       // 0..kMaxLfoResonanceK

	void setLfo2Rate (double hz);
	void setLfo2Level (double level);                  // 0..1

	void setCutoffHz (double hz);
	void setResonance (double loopGain);               // 0..kMaxResonanceK
	void setKeyTrack (double amount);                  // 0..1
	void setVcfEnvAmount (double octaves);             // signed
	void setVcfEnvelope (const AdsrSpec& spec);
	void setVcaEnvelope (const AdsrSpec& spec);

	/** Moog ladder, MS-20 I or MS-20 II - see FilterType. A change
	    cross-fades over kFilterCrossfadeSeconds; reset() lands on it at
	    once. */
	void setFilterType (FilterType type);
	FilterType filterType () const { return mFilterType; }

	/** How much of a filter is in the output: 1 for the chosen one once
	    settled, 0 for the others; in between while cross-fading. The
	    three always sum to 1. */
	double filterWeight (FilterType type) const;

	/** The glide time, in seconds - see portamentoActive(). A change
	    applies from the NEXT note; a slide already under way finishes at
	    the time it started with. */
	void setPortamento (double seconds);

	//--------------------------------------------------------------------
	// Notes - MONOPHONIC, LAST-NOTE PRIORITY
	//
	// Every new note retriggers both envelopes. Releasing the sounding
	// note falls back to the most recent one still held, at its pitch,
	// WITHOUT retriggering; the gate only closes when no key is down.
	//--------------------------------------------------------------------
	void noteOn (int pitch, float velocity);
	void noteOff (int pitch);
	void allNotesOff ();

	/** The note the voice is playing, or -1 before the first one. */
	int currentNote () const { return mCurrentNote; }

	/** The pitch the VCOs are at, in (fractional) MIDI notes, before
	    Tune and vibrato - currentNote() itself once a slide has arrived.
	    For the tests. */
	double currentPitch () const { return mPitch; }
	bool gliding () const { return mGlideSamples > 0; }
	bool gateOpen () const { return mHeldCount > 0; }
	bool voiceActive () const { return mVcaEnv.active (); }

	/** Render one block. Tolerates numSamples <= 0 and null pointers. */
	void render (float* left, float* right, int numSamples);

	/** The output stage on its own, in place. Bit-identical at 0 dB with
	    the smoother settled - the tests hold it to that. */
	void applyOutputLevel (float* left, float* right, int numSamples);
	float currentOutputGain () const { return mOutput.value (); }

	/** How long after the last key-up the voice can still be heard:
	    the VCA release, converted by releaseTailSeconds(). */
	double tailSeconds () const;

	/** The cutoff the filter used on the last sample, in Hz - for the
	    tests' measurements of the modulation, not for the panel. */
	double lastCutoffHz () const { return mLastCutoffHz; }

	/** The Shape LFO 1 ran at on the last sample - the knob swept by
	    LFO 2 - for the tests. */
	double lastLfoShape () const { return mLastLfoShape; }

	/** The loop gain the filter was handed on the last sample - the
	    knob moved by LFO 1 - for the tests. */
	double lastResonance () const { return mLastResonance; }

private:
	float renderSample ();

	double mSampleRate   = 44100.0;
	int    mMaxBlockSize = 0;
	double mOutputDb     = 0.0;

	Vco   mVco[kNumVcos];
	MsLfo mLfo;
	SineLfo mLfo2;
	MoogLadder mFilter;
	Ms20Filter mMs20;
	Ms20LateFilter mMs20Late;
	Adsr  mVcfEnv;
	Adsr  mVcaEnv;

	// Smoothed controls. Tune is smoothed too: a knob move is otherwise
	// a pitch step, and a pitch step through a resonant filter is a
	// click.
	Smoother mVcoLevel[kNumVcos];
	Smoother mVcoTune[kNumVcos];
	Smoother mCutoffOct;        // log2 (Hz) - smoothed in octaves, not Hz
	Smoother mResonance;
	Smoother mLfoPitch;
	Smoother mLfoCutoff;
	Smoother mLfoVca;
	Smoother mLfoRes;
	Smoother mLfo2Level;
	Smoother mEnvAmount;
	Smoother mOutput;

	double mKeyTrack = 0.5;
	double mLfoShapeKnob = 0.0;     // the Shape knob; LFO 2 sweeps around it
	double mLastLfoShape = 0.0;
	double mLastResonance = 0.0;
	std::uint32_t mNoiseState = 0x12345678u;
	double mLastCutoffHz = 1000.0;

	// The VCF Type and its cross-fade: one WEIGHT per filter, indexed by
	// FilterType. A change moves every weight in a straight line from
	// where it is to its target (1 for the chosen filter, 0 for the
	// others) in the same number of samples, so the three always sum to
	// 1 - even when a switch arrives mid-fade - and the last step lands
	// EXACTLY. Settled, one weight is exactly 1: that filter alone runs,
	// and a settled Moog is the ladder to the bit. A filter whose weight
	// reaches 0 is reset, so a later switch fades it in from silence
	// rather than from stale state.
	void settleFilterWeights ();

	FilterType mFilterType = FilterType::Moog;
	double mFilterWeight[kNumFilterTypes]     = { 1.0, 0.0, 0.0 };
	double mFilterWeightStep[kNumFilterTypes] = { 0.0, 0.0, 0.0 };
	int    mFilterFadeSamples = 0;

	// The held-note stack, oldest first. Fixed size: sixteen held keys is
	// more than two hands, and a seventeenth drops the OLDEST.
	static constexpr int kMaxHeld = 16;
	int mHeld[kMaxHeld] = {};
	int mHeldCount   = 0;
	int mCurrentNote = -1;

	/** Moves the pitch towards mCurrentNote: a slide when portamento is
	    on and there was a note before, a jump otherwise. */
	void startPitchTo (int note);

	// Portamento. mPitch is where the VCOs are; it is held between notes,
	// and only reset() forgets it (mHavePitch false), so the first note
	// after a reset starts in tune rather than sliding up from middle C.
	double mPortamentoSeconds = kMinPortamentoSeconds;
	double mPitch      = 60.0;
	double mGlideStep  = 0.0;   // semitones a sample
	int    mGlideSamples = 0;   // samples left; 0 = arrived
	bool   mHavePitch  = false;
};

//------------------------------------------------------------------------
} // namespace R2Synth
