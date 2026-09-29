//------------------------------------------------------------------------
// SimpleSynth - parameter definitions
//
// THREE RANGES PER PARAMETER. The shape is inherited from the DXi ports
// in this folder and is kept on purpose:
//
//   normalised   0..1, what VST3 automates and what the host stores
//   plain        what the USER sees - the number on the panel and in the
//                host's own generic editor
//   internal     what the DSP is handed, via toInternal()
//
// MOSTLY INTERNAL == PLAIN, and where it is not it is for a reason the
// row in SimpleSynthParams.cpp gives:
//
//   * the per-cent levels and depths are handed over as 0..1;
//   * the ADSR times are shown in ms and handed over in SECONDS;
//   * Resonance is shown in per cent and handed over as LOOP GAIN,
//     0..kMaxResonanceK, where 1.0 is self-oscillation at every cutoff;
//   * LFO Shape is shown -100..+100 % and handed over as -1..+1;
//   * LFO2 Level is shown 0..100 % and handed over as 0..1 - how far
//     LFO 2 sweeps the Shape, in units of kMaxLfo2ShapeSweep;
//   * VCF LFO Res is shown 0..100 % OF THE RESONANCE KNOB and handed
//     over as loop gain, 0..kMaxResonanceK - the same scale as
//     Resonance, so 20 % of depth swings exactly 20 % of the knob.
//
// Note what is NOT a range mapping: dB to linear gain, and Hz to the
// ladder's coefficient. Those are the DSP's arithmetic and live in
// SimpleSynthDsp.h, where the editor can call the same copy of them.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/vst/vsttypes.h"

#include <cmath>

namespace SimpleSynth {

//------------------------------------------------------------------------
/** THE IDS. APPEND, NEVER INSERT - see below.

    kOutputLevel is id 0 because it is the scaffold's Output Trim,
    renamed and re-ranged before any build existed; everything else was
    appended after it in signal order. The PANEL order is the signal
    order too, so here the two happen to agree. */
enum Param : Steinberg::Vst::ParamID
{
	kOutputLevel = 0,

	kVco1Wave,
	kVco1Tune,
	kVco1Level,
	kVco2Wave,
	kVco2Tune,
	kVco2Level,
	kVco3Wave,
	kVco3Tune,
	kVco3Level,

	kLfoRate,
	kLfoShape,
	kLfoOutput,
	kLfoPitch,

	kCutoff,
	kResonance,
	kKeyTrack,
	kVcfLfo,
	kVcfEnvAmount,
	kVcfAttack,
	kVcfDecay,
	kVcfSustain,
	kVcfRelease,

	kVcaLfo,
	kVcaAttack,
	kVcaDecay,
	kVcaSustain,
	kVcaRelease,

	// LFO 2, appended on 29 Sep 2026: a sine whose output sweeps LFO 1's
	// Shape. At the END, per the rule below, even though the panel puts
	// it next to LFO 1.
	kLfo2Rate,
	kLfo2Level,

	// LFO 1 -> resonance depth, appended on 29 Sep 2026. On the panel it
	// sits in the VCF group next to the cutoff depth; here, at the end.
	kVcfLfoRes,

	kNumParams,

	//--------------------------------------------------------------------
	/** Standard VST3 bypass.

	    1000, DELIBERATELY FAR PAST THE END OF THE TABLE. kParams is
	    indexed by ParamID, and every id that reaches an index must be
	    range-checked first - paramDef() below is the only place that
	    indexing happens. An unchecked kParams[kBypass] reads 972 entries
	    past the end.

	    IDs 1001 upwards are the space for READ-ONLY OUTPUT parameters -
	    the mechanism for a value the DSP computes per block that the
	    panel needs. A sendMessage from process() is silently discarded
	    by the host's connection proxy (see SimpleSynthIDs.h), whereas
	    data.outputParameterChanges is delivered. kLfoShapeLive is the
	    first. */
	kBypass = 1000,

