//------------------------------------------------------------------------
// SimpleSynth - layout and panel-text tests
//
// The panel cannot be opened from the session that writes it - there is
// no host - so what CAN be checked about it is checked here, from the
// same tables and the same text functions the editor uses:
//
//   * every parameter is on the panel exactly once, a switch for every
//     Enum and a knob for everything else;
//   * nothing overlaps; every control, screen and caption is inside its
//     section, every section inside the panel and below the header;
//   * every reading and label fits its knob at both ends of its range
//     and in between, and every switch position fits its segment;
//   * the lines of text that change fit their places at their longest;
//   * the filter screen's data is the shape a low-pass curve should be.
//
// THE "SIGNAL FLOW" PANEL (branch UpdatedUI). The FilterDrum grid's
// checks - title bands, the common right edge, the readout lines beside
// LFO 2 - went with the grid.
//
// Needs one header-only file from the SDK (pluginterfaces/vst/vsttypes.h,
// through SimpleSynthParams.h); nothing is linked from it:
//
//   c++ -std=c++17 -I<sdk> -I../source LayoutTests.cpp
//       ../source/SimpleSynthReadouts.cpp ../source/SimpleSynthParams.cpp
//       ../source/SimpleSynthDsp.cpp -o /tmp/layouttests
//------------------------------------------------------------------------

#include "SimpleSynthLayout.h"
#include "SimpleSynthReadouts.h"
#include "SimpleSynthDsp.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <string>
#include <vector>

using namespace SimpleSynth;
using namespace SimpleSynth::Layout;

