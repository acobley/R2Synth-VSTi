//------------------------------------------------------------------------
// SimpleSynth - the panel's text, implementation
//------------------------------------------------------------------------

#include "SimpleSynthReadouts.h"
#include "SimpleSynthDsp.h"

#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace Steinberg::Vst;

namespace SimpleSynth {

//------------------------------------------------------------------------
std::string shortLabel (ParamID tag)
{
	std::string label = paramDef (tag).title;

	// The box says which section; the control says what it is.
	for (const char* prefix : { "VCO1 ", "VCO2 ", "VCO3 ", "LFO ", "LFO2 ", "VCF ", "VCA " })
	{
		const size_t n = std::strlen (prefix);
		if (label.compare (0, n, prefix) == 0)
		{
			label.erase (0, n);
			break;
		}
	}

	// "Output Level" stays whole: in a box called VCA / OUTPUT, a bare
	// "Level" next to the VCA's LFO depth could be read as the VCA's.
	return label;
}

//------------------------------------------------------------------------
std::string selectorText (ParamID tag, int position)
{
	return enumName (tag, position);
}

//------------------------------------------------------------------------
std::string readoutText (ParamID tag, double normalized)
{
	if (!isTableParam (tag))
		return std::string ();

	const ParamDef& def = paramDef (tag);
	const double plain = def.toPlain (normalized);

	char text[64] = {};

	switch (tag)
	{
		case kOutputLevel:
			std::snprintf (text, sizeof (text), "%+.1f dB", plain);
			break;

		case kVco1Tune: case kVco2Tune: case kVco3Tune:
			// Two decimals: a tune of +0.07 st is a deliberate beat, and
			// "+0.1" would hide the difference between two of them.
			std::snprintf (text, sizeof (text), "%+.2f st", plain);
			break;

		case kVco1Level: case kVco2Level: case kVco3Level:
			// AT ZERO IT SAYS SO IN WORDS, because a VCO at 0 % is a VCO
			// that is off, and "0 %" is easy to misread as "at the
			// bottom of a range that goes somewhere".
			if (plain < 0.5)
				std::snprintf (text, sizeof (text), "off");
			else
				std::snprintf (text, sizeof (text), "%.0f %%", plain);
			break;

		case kLfoRate:
		case kLfo2Rate:
			std::snprintf (text, sizeof (text), (plain < 10.0) ? "%.2f Hz" : "%.1f Hz", plain);
			break;

		case kLfoShape:
			// THE THREE NAMED POINTS IN WORDS, the rest as a per cent.
			// The per cent is how far the peak has slid from the middle
			// of the cycle, which is what the knob does.
			if (plain <= -99.5)
				std::snprintf (text, sizeof (text), "falling saw");
			else if (plain >= 99.5)
				std::snprintf (text, sizeof (text), "rising saw");
			else if (std::fabs (plain) < 0.5)
				std::snprintf (text, sizeof (text), "triangle");
			else
				std::snprintf (text, sizeof (text), "%+.0f %%", plain);
			break;

		case kLfoPitch:
			std::snprintf (text, sizeof (text), (plain < 0.005) ? "off" : "%.2f st", plain);
			break;

		case kCutoff:
			// Hz below 1 k, kHz above - the same rule as the host's own
			// generic editor, because it is the same rule in
			// SimpleSynthController.cpp's toString.
			if (plain >= 1000.0)
				std::snprintf (text, sizeof (text), "%.2fk", plain / 1000.0);
			else
				std::snprintf (text, sizeof (text), "%.0f Hz", plain);
			break;

		case kResonance:
			// PER CENT AND LOOP GAIN. The per cent is the knob; K is the
			// number that means something - 1.00 is where it sings, at
			// every cutoff, and the lamp lights from the same predicate.
			std::snprintf (text, sizeof (text), "%.0f%%  K%.2f", plain, def.toInternal (normalized));
			break;

		case kVcfLfo:
			std::snprintf (text, sizeof (text), (plain < 0.005) ? "off" : "%.2f oct", plain);
			break;

		case kVcfLfoRes:
			// +/- because it swings both ways round the Resonance knob.
			std::snprintf (text, sizeof (text), (plain < 0.5) ? "off" : "+/-%.0f %%", plain);
			break;

		case kVcfEnvAmount:
			// SIGNED, EXPLICITLY: the negative half sweeps downwards,
			// and a sign swallowed would make the two halves read alike.
			std::snprintf (text, sizeof (text), "%+.1f oct", plain);
			break;

		case kVcaLfo:
		case kLfo2Level:
			std::snprintf (text, sizeof (text), (plain < 0.5) ? "off" : "%.0f %%", plain);
			break;

		case kPortamento:
			// "off" at the bottom - from the DSP's own predicate - then
			// the envelope times' style.
			if (!portamentoActive (def.toInternal (normalized)))
			{
				std::snprintf (text, sizeof (text), "off");
				break;
			}
			[[fallthrough]];
		default:
			if (def.type == ParamType::Log && std::strcmp (def.units, "ms") == 0)
			{
				// Four decades: the decimals follow the magnitude.
				if (plain >= 1000.0)
					std::snprintf (text, sizeof (text), "%.2f s", plain / 1000.0);
				else if (plain < 10.0)
					std::snprintf (text, sizeof (text), "%.2f ms", plain);
				else if (plain < 100.0)
					std::snprintf (text, sizeof (text), "%.1f ms", plain);
				else
					std::snprintf (text, sizeof (text), "%.0f ms", plain);
			}
			else
			{
				std::snprintf (text, sizeof (text), "%.0f %s", plain, def.units);
			}
			break;
	}

	return std::string (text);
}

//------------------------------------------------------------------------
std::string lfoLineText (const std::function<double (ParamID)>& internalOf)
{
	// WHAT THE LFO IS DOING, in one line: its period, which output is
	// patched, and what the Shape knob is doing to THAT output - the
	// square's duty, or the sine shaper's balance - which is the one
	// thing about it nobody guesses.
	const double rate  = internalOf (kLfoRate);
	const double shape = internalOf (kLfoShape);
	const auto output  = static_cast<LfoOutput> (static_cast<int> (internalOf (kLfoOutput)));

	char detail[48] = {};
	switch (output)
	{
		case LfoOutput::Square:
			std::snprintf (detail, sizeof (detail), "Square out, duty %.0f%%", lfoSquareDuty (shape) * 100.0);
			break;
		case LfoOutput::Sine:
			std::snprintf (detail, sizeof (detail), "Sine out, balance %+.0f%%", shape * kMaxSineBalance * 100.0);
			break;
		case LfoOutput::Shape:
		default:
			std::snprintf (detail, sizeof (detail), "Shape out");
			break;
	}

	char text[160] = {};
	std::snprintf (text, sizeof (text), "LFO 1  %.0f ms/cycle, %s", 1000.0 / rate, detail);
	return std::string (text);
}

//------------------------------------------------------------------------
std::string lfoDepthsLineText (const std::function<double (ParamID)>& internalOf)
{
	char text[160] = {};
	// Resonance as +/- per cent of the knob, the way its own readout
	// says it - the depth is in the knob's units.
	std::snprintf (text, sizeof (text), "   ->  pitch %.2f st, cutoff %.2f oct, res +/-%.0f%%, VCA %.0f%%",
	               internalOf (kLfoPitch), internalOf (kVcfLfo),
	               internalOf (kVcfLfoRes) / kMaxResonanceK * 100.0, internalOf (kVcaLfo) * 100.0);
	return std::string (text);
}

//------------------------------------------------------------------------
std::string lfo2LineText (const std::function<double (ParamID)>& internalOf)
{
	// WHERE THE SHAPE ACTUALLY GOES: the knob +/- LFO 2, clamped - from
	// lfo2ShapeRange(), which calls the sweptLfoShape() the DSP runs. The
	// clamp is the part worth printing: from a knob off centre, full
	// Level spends part of each cycle parked at a sawtooth.
	const double rate  = internalOf (kLfo2Rate);
	const double level = internalOf (kLfo2Level);

	char text[160] = {};
	if (level < 0.005)
	{
		std::snprintf (text, sizeof (text), "LFO 2  %.0f ms/cycle, off - Shape stays on the knob",
		               1000.0 / rate);
	}
	else
	{
		double lo = 0.0, hi = 0.0;
		lfo2ShapeRange (internalOf (kLfoShape), level, lo, hi);
		std::snprintf (text, sizeof (text), "LFO 2  %.0f ms/cycle  ->  Shape %+.0f..%+.0f %%",
		               1000.0 / rate, lo * 100.0, hi * 100.0);
	}
	return std::string (text);
}

//------------------------------------------------------------------------
const char* lfoDisplayCaption (double lfo2Level)
{
	return (lfo2Level < 0.005) ? "LFO 1  (2 cycles)" : "LFO 1  (2 cycles, swept by LFO 2)";
}

//------------------------------------------------------------------------
const char* panelTitleText ()
{
	return "SimpleSynth   -   three VCOs, an MS-20 LFO swept by a sine, a Moog ladder and two ADSRs";
}

const char* panelFlowText ()
{
	return "VCO 1-3 -> mixer -> Moog ladder VCF -> VCA -> output      "
	       "(mono, last note wins; every new note retriggers both envelopes; lamp = self-oscillating)";
}

//------------------------------------------------------------------------
void lfoDisplayData (double outputPosition, double shape, float* patched, float* companion,
                     int count, double cycles, std::string& patchedLegend,
                     std::string& companionLegend)
{
	// -1..+1 into the middle 84 % of the plot, so a square at full swing
	// does not sit on the plate's frame; the view's scale is 0..1.
	constexpr double kSwing = 0.42;

	const auto output = static_cast<LfoOutput> (static_cast<int> (outputPosition + 0.5));
	const LfoOutput other = (output == LfoOutput::Shape) ? LfoOutput::Square : LfoOutput::Shape;

	// DRAWN BY traceLfo(), which calls the same lfoOutputValue() the
	// running LFO does - so the picture is the wave, not a drawing of it.
	traceLfo (output, shape, patched, count, cycles);
	traceLfo (other, shape, companion, count, cycles);
	for (int i = 0; i < count; ++i)
	{
		patched[i]   = static_cast<float> (0.5 + kSwing * patched[i]);
		companion[i] = static_cast<float> (0.5 + kSwing * companion[i]);
	}

	auto upper = [] (std::string s) { for (auto& c : s) c = static_cast<char> (std::toupper (c)); return s; };
	auto lower = [] (std::string s) { for (auto& c : s) c = static_cast<char> (std::tolower (c)); return s; };
	patchedLegend   = "> " + upper (enumName (kLfoOutput, static_cast<int> (output)));
	companionLegend = lower (enumName (kLfoOutput, static_cast<int> (other)));
}

//------------------------------------------------------------------------
std::string engineLineText (double sampleRate, double vcaReleaseSeconds)
{
	// The cutoff CEILING is a fraction of the sample rate, from
	// cutoffCeilingHz() - the same call the filter makes - so it is
	// worth showing next to the rate: at 44.1 k the top of the Cutoff
	// knob is not reachable, and that is a question people ask rather
	// than a bug.
	char text[160] = {};
	std::snprintf (text, sizeof (text),
	               "Engine: %.0f Hz    cutoff ceiling %.0f Hz    release tail %.2f s",
	               sampleRate, cutoffCeilingHz (sampleRate), releaseTailSeconds (vcaReleaseSeconds));
	return std::string (text);
}

//------------------------------------------------------------------------
std::string controlLabel (const Layout::Control& control)
{
	return (control.label != nullptr) ? std::string (control.label) : shortLabel (control.tag);
}

//------------------------------------------------------------------------
void filterDisplayData (double cutoffHz, double resonanceK, double sampleRate,
                        float* curve, int count, double& markerX, double& markerY)
{
	using namespace Layout;
	auto toY = [] (double db) {
		const double y = (db - kFilterBottomDb) / (kFilterTopDb - kFilterBottomDb);
		return (y < 0.0) ? 0.0 : (y > 1.0) ? 1.0 : y;
	};

	// AT LOOP GAIN 1 AND OVER the filter is an oscillator and the linear
	// response has no meaning past it (the loop is unstable; it is
	// limitFeedback() that sets the level). Drawn at the edge instead -
	// a peak far off the top of the screen - which is what it sounds
	// like: a sine at the cutoff.
	if (selfOscillating (resonanceK))
		resonanceK = 0.999;

	if (curve != nullptr && count > 0)
	{
		traceFilterResponse (cutoffHz, resonanceK, sampleRate, kFilterMinHz, kFilterMaxHz, curve, count);
		for (int i = 0; i < count; ++i)
			curve[i] = static_cast<float> (toY (curve[i]));
	}

	// The dot: at the cutoff the FILTER uses - clamped to the ceiling
	// exactly as process() clamps it - and on the curve there.
	if (sampleRate < 1000.0)
		sampleRate = 44100.0;
	double hz = cutoffHz;
	if (hz < kMinCutoffHz) hz = kMinCutoffHz;
	if (hz > cutoffCeilingHz (sampleRate)) hz = cutoffCeilingHz (sampleRate);
	float db = 0.f;
	traceFilterResponse (cutoffHz, resonanceK, sampleRate, hz, hz, &db, 1);
	markerX = std::log (hz / kFilterMinHz) / std::log (kFilterMaxHz / kFilterMinHz);
	markerY = toY (db);
}

const char* filterDisplayCaption (bool live)
{
	return live ? "live" : "at the knobs";
}

//------------------------------------------------------------------------
std::string spanText (double seconds)
{
	char text[32] = {};
	if (seconds >= 1.0)
		std::snprintf (text, sizeof (text), "%.2f s", seconds);
	else
		std::snprintf (text, sizeof (text), "%.0f ms", seconds * 1000.0);
	return text;
}

std::string envAmountText (double octaves)
{
	char text[32] = {};
	std::snprintf (text, sizeof (text), "%+.1f oct", octaves);
	return text;
}

std::string filterDisplayAnnotation (double cutoffNormalized, double resonanceNormalized)
{
	return readoutText (kCutoff, cutoffNormalized) + "   " + readoutText (kResonance, resonanceNormalized);
}

//------------------------------------------------------------------------
std::string modInfoLineText (const std::function<double (Steinberg::Vst::ParamID)>& internalOf)
{
	return lfoLineText (internalOf) + "        " + lfo2LineText (internalOf);
}

} // namespace SimpleSynth