	//--------------------------------------------------------------------
	/** READ-ONLY OUTPUT: the Shape LFO 1 is running at right now - the
	    knob swept by LFO 2 - published by the processor so the panel's
	    LFO display can animate with it. Hidden and read-only, so a host
	    never shows or automates it; NOT in the table and NOT in the
	    state stream. Same normalisation as kLfoShape (0 = -100 %,
	    1 = +100 %), so paramDef (kLfoShape).toInternal() reads it. */
	kLfoShapeLive = 1001
};

/** The three VCOs' ids are the same three parameters at a stride of
    three. The processor and the panel both address them this way, so
    the three are one implementation and cannot drift. */
constexpr int kVcoParamStride = kVco2Wave - kVco1Wave;
static_assert (kVco3Wave - kVco2Wave == kVcoParamStride, "the three VCOs must share one stride");

inline Steinberg::Vst::ParamID vcoParam (int vco, Param vco1Param)
{
	return static_cast<Steinberg::Vst::ParamID> (vco1Param + vco * kVcoParamStride);
}

/** The two ADSRs likewise: A, D, S, R in the same order at a fixed
    distance, so one function lays out both on the panel. */
constexpr int kEnvParamStride = kVcaAttack - kVcfAttack;
static_assert (kVcaRelease - kVcfRelease == kEnvParamStride, "the two ADSRs must share one stride");

//------------------------------------------------------------------------
/** APPEND, NEVER INSERT.

    A parameter added in the middle renumbers every id after it, and a
    project saved by the old build then restores its values into the
    wrong parameters - quietly, with no error anywhere. New parameters go
    immediately before kNumParams, and a parameter that is no longer
    wanted is left in place and hidden rather than removed.

    (The ids above were laid out in one go before any build existed, so
    nothing has been saved against them yet. From the first build on,
    this rule holds without exception.) */
constexpr int kFirstFreeParamSlot = kNumParams;

/** The table's length as a plain int, for loops - comparing an int
    counter with the unsigned enum is a sign-compare warning on every
    loop otherwise. */
constexpr int kParamCount = static_cast<int> (kNumParams);

//------------------------------------------------------------------------
enum class ParamType
{
	/** Linear between plainMin and plainMax. */
	Float,

	/** LOGARITHMIC between plainMin and plainMax, both of which must be
	    greater than zero. The ear is logarithmic: a cutoff swept linearly
	    from 20 Hz to 20 kHz spends its first two per cent of travel on
	    the bottom five octaves, and an attack from 0.5 ms to 10 s
	    linearly has every percussive value in the first thousandth. */
	Log,

	Bool,

	/** A switch between named positions - stepCount + 1 of them. The
	    names are in the controller and the panel, from enumName(). */
	Enum
};

//------------------------------------------------------------------------
struct ParamDef
{
	Steinberg::Vst::ParamID id;
	const char* title;          // shown by the host - NEVER null
	const char* units;          // likewise
	ParamType   type;
	double      plainMin;       // what the user sees
	double      plainMax;
	double      plainDefault;
	double      internalMin;    // what the DSP is handed
	double      internalMax;
	int         stepCount;      // 0 = continuous
	bool        smoothed;       // interpolated per sample by the DSP

	//--------------------------------------------------------------------
	double toPlain (double normalized) const
	{
		if (type == ParamType::Log)
			return plainMin * std::pow (plainMax / plainMin, normalized);

		const double v = plainMin + normalized * (plainMax - plainMin);
		if (type == ParamType::Enum)
			return std::floor (v + 0.5);
		return v;
	}

	double toNormalized (double plain) const
	{
		if (type == ParamType::Log)
		{
			if (plain <= plainMin) return 0.0;
			if (plain >= plainMax) return 1.0;
			return std::log (plain / plainMin) / std::log (plainMax / plainMin);
		}

		if (plainMax == plainMin)
			return 0.0;
		return (plain - plainMin) / (plainMax - plainMin);
	}

	/** The DXi's ParamInfo::MapToInternal, generalised.

	    A LOG PARAMETER MAPS LOGARITHMICALLY ON BOTH SCALES, which is what
	    makes the millisecond parameters come out at exactly plain / 1000:
	    the internal range is the plain one scaled by a constant, and an
	    exponential map commutes with that. */
	double toInternal (double normalized) const
	{
		if (type == ParamType::Bool)
			return (normalized < 0.5) ? 0.0 : 1.0;

		if (type == ParamType::Log)
			return internalMin * std::pow (internalMax / internalMin, normalized);

		const double v = internalMin + normalized * (internalMax - internalMin);
		if (type == ParamType::Enum)
			return std::floor (v + 0.5);
		return v;
	}

	double defaultNormalized () const { return toNormalized (plainDefault); }
};

//------------------------------------------------------------------------
extern const ParamDef kParams[kNumParams];

/** ids are dense from 0 to kNumParams. */
inline bool isTableParam (Steinberg::Vst::ParamID id)
{
	return id < static_cast<Steinberg::Vst::ParamID> (kNumParams);
}

/** THE ONLY PLACE kParams IS INDEXED, and therefore the only place the
    range check has to be right. Anything outside the table - kBypass, an
    output parameter, a garbled id - gets entry 0 rather than a read off
    the end; callers that must tell the two apart test isTableParam(). */
inline const ParamDef& paramDef (Steinberg::Vst::ParamID id)
{
	return kParams[isTableParam (id) ? id : 0];
}

/** The position names of an Enum parameter. ONE LIST, read by the
    controller's toString and by the panel's selectors, so the host's
    automation lane and the panel say the same word. Returns "" for a
    position that has no name. */
const char* enumName (Steinberg::Vst::ParamID id, int position);

//------------------------------------------------------------------------
} // namespace SimpleSynth
