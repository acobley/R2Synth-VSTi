//------------------------------------------------------------------------
// SimpleSynth - parameter table
//
// Column order matches the DXi ports in this folder:
//   id, title, units, type, plain min/max/default, internal min/max,
//   step count, smoothed
//
// TITLE AND UNITS MUST NEVER BE NULL. RangeParameter hands both straight
// to UString::assign, which dereferences without a null check; the
// symptom is the SDK VALIDATOR segfaulting in the post-build step. Empty
// string, never null.
//
// THE TITLES CARRY THEIR SECTION ("VCO1 Tune", "VCF Attack") because a
// host's automation list has no sections - twelve parameters all called
// "Attack" would be useless there. The panel strips the prefix, because
// its group boxes already say which section a control is in.
//
// THE DEFAULT PATCH is a playable bass/lead: VCO 1 sawtooth, VCO 2 a
// square an octave down at half level, VCO 3 off; the filter half open
// with three octaves of envelope; no LFO. Its level is measured by the
// DSP suite - see measureDefaultLevel() in tests/DspTests.cpp.
//------------------------------------------------------------------------

#include "SimpleSynthParams.h"
#include "SimpleSynthDsp.h"

namespace SimpleSynth {

//------------------------------------------------------------------------
const ParamDef kParams[kNumParams] = {
//   id              title                units   type               pMin     pMax     pDef    iMin     iMax     steps smooth
	{kOutputLevel,  "Output Level",      "dB",   ParamType::Float,  -48.,    6.,      0.,     -48.,    6.,      0,    true },

	{kVco1Wave,     "VCO1 Wave",         "",     ParamType::Enum,   0.,      1.,      0.,     0.,      1.,      1,    false},
	{kVco1Tune,     "VCO1 Tune",         "st",   ParamType::Float,  -12.,    12.,     0.,     -12.,    12.,     0,    true },
	{kVco1Level,    "VCO1 Level",        "%",    ParamType::Float,  0.,      100.,    100.,   0.,      1.,      0,    true },
	{kVco2Wave,     "VCO2 Wave",         "",     ParamType::Enum,   0.,      1.,      1.,     0.,      1.,      1,    false},
	{kVco2Tune,     "VCO2 Tune",         "st",   ParamType::Float,  -12.,    12.,     -12.,   -12.,    12.,     0,    true },
	{kVco2Level,    "VCO2 Level",        "%",    ParamType::Float,  0.,      100.,    50.,    0.,      1.,      0,    true },
	{kVco3Wave,     "VCO3 Wave",         "",     ParamType::Enum,   0.,      1.,      0.,     0.,      1.,      1,    false},
	{kVco3Tune,     "VCO3 Tune",         "st",   ParamType::Float,  -12.,    12.,     0.,     -12.,    12.,     0,    true },
	{kVco3Level,    "VCO3 Level",        "%",    ParamType::Float,  0.,      100.,    0.,     0.,      1.,      0,    true },

	// The MS-20's MG ran from about 0.1 Hz to 20 Hz, and so does this.
	{kLfoRate,      "LFO Rate",          "Hz",   ParamType::Log,    0.1,     20.,     4.,     0.1,     20.,     0,    false},
	// -100 falling sawtooth, 0 triangle, +100 rising sawtooth.
	{kLfoShape,     "LFO Shape",         "%",    ParamType::Float,  -100.,   100.,    0.,     -1.,     1.,      0,    false},
	// 0 = shape, 1 = square, 2 = sine - LfoOutput's values. The Sine
	// position was added on 29 Sep 2026, before any release: it moved
	// Square from normalised 1.0 to 0.5, so a test project saved with
	// Square selected opens on Sine. From the first release on, the
	// positions are as permanent as the ids.
	{kLfoOutput,    "LFO Output",        "",     ParamType::Enum,   0.,      2.,      0.,     0.,      2.,      2,    false},
	{kLfoPitch,     "LFO Pitch",         "st",   ParamType::Float,  0.,      kMaxLfoPitchSemis, 0., 0., kMaxLfoPitchSemis, 0, true },

	{kCutoff,       "VCF Cutoff",        "Hz",   ParamType::Log,    20.,     20000.,  1200.,  20.,     20000.,  0,    true },
	// Per cent on the knob, LOOP GAIN to the DSP: 1.0 self-oscillates at
	// every cutoff, so the top ninth of the knob is the singing part.
	{kResonance,    "VCF Resonance",     "%",    ParamType::Float,  0.,      100.,    20.,    0.,      kMaxResonanceK, 0, true },
	{kKeyTrack,     "VCF Key Track",     "%",    ParamType::Float,  0.,      100.,    50.,    0.,      1.,      0,    false},
	{kVcfLfo,       "VCF LFO",           "oct",  ParamType::Float,  0.,      kMaxLfoCutoffOctaves, 0., 0., kMaxLfoCutoffOctaves, 0, true },
	{kVcfEnvAmount, "VCF Env Amount",    "oct",  ParamType::Float,  -kMaxEnvOctaves, kMaxEnvOctaves, 3., -kMaxEnvOctaves, kMaxEnvOctaves, 0, true },
	{kVcfAttack,    "VCF Attack",        "ms",   ParamType::Log,    0.5,     10000.,  5.,     0.0005,  10.,     0,    false},
	{kVcfDecay,     "VCF Decay",         "ms",   ParamType::Log,    1.,      10000.,  300.,   0.001,   10.,     0,    false},
	{kVcfSustain,   "VCF Sustain",       "%",    ParamType::Float,  0.,      100.,    30.,    0.,      1.,      0,    false},
	{kVcfRelease,   "VCF Release",       "ms",   ParamType::Log,    1.,      10000.,  300.,   0.001,   10.,     0,    false},

	{kVcaLfo,       "VCA LFO",           "%",    ParamType::Float,  0.,      100.,    0.,     0.,      1.,      0,    true },
	{kVcaAttack,    "VCA Attack",        "ms",   ParamType::Log,    0.5,     10000.,  3.,     0.0005,  10.,     0,    false},
	{kVcaDecay,     "VCA Decay",         "ms",   ParamType::Log,    1.,      10000.,  500.,   0.001,   10.,     0,    false},
	{kVcaSustain,   "VCA Sustain",       "%",    ParamType::Float,  0.,      100.,    80.,    0.,      1.,      0,    false},
	{kVcaRelease,   "VCA Release",       "ms",   ParamType::Log,    1.,      10000.,  250.,   0.001,   10.,     0,    false},

	// LFO 2 - a plain sine that sweeps LFO 1's Shape. Slower at the
	// bottom than LFO 1 (0.02 Hz, 50 s a cycle), because a shape that
	// drifts over a phrase is the usual use. Level 0 by default, so the
	// default patch is unchanged.
	{kLfo2Rate,     "LFO2 Rate",         "Hz",   ParamType::Log,    0.02,    20.,     0.5,    0.02,    20.,     0,    false},
	{kLfo2Level,    "LFO2 Level",        "%",    ParamType::Float,  0.,      100.,    0.,     0.,      1.,      0,    true },

	// LFO 1 on the resonance, BIPOLAR round the Resonance knob and in
	// the knob's own units: per cent of its travel on the panel, loop
	// gain to the DSP. Smoothed like every depth. 0 by default, so the
	// default patch is unchanged.
	{kVcfLfoRes,    "VCF LFO Res",       "%",    ParamType::Float,  0.,      100.,    0.,     0.,      kMaxResonanceK, 0, true },

	// PORTAMENTO: the slide from one note to the next, in ms - always, not
	// only when the keys overlap. Logarithmic like the envelope times,
	// 1 ms to 5 s; THE BOTTOM OF THE KNOB IS OFF (portamentoActive()), and
	// that is the default, so the default patch is unchanged. Not
	// smoothed: it is read once per note.
	{kPortamento,   "Portamento",        "ms",   ParamType::Log,    kMinPortamentoSeconds * 1000., kMaxPortamentoSeconds * 1000., kMinPortamentoSeconds * 1000., kMinPortamentoSeconds, kMaxPortamentoSeconds, 0, false},
};

//------------------------------------------------------------------------
const char* enumName (Steinberg::Vst::ParamID id, int position)
{
	switch (id)
	{
		case kVco1Wave:
		case kVco2Wave:
		case kVco3Wave:
			return (position == 0) ? "Saw" : (position == 1) ? "Square" : "";

		case kLfoOutput:
			// "Shape", not "Triangle": the output is whatever the Shape
			// knob makes it, and at either end that is a sawtooth.
			return (position == 0) ? "Shape" : (position == 1) ? "Square" : (position == 2) ? "Sine" : "";

		default:
			return "";
	}
}

//------------------------------------------------------------------------
} // namespace SimpleSynth
