//------------------------------------------------------------------------
// tools/dump-layout.cpp - the panel as JSON, for tools/render-panel.py
//
// WHY A C++ TOOL AND NOT A PARSER. Everything the picture needs - the
// positions, the boxes, the text on every control, the two displays'
// curves - is printed here by the SAME code the editor runs:
// SimpleSynthLayout.h for where things are, SimpleSynthReadouts.cpp for
// what they say, traceLfo() and traceEnvelopes() for the curves. The
// Python script only draws. So the picture cannot drift from the panel
// without this failing to compile first.
//
// The values are the parameter table's DEFAULTS, so docs/panel.png is the
// panel as it opens on a fresh instance at 48 k.
//
//   c++ -std=c++17 -I<sdk> -Isource tools/dump-layout.cpp
//       source/SimpleSynthReadouts.cpp source/SimpleSynthParams.cpp
//       source/SimpleSynthDsp.cpp -o /tmp/dump-layout
//------------------------------------------------------------------------

#include "SimpleSynthLayout.h"
#include "SimpleSynthReadouts.h"
#include "SimpleSynthDsp.h"

#include <cstdio>
#include <string>

using namespace SimpleSynth;
using namespace SimpleSynth::Layout;

namespace {

std::string quoted (const std::string& s)
{
	std::string out = "\"";
	for (char c : s)
	{
		if (c == '"' || c == '\\') out += '\\';
		out += c;
	}
	return out + "\"";
}

double internalDefault (Steinberg::Vst::ParamID id)
{
	return paramDef (id).toInternal (paramDef (id).defaultNormalized ());
}

void printCurve (const char* key, const float* v, int n, bool last)
{
	std::printf ("  %s: [", quoted (key).c_str ());
	for (int i = 0; i < n; ++i)
		std::printf ("%s%.4f", i ? "," : "", v[i]);
	std::printf ("]%s\n", last ? "" : ",");
}

} // anonymous namespace

int main ()
{
	const double sampleRate = 48000.0;

	std::printf ("{\n");
	std::printf ("  \"width\": %d, \"height\": %d,\n", kEditorWidth, kEditorHeight);
	std::printf ("  \"contentX\": %d, \"titleY\": %d, \"flowY\": %d, \"labelHeight\": %d,\n",
	             kContentX, kTitleY, kFlowY, kLabelHeight);

	std::printf ("  \"groups\": [\n");
	for (int i = 0; i < kNumGroups; ++i)
	{
		const Rect r = groupRect (kGroups[i]);
		std::printf ("    {\"title\": %s, \"rect\": [%d,%d,%d,%d]}%s\n", quoted (kGroups[i].title).c_str (),
		             r.left, r.top, r.right, r.bottom, (i + 1 < kNumGroups) ? "," : "");
	}
	std::printf ("  ],\n");

	std::printf ("  \"controls\": [\n");
	for (int i = 0; i < kNumControls; ++i)
	{
		const Control& c = kControls[i];
		const Rect r = controlRect (c);
		const double n = paramDef (c.tag).defaultNormalized ();
		const bool selector = (c.kind == Kind::Selector);
		const std::string text = selector ? selectorText (c.tag, static_cast<int> (internalDefault (c.tag)))
		                                  : readoutText (c.tag, n);
		const bool lamp = (c.tag == kResonance);
		std::printf ("    {\"kind\": %s, \"rect\": [%d,%d,%d,%d], \"value\": %.4f, \"text\": %s, \"label\": %s, "
		             "\"lamp\": %s, \"lit\": %s}%s\n",
		             selector ? "\"selector\"" : "\"slider\"", r.left, r.top, r.right, r.bottom, n,
		             quoted (text).c_str (), quoted (shortLabel (c.tag)).c_str (),
		             lamp ? "true" : "false",
		             (lamp && selfOscillating (internalDefault (kResonance))) ? "true" : "false",
		             (i + 1 < kNumControls) ? "," : "");
	}
	std::printf ("  ],\n");

	// The readout lines, from the same functions the editor calls.
	std::printf ("  \"readouts\": [\n");
	std::printf ("    {\"x\": %d, \"y\": %d, \"colour\": \"trace\", \"text\": %s},\n", kReadoutX, kReadout1Y,
	             quoted (lfoLineText (internalDefault)).c_str ());
	std::printf ("    {\"x\": %d, \"y\": %d, \"colour\": \"value\", \"text\": %s},\n", kReadoutX, kReadout2Y,
	             quoted (engineLineText (sampleRate, internalDefault (kVcaRelease))).c_str ());
	std::printf ("    {\"x\": %d, \"y\": %d, \"colour\": \"label\", \"text\": %s}\n", kReadoutX, kReadout3Y,
	             quoted ("Every new note retriggers both envelopes; letting go returns to the last key held.").c_str ());
	std::printf ("  ],\n");

	// The two displays, drawn by the plug-in's own trace functions.
	float a[kDisplayPoints], b[kDisplayPoints];
	traceLfo (internalDefault (kLfoShape), a, b, kDisplayPoints, 2.0);
	for (int i = 0; i < kDisplayPoints; ++i)
	{
		a[i] = 0.5f + 0.42f * a[i];
		b[i] = 0.5f + 0.42f * b[i];
	}
	const bool square = internalDefault (kLfoOutput) >= 0.5;
	std::printf ("  \"lfoView\": {\"rect\": [%d,%d,%d,%d], \"caption\": \"LFO  (2 cycles)\", \"annotation\": \"%.2f s\", "
	             "\"legend\": [%s, %s], \"gate\": -1,\n",
	             kDisplayX, kLfoViewTop, kDisplayX + kDisplayWidth, kLfoViewBottom, 2.0 / internalDefault (kLfoRate),
	             square ? "\"shape\"" : "\"> SHAPE\"", square ? "\"SQUARE <\"" : "\"square\"");
	printCurve ("a", a, kDisplayPoints, false);
	printCurve ("b", b, kDisplayPoints, true);
	std::printf ("  },\n");

	AdsrSpec vcf { internalDefault (kVcfAttack), internalDefault (kVcfDecay),
	               internalDefault (kVcfSustain), internalDefault (kVcfRelease) };
	AdsrSpec vca { internalDefault (kVcaAttack), internalDefault (kVcaDecay),
	               internalDefault (kVcaSustain), internalDefault (kVcaRelease) };
	double gateOff = 0.0;
	const double span = traceEnvelopes (vcf, vca, a, b, kDisplayPoints, &gateOff);
	char legend[48];
	std::snprintf (legend, sizeof (legend), "VCF ENV %+.1foct", internalDefault (kVcfEnvAmount));
	std::printf ("  \"envView\": {\"rect\": [%d,%d,%d,%d], \"caption\": \"ENVELOPES  (key up at the dots)\", "
	             "\"annotation\": \"%s\", \"legend\": [%s, \"VCA ENV\"], \"gate\": %.4f,\n",
	             kDisplayX, kEnvViewTop, kDisplayX + kDisplayWidth, kEnvViewBottom,
	             (span >= 1.0 ? (std::to_string (span).substr (0, 4) + " s").c_str ()
	                          : (std::to_string (static_cast<int> (span * 1000.0 + 0.5)) + " ms").c_str ()),
	             quoted (legend).c_str (), gateOff / span);
	printCurve ("a", a, kDisplayPoints, false);
	printCurve ("b", b, kDisplayPoints, true);
	std::printf ("  }\n");

	std::printf ("}\n");
	return 0;
}
