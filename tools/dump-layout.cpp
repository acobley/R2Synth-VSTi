//------------------------------------------------------------------------
// tools/dump-layout.cpp - the panel as JSON, for tools/render-panel.py
//
// WHY A C++ TOOL AND NOT A PARSER. Everything the picture needs - the
// positions, the sections, the text on every control, the four screens'
// curves - is printed here by the SAME code the editor runs:
// R2SynthLayout.h for where things are, R2SynthReadouts.cpp for
// what they say, traceFilterResponse(), traceLfo() and traceEnvelopes()
// for the curves. The Python script only draws. So the picture cannot
// drift from the panel without this failing to compile first.
//
// The values are the parameter table's DEFAULTS, so docs/panel.png is the
// panel as it opens on a fresh instance at 48 k.
//
// THE BLACK PANEL (PolivoksUI, 22 Oct 2026) adds what the lettering and
// the selectors need: the text styles, every glyph of the face PARSED
// (SciFiLettering.h - so the script strokes the same paths and keeps no
// copy of the table), each section's title box, and each selector's
// geometry and where its position names are printed.
//
//   c++ -std=c++17 -I<sdk> -Isource tools/dump-layout.cpp
//       source/R2SynthReadouts.cpp source/R2SynthParams.cpp
//       source/R2SynthDsp.cpp -o /tmp/dump-layout
//------------------------------------------------------------------------

#include "R2SynthLayout.h"
#include "R2SynthReadouts.h"
#include "R2SynthDsp.h"

#include <cstdio>
#include <cstdlib>
#include <string>

using namespace R2Synth;
using namespace R2Synth::Layout;

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

std::string rect (const Rect& r)
{
	char t[64];
	std::snprintf (t, sizeof (t), "[%d,%d,%d,%d]", r.left, r.top, r.right, r.bottom);
	return t;
}

const char* accentName (Accent a)
{
	switch (a)
	{
		case Accent::Osc:    return "\"osc\"";
		case Accent::Filter: return "\"filter\"";
		case Accent::Amp:    return "\"amp\"";
		case Accent::Mod:    return "\"mod\"";
	}
	return "\"osc\"";
}

const char* styleName (CaptionStyle s)
{
	switch (s)
	{
		case CaptionStyle::Block: return "\"block\"";
		case CaptionStyle::Field: return "\"field\"";
		case CaptionStyle::Note:  return "\"note\"";
		case CaptionStyle::Badge: return "\"badge\"";
	}
	return "\"note\"";
}

double internalDefault (Steinberg::Vst::ParamID id)
{
	return paramDef (id).toInternal (paramDef (id).defaultNormalized ());
}

std::string num (double v)
{
	char t[32];
	std::snprintf (t, sizeof (t), "%.3f", v);
	return t;
}

std::string style (const Lettering::Style& s)
{
	return "{\"cap\": " + num (s.cap) + ", \"weight\": " + num (s.weight) + ", \"track\": " + num (s.track) + "}";
}

std::string box (const Box& b)
{
	return "[" + num (b.left) + "," + num (b.top) + "," + num (b.right) + "," + num (b.bottom) + "]";
}

std::string commands (const char* path)
{
	std::string out = "[";
	bool first = true;
	for (const auto& c : Lettering::parse (path))
	{
		out += first ? "" : ",";
		first = false;
		out += "[\"" + std::string (1, c.op) + "\"," + num (c.x1) + "," + num (c.y1) + "," + num (c.x) + "," + num (c.y) + "]";
	}
	return out + "]";
}

std::string curve (const float* v, int n)
{
	std::string out = "[";
	char t[24];
	for (int i = 0; i < n; ++i)
	{
		std::snprintf (t, sizeof (t), "%s%.4f", i ? "," : "", v[i]);
		out += t;
	}
	return out + "]";
}

} // anonymous namespace