namespace {

int gChecks = 0;
int gFailures = 0;

void check (bool condition, const std::string& what)
{
	++gChecks;
	if (!condition)
	{
		++gFailures;
		std::printf ("  FAIL  %s\n", what.c_str ());
	}
}

bool overlaps (const Rect& a, const Rect& b)
{
	return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

bool inside (const Rect& inner, const Rect& outer)
{
	return inner.left >= outer.left && inner.right <= outer.right &&
	       inner.top >= outer.top && inner.bottom <= outer.bottom;
}

std::string name (const Control& c)
{
	return std::string (paramDef (c.tag).title);
}

/** The inside of a section: below its title row, inside its padding. */
Rect innerOf (const Section& s)
{
	return { s.r.left + kSectionPadX, s.r.top + kSectionPadTop + kSectionTitleH,
	         s.r.right - kSectionPadX, s.r.bottom - 10 };
}

/** The section a rectangle is inside, or -1; -2 if more than one. */
int sectionOf (const Rect& r)
{
	int found = -1;
	for (int i = 0; i < kNumSections; ++i)
		if (inside (r, innerOf (kSections[i])))
			found = (found == -1) ? i : -2;
	return found;
}

/** A knob's reading is drawn 4 px wider than the knob each side. */
constexpr int kTextSlack = 8;

/** The segment names, bold 12: a generous average. */
constexpr int kSegmentCharWidth = 8;

/** The licence pill's face, bold 13, and its padding round the text. */
constexpr int kPillCharWidth = 8;
constexpr int kPillPadding   = 40;

} // anonymous namespace

//------------------------------------------------------------------------
int main ()
{
	std::printf ("SimpleSynth layout tests\n");
	std::printf ("------------------------\n");

	const Rect panel { 0, 0, kEditorWidth, kEditorHeight };
	const Rect body  { 0, kHeaderHeight, kEditorWidth, kEditorHeight };
	const Rect screens[] = { kFilterView, kFilterEnvView, kAmpEnvView, kLfoView };

	// ---- every parameter, once ----------------------------------------
	std::printf ("every parameter once\n");
	for (int id = 0; id < kParamCount; ++id)
	{
		int count = 0;
		for (const auto& c : kControls)
			if (static_cast<int> (c.tag) == id)
				++count;
		check (count == 1, std::string (kParams[id].title) + " is on the panel exactly once (" +
		                   std::to_string (count) + ")");
	}
	for (const auto& c : kControls)
	{
		check ((paramDef (c.tag).type == ParamType::Enum) == (c.kind == Kind::Switch),
		       name (c) + ": an Enum is a switch and a range is a knob");
		check ((c.kind == Kind::Knob) == (c.diameter > 0), name (c) + ": a knob has a diameter, a switch none");
		check (c.kind == Kind::Switch || c.diameter <= c.width, name (c) + ": the knob fits its own width");
	}

	// ---- nothing overlaps -----------------------------------------------
	std::printf ("nothing overlaps\n");
	for (int i = 0; i < kNumControls; ++i)
		for (int j = i + 1; j < kNumControls; ++j)
			check (!overlaps (controlRect (kControls[i]), controlRect (kControls[j])),
			       name (kControls[i]) + " clear of " + name (kControls[j]));

	for (int i = 0; i < kNumSections; ++i)
	{
		check (inside (kSections[i].r, body), std::string (kSections[i].title) + " inside the panel, below the header");
		for (int j = i + 1; j < kNumSections; ++j)
			check (!overlaps (kSections[i].r, kSections[j].r),
			       std::string (kSections[i].title) + " clear of " + kSections[j].title);
	}

	for (const auto& s : screens)
	{
		check (sectionOf (s) >= 0, "every screen is inside exactly one section");
		for (const auto& c : kControls)
			check (!overlaps (s, controlRect (c)), "a screen clear of " + name (c));
	}
	for (size_t i = 0; i < 4; ++i)
		for (size_t j = i + 1; j < 4; ++j)
			check (!overlaps (screens[i], screens[j]), "the screens do not overlap");

	for (const auto& c : kControls)
		check (sectionOf (controlRect (c)) >= 0, name (c) + " is inside exactly one section");

	// THE COLOUR SAYS WHICH SECTION: a control's accent is its section's.
	for (const auto& c : kControls)
	{
		const int s = sectionOf (controlRect (c));
		if (s >= 0)
			check (kSections[s].accent == c.accent, name (c) + " wears its section's colour");
	}

	for (const auto& cap : kCaptions)
	{
		check (sectionOf (cap.r) >= 0, std::string ("caption \"") + cap.text + "\" inside a section");
		for (const auto& c : kControls)
			check (!overlaps (cap.r, controlRect (c)), std::string ("caption \"") + cap.text + "\" clear of " + name (c));
		for (const auto& s : screens)
			check (!overlaps (cap.r, s), std::string ("caption \"") + cap.text + "\" clear of the screens");
	}

	for (const auto& r : kRules)
	{
		for (const auto& c : kControls)
			check (!overlaps (r, controlRect (c)), "a rule clear of " + name (c));
		for (const auto& s : screens)
			check (!overlaps (r, s), "a rule clear of the screens");
	}

	check (inside (kSelfOscLamp, kFilterRect), "the SELF-OSC lamp is in the filter section");
	check (kSelfOscLamp.bottom <= kFilterView.top, "and above the filter screen");
	check (inside (kModInfoLine, innerOf (kSections[3])), "the info line is inside the modulation strip");
	for (const auto& c : kControls)
		check (!overlaps (kModInfoLine, controlRect (c)), "the info line clear of " + name (c));

	check (inside (kWordmark, panel) && kWordmark.bottom <= kHeaderHeight, "the wordmark is in the header");
	check (inside (kEngineLine, panel) && kEngineLine.bottom <= kHeaderHeight, "the engine line is in the header");
	check (inside (kBadge, panel) && kBadge.bottom <= kHeaderHeight, "the licence pill is in the header");
	check (kWordmark.right < kEngineLine.left && kEngineLine.right < kBadge.left,
	       "wordmark, engine line and pill in a row, left to right");
	check (kBadge.right == kEditorWidth - kMargin, "the pill ends on the right margin");

	// ---- every reading and label fits -------------------------------------
	std::printf ("readings and labels fit\n");
	for (const auto& c : kControls)
	{
		const int width = controlRect (c).width ();
		if (c.kind == Kind::Switch)
		{
			const int positions = paramDef (c.tag).stepCount + 1;
			const int segment = (width - 4) / positions;
			for (int p = 0; p < positions; ++p)
			{
				const std::string t = enumName (c.tag, p);
				check (!t.empty (), name (c) + " position " + std::to_string (p) + " has a name");
				check (static_cast<int> (t.size ()) * kSegmentCharWidth <= segment - 6,
				       name (c) + " \"" + t + "\" fits its segment");
			}
			continue;
		}

		for (double n : { 0.0, 0.001, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0, paramDef (c.tag).defaultNormalized () })
		{
			const std::string t = readoutText (c.tag, n);
			check (!t.empty (), name (c) + " has a reading at " + std::to_string (n));
			check (static_cast<int> (t.size ()) * kValueCharWidth <= width + kTextSlack,
			       name (c) + " reading \"" + t + "\" fits " + std::to_string (width) + " px");
		}
		const std::string l = controlLabel (c);
		check (!l.empty (), name (c) + " has a label");
		check (static_cast<int> (l.size ()) * kLabelCharWidth <= width + kTextSlack,
		       name (c) + " label \"" + l + "\" fits");
	}

	for (const auto& cap : kCaptions)
	{
		const int cw = (cap.style == CaptionStyle::Note) ? kNoteCharWidth : kLabelCharWidth;
		if (cap.style != CaptionStyle::Badge)
			check (static_cast<int> (std::string (cap.text).size ()) * cw <= cap.r.width (),
			       std::string ("caption \"") + cap.text + "\" fits its place");
	}
	for (const auto& s : kSections)
	{
		// Title (bold 13) and subtitle (12) side by side on the title row.
		const int need = kSectionPadX + 16 + static_cast<int> (std::string (s.title).size ()) * 9 + 10 +
		                 static_cast<int> (std::string (s.subtitle).size ()) * 6;
		check (need <= s.r.width () - kSectionPadX, std::string (s.title) + "'s title row fits");
	}
	{
		// The filter section's title row also carries the lamp.
		const int titleEnd = kFilterRect.left + kSectionPadX + 16 + 6 * 9 + 10 + 18 * 6;
		check (titleEnd <= kSelfOscLamp.left, "the filter's title and subtitle stop short of the lamp");
	}

	// ---- the lines of text that change -------------------------------------
	std::printf ("the changing lines fit\n");
	{
		// THE INFO LINE at its longest, over every Output position and a
		// spread of Shape and Level, the rates at their slowest (most
		// digits).
		size_t longest = 0;
		for (double output : { 0.0, 0.5, 1.0 })
			for (double shape : { 0.0, 0.25, 0.5, 0.75, 1.0 })
				for (double level : { 0.0, 0.001, 0.5, 1.0 })
				{
					auto at = [&] (Steinberg::Vst::ParamID id) {
						const ParamDef& d = paramDef (id);
						if (id == kLfoRate || id == kLfo2Rate) return d.toInternal (0.0);
						if (id == kLfoOutput) return d.toInternal (output);
						if (id == kLfoShape)  return d.toInternal (shape);
						if (id == kLfo2Level) return d.toInternal (level);
						return d.toInternal (1.0);
					};
					const std::string line = modInfoLineText (at);
					longest = std::max (longest, line.size ());
					if (static_cast<int> (line.size ()) * kValueCharWidth > kModInfoLine.width ())
						check (false, "info line fits the modulation strip: " + line);
				}
		std::printf ("  longest info line: %d chars in %d px\n", static_cast<int> (longest), kModInfoLine.width ());
		check (static_cast<int> (longest) * kValueCharWidth <= kModInfoLine.width (), "every info line fits");

		const std::string eng = engineLineText (192000.0, 10.0);
		check (static_cast<int> (eng.size ()) * kNoteCharWidth <= kEngineLine.width (),
		       "the engine line fits the header: " + eng);

		// The licence pill: 40 characters (R2Licence::kBadgeMaxChars,
		// tested in LicenceTests) in the pill's face, plus its padding.
		check (40 * kPillCharWidth + kPillPadding <= kBadge.width () + 24,
		       "the licence pill's longest line fits");

		// LFO 2's part of the line from the SAME clamp the DSP runs: knob
		// +60 %, full Level, reaches -40 % and parks at the rising saw.
		auto clamped = [] (Steinberg::Vst::ParamID id) {
			if (id == kLfoShape)  return 0.6;
			if (id == kLfo2Level) return 1.0;
			if (id == kLfo2Rate)  return 0.5;
			return paramDef (id).toInternal (paramDef (id).defaultNormalized ());
		};
		const std::string l2 = modInfoLineText (clamped);
		check (l2.find ("-40..+100") != std::string::npos, "the info line prints LFO 2's clamped range: " + l2);
		check (modInfoLineText ([] (Steinberg::Vst::ParamID id) {
		           return paramDef (id).toInternal (paramDef (id).defaultNormalized ()); }).find ("off") != std::string::npos,
		       "at its default Level the info line says LFO 2 is off");
	}

	// ---- the filter screen's data -----------------------------------------
	std::printf ("the filter screen\n");
	{
		float curve[kDisplayPoints];
		double mx = 0.0, my = 0.0;
		filterDisplayData (1200.0, 0.0, 48000.0, curve, kDisplayPoints, mx, my);
		const double zeroDb = (0.0 - kFilterBottomDb) / (kFilterTopDb - kFilterBottomDb);
		check (std::fabs (curve[0] - zeroDb) < 0.01, "flat at 0 dB at the left edge with no resonance");
		check (curve[kDisplayPoints - 1] < 0.05f, "at the floor at 20 kHz from a 1.2 kHz cutoff");
		for (float v : curve)
			if (!(v >= 0.f && v <= 1.f))
			{
				check (false, "every point is inside the screen");
				break;
			}
		const double wantX = std::log (1200.0 / kFilterMinHz) / std::log (kFilterMaxHz / kFilterMinHz);
		check (std::fabs (mx - wantX) < 1e-9, "the dot is at the cutoff");
		check (my < zeroDb && my > 0.0, "and on the curve, below 0 dB, at no resonance");

		// Resonance lifts the dot; self-oscillation pins it to the top.
		double my2 = 0.0;
		filterDisplayData (1200.0, 0.9, 48000.0, curve, kDisplayPoints, mx, my2);
		check (my2 > my + 0.1, "resonance lifts the curve at the cutoff");
		double my3 = 0.0;
		filterDisplayData (1200.0, kMaxResonanceK, 48000.0, curve, kDisplayPoints, mx, my3);
		check (my3 >= 0.99, "a self-oscillating peak is drawn at the top of the screen");

		// The ceiling: a 20 kHz knob at 44.1 k is drawn where the filter
		// really is - cutoffCeilingHz() - not at the knob.
		filterDisplayData (20000.0, 0.0, 44100.0, curve, kDisplayPoints, mx, my);
		const double ceilingX = std::log (cutoffCeilingHz (44100.0) / kFilterMinHz) / std::log (kFilterMaxHz / kFilterMinHz);
		check (std::fabs (mx - ceilingX) < 1e-9, "the dot stops at the cutoff ceiling");
	}

	std::printf ("------------------------\n");
	std::printf ("%d checks, %d failures\n", gChecks, gFailures);
	return (gFailures == 0) ? 0 : 1;
}
