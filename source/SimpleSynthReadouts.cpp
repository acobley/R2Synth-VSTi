//------------------------------------------------------------------------
// SimpleSynth - the panel's text, implementation
//------------------------------------------------------------------------

#include "SimpleSynthReadouts.h"
#include "SimpleSynthDsp.h"

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
	for (const char* prefix : { "VCO1 ", "VCO2 ", "VCO3 ", "LFO ", "VCF ", "VCA " })
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
	return shortLabel (tag) + ": " + enumName (tag, position);
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

		case kVcfEnvAmount:
			// SIGNED, EXPLICITLY: the negative half sweeps downwards,
			// and a sign swallowed would make the two halves read alike.
			std::snprintf (text, sizeof (text), "%+.1f oct", plain);
			break;

		case kVcaLfo:
			std::snprintf (text, sizeof (text), (plain < 0.5) ? "off" : "%.0f %%", plain);
			break;

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
	// patched, and the square's duty - which follows the Shape knob, as
	// the MS-20's does, and is the one thing about it nobody guesses.
	const double rate  = internalOf (kLfoRate);
	const double shape = internalOf (kLfoShape);
	const bool square  = internalOf (kLfoOutput) >= 0.5;

	char text[160] = {};
	std::snprintf (text, sizeof (text),
	               "LFO %.0f ms/cycle, %s out, square duty %.0f%%  ->  pitch %.2f st, cutoff %.2f oct, VCA %.0f%%",
	               1000.0 / rate, square ? "Square" : "Shape",
	               lfoSquareDuty (shape) * 100.0,
	               internalOf (kLfoPitch), internalOf (kVcfLfo), internalOf (kVcaLfo) * 100.0);
	return std::string (text);
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

} // namespace SimpleSynth