int main ()
{
	const double sampleRate = 48000.0;
	const int n = kDisplayPoints;

	std::printf ("{\n");
	std::printf ("  \"width\": %d, \"height\": %d, \"headerHeight\": %d,\n", kEditorWidth, kEditorHeight, kHeaderHeight);
	std::printf ("  \"sectionPadX\": %d, \"sectionPadTop\": %d, \"sectionTitleH\": %d, \"sectionRadius\": %d,\n",
	             kSectionPadX, kSectionPadTop, kSectionTitleH, kSectionRadius);
	std::printf ("  \"knobGap\": %d, \"knobValueH\": %d, \"knobLabelH\": %d,\n", kKnobGap, kKnobValueH, kKnobLabelH);
	std::printf ("  \"wordmark\": %s, \"engineLine\": {\"rect\": %s, \"text\": %s},\n", rect (kWordmark).c_str (),
	             rect (kEngineLine).c_str (), quoted (engineLineText (sampleRate, internalDefault (kVcaRelease))).c_str ());
	std::printf ("  \"badge\": %s, \"pillText\": \"Evaluation - 7 days left - activate...\",\n", rect (kBadge).c_str ());
	std::printf ("  \"headerDivider\": %d, \"screenBezel\": %d, \"badgeSize\": %d, \"selectorNameH\": %d,\n",
	             kHeaderDivider, kScreenBezel, kBadgeSize, kSelectorNameH);

	// ---- the lettering: the styles, and every glyph parsed -----------------
	std::printf ("  \"styles\": {\"wordmark\": %s, \"title\": %s, \"subtitle\": %s, \"label\": %s, \"value\": %s,\n"
	             "             \"block\": %s, \"note\": %s, \"engine\": %s, \"info\": %s, \"screen\": %s,\n"
	             "             \"badge\": %s, \"badgeSub\": %s, \"tick\": %s, \"pill\": %s},\n",
	             style (kTextWordmark).c_str (), style (kTextTitle).c_str (), style (kTextSubtitle).c_str (),
	             style (kTextLabel).c_str (), style (kTextValue).c_str (), style (kTextBlock).c_str (),
	             style (kTextNote).c_str (), style (kTextEngine).c_str (), style (kTextInfo).c_str (),
	             style (kTextScreen).c_str (), style (kTextBadge).c_str (), style (kTextBadgeSub).c_str (),
	             style (kTextTick).c_str (), style (kTextPill).c_str ());
	std::printf ("  \"space\": %s,\n  \"glyphs\": {\n", num (Lettering::kSpace).c_str ());
	for (uint32_t c = 1; c < 0x2600; ++c)
		if (const Lettering::Glyph* g = Lettering::glyphFor (c))
			std::printf ("    \"%u\": [%s, %s],\n", c, num (g->advance).c_str (), commands (g->path).c_str ());
	std::printf ("    \"missing\": [%s, %s]\n  },\n", num (Lettering::missingGlyph ().advance).c_str (),
	             commands (Lettering::missingGlyph ().path).c_str ());

	std::printf ("  \"screws\": [");
	for (size_t i = 0; i < sizeof (kScrews) / sizeof (kScrews[0]); ++i)
		std::printf ("%s[%d,%d]", i ? "," : "", kScrews[i].x, kScrews[i].y);
	std::printf ("],\n");
	std::printf ("  \"lamp\": {\"rect\": %s, \"on\": %s},\n", rect (kSelfOscLamp).c_str (),
	             selfOscillating (internalDefault (kResonance)) ? "true" : "false");
	std::printf ("  \"info\": {\"rect\": %s, \"text\": %s},\n", rect (kModInfoLine).c_str (),
	             quoted (modInfoLineText (internalDefault)).c_str ());

	std::printf ("  \"sections\": [\n");
	for (int i = 0; i < kNumSections; ++i)
		std::printf ("    {\"title\": %s, \"subtitle\": %s, \"accent\": %s, \"rect\": %s, \"titleBox\": %s, "
		             "\"subtitleLeft\": %s}%s\n",
		             quoted (kSections[i].title).c_str (), quoted (kSections[i].subtitle).c_str (),
		             accentName (kSections[i].accent), rect (kSections[i].r).c_str (),
		             box (titleBox (kSections[i])).c_str (), num (subtitleLeft (kSections[i])).c_str (),
		             (i + 1 < kNumSections) ? "," : "");
	std::printf ("  ],\n");

	std::printf ("  \"chevrons\": [");
	for (size_t i = 0; i < sizeof (kChevrons) / sizeof (kChevrons[0]); ++i)
		std::printf ("%s[%d,%d]", i ? "," : "", kChevrons[i].x, kChevrons[i].y);
	std::printf ("],\n");

	std::printf ("  \"rules\": [");
	for (size_t i = 0; i < sizeof (kRules) / sizeof (kRules[0]); ++i)
		std::printf ("%s%s", i ? "," : "", rect (kRules[i]).c_str ());
	std::printf ("],\n");

	std::printf ("  \"captions\": [\n");
	for (int i = 0; i < kNumCaptions; ++i)
		std::printf ("    {\"text\": %s, \"style\": %s, \"accent\": %s, \"rect\": %s}%s\n",
		             quoted (kCaptions[i].text).c_str (), styleName (kCaptions[i].style),
		             accentName (kCaptions[i].accent), rect (kCaptions[i].r).c_str (),
		             (i + 1 < kNumCaptions) ? "," : "");
	std::printf ("  ],\n");

	std::printf ("  \"controls\": [\n");
	for (int i = 0; i < kNumControls; ++i)
	{
		const Control& c = kControls[i];
		const double v = paramDef (c.tag).defaultNormalized ();
		std::string names = "[";
		std::string selector = "null";
		if (c.kind == Kind::Switch)
		{
			const int positions = paramDef (c.tag).stepCount + 1;
			const Selector g = selectorOf (c, positions);
			std::string angles = "[", boxes = "[";
			for (int p = 0; p < positions; ++p)
			{
				names += std::string (p ? "," : "") + quoted (enumName (c.tag, p));
				angles += std::string (p ? "," : "") + num (g.angle[p]);
				boxes += std::string (p ? "," : "") + box (selectorLabelBox (g, p, enumName (c.tag, p)));
			}
			selector = "{\"cx\": " + num (g.cx) + ", \"cy\": " + num (g.cy) + ", \"radius\": " + num (g.radius) +
			           ", \"angles\": " + angles + "], \"labelBoxes\": " + boxes + "], \"nameY\": " +
			           num (g.nameCentreY) + "}";
		}
		names += "]";
		std::printf ("    {\"kind\": %s, \"accent\": %s, \"rect\": %s, \"diameter\": %d, \"bipolar\": %s, "
		             "\"value\": %.4f, \"text\": %s, \"label\": %s, \"names\": %s, \"selector\": %s}%s\n",
		             (c.kind == Kind::Knob) ? "\"knob\"" : "\"switch\"", accentName (c.accent),
		             rect (controlRect (c)).c_str (), c.diameter, c.bipolar ? "true" : "false", v,
		             quoted (readoutText (c.tag, v)).c_str (), quoted (controlLabel (c)).c_str (),
		             names.c_str (), selector.c_str (), (i + 1 < kNumControls) ? "," : "");
	}
	std::printf ("  ],\n");

	// ---- the four screens, from the editor's own functions ---------------
	float filter[kDisplayPoints];
	double mx = 0.0, my = 0.0;
	filterDisplayData (internalDefault (kCutoff), internalDefault (kResonance), sampleRate, filter, n, mx, my);

	AdsrSpec vcf, vca;
	vcf.attack = internalDefault (kVcfAttack);  vcf.decay = internalDefault (kVcfDecay);
	vcf.sustain = internalDefault (kVcfSustain); vcf.release = internalDefault (kVcfRelease);
	vca.attack = internalDefault (kVcaAttack);  vca.decay = internalDefault (kVcaDecay);
	vca.sustain = internalDefault (kVcaSustain); vca.release = internalDefault (kVcaRelease);
	float vcfCurve[kDisplayPoints], vcaCurve[kDisplayPoints];
	double gateOff = 0.0;
	const double span = traceEnvelopes (vcf, vca, vcfCurve, vcaCurve, n, &gateOff);
	const std::string amount = envAmountText (internalDefault (kVcfEnvAmount));

	float patched[kDisplayPoints], companion[kDisplayPoints];
	std::string patchedLegend, companionLegend;
	lfoDisplayData (internalDefault (kLfoOutput), internalDefault (kLfoShape), patched, companion, n, 2.0,
	                patchedLegend, companionLegend);

	const std::string filterNote = filterDisplayAnnotation (paramDef (kCutoff).defaultNormalized (),
	                                                        paramDef (kResonance).defaultNormalized ());

	std::printf ("  \"screens\": [\n");
	std::printf ("    {\"rect\": %s, \"accent\": \"filter\", \"fill\": true, \"caption\": %s, \"annotation\": %s, "
	             "\"legend\": [\"20 Hz\", \"20 kHz\"], \"marker\": [%.4f, %.4f], \"gate\": -1, \"main\": %s, \"companion\": []},\n",
	             rect (kFilterView).c_str (), quoted (filterDisplayCaption ()).c_str (), quoted (filterNote).c_str (),
	             mx, my, curve (filter, n).c_str ());
	std::printf ("    {\"rect\": %s, \"accent\": \"filter\", \"fill\": true, \"caption\": \"\", \"annotation\": %s, "
	             "\"legend\": [\"\", \"\"], \"marker\": [-1, 0], \"gate\": %.4f, \"main\": %s, \"companion\": []},\n",
	             rect (kFilterEnvView).c_str (), quoted (amount).c_str (), gateOff / span, curve (vcfCurve, n).c_str ());
	std::printf ("    {\"rect\": %s, \"accent\": \"amp\", \"fill\": true, \"caption\": \"key up at the dotted line\", "
	             "\"annotation\": %s, \"legend\": [\"\", \"\"], \"marker\": [-1, 0], \"gate\": %.4f, \"main\": %s, \"companion\": []},\n",
	             rect (kAmpEnvView).c_str (), quoted (spanText (span)).c_str (), gateOff / span, curve (vcaCurve, n).c_str ());
	std::printf ("    {\"rect\": %s, \"accent\": \"mod\", \"fill\": false, \"caption\": \"\", \"annotation\": \"\", "
	             "\"legend\": [%s, %s], \"marker\": [-1, 0], \"gate\": -1, \"main\": %s, \"companion\": %s}\n",
	             rect (kLfoView).c_str (), quoted (patchedLegend).c_str (), quoted (companionLegend).c_str (),
	             curve (patched, n).c_str (), curve (companion, n).c_str ());
	std::printf ("  ]\n");
	std::printf ("}\n");
	return 0;
}
