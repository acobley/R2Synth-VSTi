//------------------------------------------------------------------------
// R2Synth - layout and panel-text tests
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
// THE BLACK PANEL (branch PolivoksUI, 22 Oct 2026). Every fit is now
// measured with the panel's own lettering - Lettering::width() from
// SciFiLettering.h, the function the editor draws with - rather than an
// average character width. The selectors are held to their rectangles:
// every printed position inside it, clear of the knob and of each other,
// and a click on a position's name picks that position. The screens'
// black cut-outs are held clear of everything else.
//
// THE WEAR (22 Oct 2026, "Studio Worn", PanelWear.h): the same marks
// every time; every mark on the panel and off the screens' glass, every
// glass mark on its own glass, every knob's marks on its own knob; and
// all of it faint - no mark may be opaque enough to hide a label.
//
// Needs one header-only file from the SDK (pluginterfaces/vst/vsttypes.h,
// through R2SynthParams.h); nothing is linked from it:
//
//   c++ -std=c++17 -I<sdk> -I../source LayoutTests.cpp
//       ../source/R2SynthReadouts.cpp ../source/R2SynthParams.cpp
//       ../source/R2SynthDsp.cpp -o /tmp/layouttests
//------------------------------------------------------------------------

#include "PanelWear.h"
#include "R2SynthLayout.h"
#include "R2SynthReadouts.h"
#include "R2SynthDsp.h"
#include "SciFiLettering.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

using namespace R2Synth;
using namespace R2Synth::Layout;

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

/** The section a control is inside. */
int sectionOfControl (const Control& c)
{
	return sectionOf (controlRect (c));
}

/** A knob's reading is allowed 4 px past the knob's rectangle each side. */
constexpr int kTextSlack = 8;

/** Does `text` in `style` fit `pixels`? */
bool fits (const std::string& text, const Lettering::Style& style, double pixels)
{
	return Lettering::width (text, style) <= pixels;
}

bool boxInside (const Box& b, const Rect& r)
{
	return b.left >= r.left && b.right <= r.right && b.top >= r.top && b.bottom <= r.bottom;
}

bool boxesOverlap (const Box& a, const Box& b)
{
	return a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
}

std::string px (double v)
{
	char t[32];
	std::snprintf (t, sizeof (t), "%.1f", v);
	return t;
}

} // anonymous namespace

//------------------------------------------------------------------------
int main ()
{
	std::printf ("R2Synth layout tests\n");
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
		check (c.diameter > 0, name (c) + ": a knob, or a selector's knob, has a diameter");
		check (c.diameter <= c.width, name (c) + ": the knob fits its own width");
		check (c.kind == Kind::Switch || (c.height == 0 && c.pivotX == 0),
		       name (c) + ": only a selector has a height and a pivot");
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
			check (!overlaps (bezelOf (screens[i]), bezelOf (screens[j])), "the screens do not overlap");

	// THE BLACK CUT-OUTS round the screens reach 3 px past them: still
	// inside their sections, and clear of every control.
	for (const auto& s : screens)
	{
		check (sectionOf (bezelOf (s)) >= 0 || sectionOf (s) >= 0, "a screen's cut-out stays in its section");
		const Rect& sec = kSections[sectionOf (s) >= 0 ? sectionOf (s) : 0].r;
		check (inside (bezelOf (s), { sec.left + 8, sec.top + 8, sec.right - 8, sec.bottom - 8 }),
		       "a screen's cut-out is 8 px or more inside its section's edge");
		for (const auto& c : kControls)
			check (!overlaps (bezelOf (s), controlRect (c)), "a screen's cut-out clear of " + name (c));
	}

	for (const auto& c : kControls)
		check (sectionOfControl (c) >= 0, name (c) + " is inside exactly one section");

	// THE COLOUR SAYS WHICH SECTION: a control's accent is its section's.
	for (const auto& c : kControls)
	{
		const int s = sectionOfControl (c);
		if (s >= 0)
			check (kSections[s].accent == c.accent, name (c) + " wears its section's colour");
	}

	for (const auto& cap : kCaptions)
	{
		check (sectionOf (cap.r) >= 0, std::string ("caption \"") + cap.text + "\" inside a section");
		for (const auto& c : kControls)
			check (!overlaps (cap.r, controlRect (c)), std::string ("caption \"") + cap.text + "\" clear of " + name (c));
		for (const auto& s : screens)
			check (!overlaps (cap.r, bezelOf (s)), std::string ("caption \"") + cap.text + "\" clear of the screens");
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
		const std::string l = controlLabel (c);
		check (!l.empty (), name (c) + " has a label");

		if (c.kind == Kind::Switch)
		{
			// THE SELECTOR: its knob and marks, every printed position and
			// its name inside the rectangle the view draws in; positions
			// clear of the knob and of each other; a click on a name picks it.
			const Rect r = controlRect (c);
			const int positions = paramDef (c.tag).stepCount + 1;
			check (positions >= 2 && positions <= 3, name (c) + " has two or three positions");
			const Selector g = selectorOf (c, positions);
			const double reach = g.radius + 8.0;
			const Box knob { g.cx - reach, g.cy - reach, g.cx + reach, g.cy + reach };
			check (boxInside (knob, r), name (c) + ": the knob and its marks fit the rectangle");
			check (fits (l, kTextLabel, width), name (c) + ": its name fits");
			check (g.nameCentreY - kTextLabel.height () / 2 >= g.cy + g.radius + 2,
			       name (c) + ": its name is under the knob");

			std::vector<Box> boxes;
			for (int p = 0; p < positions; ++p)
			{
				const std::string t = enumName (c.tag, p);
				check (!t.empty (), name (c) + " position " + std::to_string (p) + " has a name");
				const Box b = selectorLabelBox (g, p, t);
				boxes.push_back (b);
				check (boxInside (b, r), name (c) + " \"" + t + "\" is printed inside the selector (" +
				       px (b.left) + ".." + px (b.right) + " in " + std::to_string (r.left) + ".." +
				       std::to_string (r.right) + ")");
				// Clear of the knob and its marks: the nearest point of the
				// name's box is outside the circle the marks reach to.
				const double nx = std::clamp (g.cx, b.left, b.right) - g.cx;
				const double ny = std::clamp (g.cy, b.top, b.bottom) - g.cy;
				check (nx * nx + ny * ny > reach * reach, name (c) + " \"" + t + "\" clear of the knob and its marks");
				const double mx = (b.left + b.right) / 2, my = (b.top + b.bottom) / 2;
				check (selectorPositionAt (g, mx, my) == p, name (c) + ": a click on \"" + t + "\" picks it");
			}
			for (size_t i = 0; i < boxes.size (); ++i)
				for (size_t j = i + 1; j < boxes.size (); ++j)
					check (!boxesOverlap (boxes[i], boxes[j]), name (c) + ": its printed positions do not touch");
			check (selectorPositionAt (g, g.cx, g.cy) == -1, name (c) + ": a click on the knob steps it on");
			continue;
		}

		for (double n : { 0.0, 0.001, 0.1, 0.25, 0.5, 0.75, 0.9, 1.0, paramDef (c.tag).defaultNormalized () })
		{
			const std::string t = readoutText (c.tag, n);
			check (!t.empty (), name (c) + " has a reading at " + std::to_string (n));
			check (fits (t, kTextValue, width + kTextSlack),
			       name (c) + " reading \"" + t + "\" fits " + std::to_string (width) + " px (" +
			       px (Lettering::width (t, kTextValue)) + ")");
		}
		check (fits (l, kTextLabel, width + kTextSlack), name (c) + " label \"" + l + "\" fits");
	}

	for (const auto& cap : kCaptions)
	{
		switch (cap.style)
		{
			case CaptionStyle::Block:
				check (fits (cap.text, kTextBlock, cap.r.width ()), std::string ("caption \"") + cap.text + "\" fits its place");
				break;
			case CaptionStyle::Field:
				check (fits (cap.text, kTextLabel, cap.r.width ()), std::string ("caption \"") + cap.text + "\" fits its place");
				break;
			case CaptionStyle::Note:
				check (fits (cap.text, kTextNote, cap.r.width ()), std::string ("caption \"") + cap.text + "\" fits its place");
				break;
			case CaptionStyle::Badge:
				check (cap.r.width () >= kBadgeSize && cap.r.height () >= kBadgeSize + 12,
				       std::string ("badge ") + cap.text + " has room for its ring and VCO");
				check (fits ("VCO", kTextBadgeSub, cap.r.width ()), "VCO fits under the ring");
				break;
		}
	}
	for (const auto& s : kSections)
	{
		// The title box, then the subtitle, inside the section's padding.
		const Box b = titleBox (s);
		double end = b.right;
		if (s.subtitle && *s.subtitle)
			end = subtitleLeft (s) + Lettering::width (s.subtitle, kTextSubtitle);
		check (end <= s.r.right - kSectionPadX, std::string (s.title) + "'s title row fits (" + px (end) + ")");
		check (b.bottom <= contentTop (s.r) - 4, std::string (s.title) + "'s title box is above the contents");
		check (b.top >= s.r.top + 4, std::string (s.title) + "'s title box is inside the section's top edge");
	}
	{
		// The filter's title row ends short of the SELF-OSC lamp, whose
		// dot sits 12 px left of its lettering.
		const Section& f = kSections[1];
		const double end = subtitleLeft (f) + Lettering::width (f.subtitle, kTextSubtitle);
		const double lampLeft = kSelfOscLamp.right - Lettering::width ("SELF-OSC", kTextLabel) - 12.0 - 8.0;
		check (end + 8 <= lampLeft, "the filter's subtitle stops short of the SELF-OSC lamp");
		check (lampLeft >= kSelfOscLamp.left, "the lamp's dot is inside its rectangle");

		// The VCF Type selector: the first thing in the filter section,
		// left of the screen's cut-out, under the title row.
		const Control* type = nullptr;
		for (const auto& c : kControls)
			if (c.tag == kVcfType)
				type = &c;
		check (type != nullptr, "the VCF Type selector is on the panel");
		if (type)
		{
			const Rect r = controlRect (*type);
			check (type->kind == Kind::Switch, "the VCF Type is a rotary selector");
			check (r.right + 4 <= bezelOf (kFilterView).left, "the VCF Type selector is left of the filter screen");
			check (r.top >= titleBox (f).bottom + 4, "and under the filter's title row");
			check (r.bottom <= 218, "and above the Cutoff knob");
		}
	}

	// ---- the lines of text that change -------------------------------------
	std::printf ("the changing lines fit\n");
	{
		// THE INFO LINE at its longest, over every Output position and a
		// spread of Shape and Level, the rates at their slowest (most
		// digits).
		size_t longest = 0;
		double widest = 0.0;
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
					widest = std::max (widest, Lettering::width (line, kTextInfo));
					if (!fits (line, kTextInfo, kModInfoLine.width ()))
						check (false, "info line fits the modulation strip: " + line);
				}
		std::printf ("  longest info line: %d chars, %s px of %d\n", static_cast<int> (longest), px (widest).c_str (),
		             kModInfoLine.width ());
		check (widest <= kModInfoLine.width (), "every info line fits");

		const std::string eng = engineLineText (192000.0, 10.0);
		check (fits (eng, kTextEngine, kEngineLine.width ()),
		       "the engine line fits the header: " + eng + " (" + px (Lettering::width (eng, kTextEngine)) + ")");

		// The wordmark: R2, 12 px, SYNTH - inside its rectangle.
		const double wordmark = Lettering::width ("R2", kTextWordmark) + 12.0 + Lettering::width ("SYNTH", kTextWordmark);
		check (kWordmark.left + wordmark <= kWordmark.right, "the wordmark fits its place (" + px (wordmark) + ")");
		check (kTextWordmark.height () <= kHeaderHeight - 12, "and its height fits the header");

		// The licence pill: 40 characters (R2Licence::kBadgeMaxChars,
		// tested in LicenceTests) - forty round capitals, a fair stand-in
		// for a long licensee's name - plus the window's lamp and margins.
		check (Lettering::width (std::string (40, 'O'), kTextPill) + 46 <= kBadge.width (),
		       "the licence pill's longest line fits");
		check (fits ("Evaluation - 7 days left - activate...", kTextPill, kBadge.width () - 46),
		       "the evaluation line fits the pill");

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

		// THE MS-20 CURVE: the same screen, its own filter. No bass loss
		// with resonance (the ladder's passband drops), the dot -6 dB at
		// the cutoff with none, and self-oscillation pinned to the top.
		float ms20[kDisplayPoints], moog[kDisplayPoints];
		double sx = 0.0, sy = 0.0;
		filterDisplayData (1200.0, 0.0, 48000.0, ms20, kDisplayPoints, sx, sy, FilterType::Ms20);
		check (std::fabs (ms20[0] - zeroDb) < 0.01, "MS-20: flat at 0 dB at the left edge");
		check (std::fabs (sy - (-7.3595 - kFilterBottomDb) / (kFilterTopDb - kFilterBottomDb)) < 0.005,
		       "MS-20 I: the dot sits -7.36 dB at the cutoff with no resonance (the Korg35's 7/3 damping)");
		{
			float late[kDisplayPoints];
			double lx = 0.0, ly = 0.0;
			filterDisplayData (1200.0, 0.0, 48000.0, late, kDisplayPoints, lx, ly, FilterType::Ms20Late);
			check (std::fabs (ly - (-6.0206 - kFilterBottomDb) / (kFilterTopDb - kFilterBottomDb)) < 0.005,
			       "MS-20 II: the dot sits -6 dB at the cutoff with no resonance");
		}
		{
			// Two octaves above the cutoff (~5 kHz): 12 dB/oct against 24.
			const int i5k = static_cast<int> (std::lround (std::log (5000.0 / kFilterMinHz) /
			                std::log (kFilterMaxHz / kFilterMinHz) * (kDisplayPoints - 1)));
			check (ms20[i5k] > curve[i5k] + 0.1f, "MS-20: shallower than the ladder two octaves up");
		}
		filterDisplayData (1200.0, 0.9, 48000.0, ms20, kDisplayPoints, sx, sy, FilterType::Ms20);
		filterDisplayData (1200.0, 0.9, 48000.0, moog, kDisplayPoints, mx, my2, FilterType::Moog);
		check (std::fabs (ms20[0] - zeroDb) < 0.01 && moog[0] < zeroDb - 0.05f,
		       "MS-20 keeps its bass at resonance 0.9; the ladder loses it");
		filterDisplayData (1200.0, kMaxResonanceK, 48000.0, ms20, kDisplayPoints, sx, sy, FilterType::Ms20);
		check (sy >= 0.99, "MS-20: a self-oscillating peak is drawn at the top of the screen");

		check (filterDisplayCaption (false, FilterType::Moog) == "at the knobs   24 dB/oct" &&
		       filterDisplayCaption (true, FilterType::Ms20) == "live   12 dB/oct",
		       "the screen's caption names the slope of the chosen filter");
		check (fits (filterDisplayCaption (false), kTextScreen, kFilterView.width () / 2.0 - 8),
		       "the caption fits the left half of the screen");
		check (fits (filterDisplayCaption (true, FilterType::Ms20Late), kTextScreen, kFilterView.width () / 2.0 - 8),
		       "and so does the live one");
		check (fits (filterDisplayAnnotation (1.0, 1.0), kTextScreen, kFilterView.width () / 2.0 - 8),
		       "the figure fits the right half: " + filterDisplayAnnotation (1.0, 1.0));
		check (filterDisplayCaption (false, FilterType::Ms20Late) == "at the knobs   12 dB/oct",
		       "the MS-20 II is 12 dB/oct too");
		check (filterTypeOf (paramDef (kVcfType).toInternal (0.0)) == FilterType::Moog &&
		       filterTypeOf (paramDef (kVcfType).toInternal (0.5)) == FilterType::Ms20 &&
		       filterTypeOf (paramDef (kVcfType).toInternal (1.0)) == FilterType::Ms20Late &&
		       filterTypeOf (paramDef (kVcfType).toInternal (paramDef (kVcfType).defaultNormalized ())) == FilterType::Moog,
		       "the VCF Type parameter maps to the DSP's FilterType, Moog by default");

		// The ceiling: a 20 kHz knob at 44.1 k is drawn where the filter
		// really is - cutoffCeilingHz() - not at the knob.
		filterDisplayData (20000.0, 0.0, 44100.0, curve, kDisplayPoints, mx, my);
		const double ceilingX = std::log (cutoffCeilingHz (44100.0) / kFilterMinHz) / std::log (kFilterMaxHz / kFilterMinHz);
		check (std::fabs (mx - ceilingX) < 1e-9, "the dot stops at the cutoff ceiling");
	}

	// ---- the lettering --------------------------------------------------------
	std::printf ("the lettering\n");
	{
		for (char c = 'A'; c <= 'Z'; ++c)
			check (Lettering::glyphFor (static_cast<uint32_t> (c)) != nullptr, std::string ("the face has ") + c);
		for (char c = '0'; c <= '9'; ++c)
			check (Lettering::glyphFor (static_cast<uint32_t> (c)) != nullptr, std::string ("the face has ") + c);
		for (const char* c : { ".", ",", ":", "%", "-", "+", "/", "=", "'", "(", ")", ">", "<", "!", "?", "\"",
		                       "|", "[", "]", "_", "&", "#", "@", "*" })
		{
			const Lettering::Glyph* g = Lettering::glyphFor (static_cast<unsigned char> (c[0]));
			check (g != nullptr, std::string ("the face has ") + c);
			if (g)
			{
				const auto cmds = Lettering::parse (g->path);
				check (!cmds.empty () && cmds.front ().op == 'M', std::string ("glyph ") + c + " parses");
			}
		}
		for (uint32_t c = 0; c < 0x2600; ++c)
			if (const Lettering::Glyph* g = Lettering::glyphFor (c))
			{
				const auto cmds = Lettering::parse (g->path);
				if (cmds.empty () || cmds.front ().op != 'M')
					check (false, "glyph U+" + std::to_string (c) + " parses");
				for (const auto& k : cmds)
					if (k.x < -1.0 || k.x > 6.0 || k.y < -1.0 || k.y > 7.5)
					{
						check (false, "glyph U+" + std::to_string (c) + " stays on its grid");
						break;
					}
			}
		check (Lettering::parse ("L1,1").empty () && Lettering::parse ("M1").empty (),
		       "a malformed path parses to nothing rather than to half a glyph");

		const auto cps = Lettering::codePoints ("aZ\xC3\xA9\xC3\x9F\xE2\x86\x92\xFF");
		check (cps.size () == 6 && cps[0] == 'A' && cps[1] == 'Z' && cps[2] == 'E' && cps[3] == 'S' &&
		       cps[4] == 0x2192 && cps[5] == 0xFFFD,
		       "lowercase in capitals, accents folded, UTF-8 decoded, a bad byte a dot");
		check (Lettering::glyphFor (0xFFFD) == nullptr && Lettering::glyphOrDot (0xFFFD).path != nullptr,
		       "a character the face lacks is drawn as the dot");

		// THE METRICS: SAW in the label style is (4 + 4 + 5) + 3 x (1.25
		// + 1.05) - 1.05 = 18.85 grid units, x 6.1 / 6 px.
		check (std::fabs (Lettering::width ("SAW", kTextLabel) - 18.85 * 6.1 / 6.0) < 1e-9, "SAW is 19.2 px wide");
		check (std::fabs (Lettering::width ("A B", kTextLabel) - Lettering::width ("AB", kTextLabel) -
		                  Lettering::kSpace * 6.1 / 6.0) < 1e-9, "a space is 2.6 units");
		check (Lettering::width ("", kTextLabel) == 0.0 && Lettering::width ("   ", kTextLabel) > 0.0,
		       "nothing is no width; spaces are not nothing");
		check (Lettering::width ("abc", kTextValue) == Lettering::width ("ABC", kTextValue), "lowercase is drawn as capitals");
	}

	// ---- the wear ---------------------------------------------------------
	{
		using namespace Wear;
		const PanelWear a = panelWear (), b = panelWear ();
		bool same = a.specks.size () == b.specks.size () && a.scratches.size () == b.scratches.size () &&
		            a.haze.size () == b.haze.size ();
		for (size_t i = 0; same && i < a.specks.size (); ++i)
			same = a.specks[i].x == b.specks[i].x && a.specks[i].y == b.specks[i].y && a.specks[i].alpha == b.specks[i].alpha;
		check (same, "the wear is the same every time it is made");
		check (a.haze.size () >= 50 && a.specks.size () >= 500 && a.scratches.size () >= 30 && !a.drifts.empty () &&
		       !a.fibres.empty (), "the panel has haze, drifts, specks, scratches and fibres");
		check (static_cast<int> (a.rings.size ()) == kNumControls, "every knob and selector has dust round its foot");

		Random r1 (42), r2 (42);
		bool repeat = true;
		for (int i = 0; i < 100; ++i)
			repeat = repeat && r1.next () == r2.next ();
		check (repeat && Random (43).next () != Random (42).next (), "the random numbers repeat for a seed, and only for it");
		{
			Random r (7);
			double sum = 0.0, sq = 0.0;
			for (int i = 0; i < 20000; ++i) { const double v = r.normal (); sum += v; sq += v * v; }
			const double mean = sum / 20000, var = sq / 20000 - mean * mean;
			check (std::fabs (mean) < 0.03 && std::fabs (var - 1.0) < 0.05, "the normal distribution is standard");
		}

		const Rect panel { 0, 0, kEditorWidth, kEditorHeight };
		auto onPanel = [&] (double x, double y) { return x >= 0 && y >= 0 && x <= kEditorWidth && y <= kEditorHeight; };
		auto onGlass = [&] (double x, double y, double pad) {
			for (const auto& s : kScreens)
			{
				const Rect bz = bezelOf (s);
				if (x >= bz.left - pad && x <= bz.right + pad && y >= bz.top - pad && y <= bz.bottom + pad)
					return true;
			}
			return false;
		};

		bool hazeOk = true, specksOk = true, linesOk = true, faint = true, driftsOk = true;
		for (const auto& h : a.haze)
			hazeOk = hazeOk && h.alpha <= kMaxHazeAlpha && h.r > 0;
		for (const auto& d : a.drifts)
			driftsOk = driftsOk && inside (d.r, panel) && d.alpha <= kMaxHazeAlpha &&
			           (std::abs (d.fromX) + std::abs (d.fromY)) == 1;
		for (const auto& f : a.rings)
			hazeOk = hazeOk && f.alpha <= 32 && f.inner < f.peak && f.peak < f.outer;
		for (const auto& sp : a.specks)
		{
			specksOk = specksOk && onPanel (sp.x, sp.y);
			for (const auto& s : kScreens)
				specksOk = specksOk && !(sp.x > s.left && sp.x < s.right && sp.y > s.top && sp.y < s.bottom);
			faint = faint && sp.alpha <= kMaxMarkAlpha && sp.r <= 0.7;
		}
		for (const auto* set : { &a.scratches, &a.fibres })
			for (const auto& l : *set)
			{
				faint = faint && l.alpha <= kMaxMarkAlpha && l.width <= 0.5;
				linesOk = linesOk && l.pts.size () >= 2;
				for (const auto& q : l.pts)
					linesOk = linesOk && onPanel (q.x, q.y) && !onGlass (q.x, q.y, 1.0);
			}
		check (hazeOk, "the haze and the foot rings are faint (alpha <= " + std::to_string (kMaxHazeAlpha) + ")");
		check (driftsOk, "every drift is on the panel, faint, and fades from one edge");
		check (specksOk, "every speck is on the panel and none on a screen's glass");
		check (linesOk, "every scratch and fibre is on the panel and stops short of the screens");
		check (faint, "no speck, scratch or fibre is opaque enough to hide a label");

		bool knobsOk = true, distinct = false;
		const KnobWear first = knobWear (1);
		for (int i = 0; i < kNumControls; ++i)
		{
			if (kControls[i].kind == Kind::Knob)
			{
				const KnobWear k = knobWear (i);
				knobsOk = knobsOk && !k.scuffs.empty () && !k.grit.empty () && k.sheen > 0 && k.sheen <= kMaxHazeAlpha;
				for (const auto& arc : k.scuffs)
					knobsOk = knobsOk && arc.radius > 0 && arc.radius <= 0.95 && arc.span > 0 && arc.span < 180 &&
					          arc.alpha <= kMaxMarkAlpha;
				for (const auto& g : k.grit)
					knobsOk = knobsOk && g.radius > 0 && g.radius < 1.0 && g.alpha <= kMaxMarkAlpha;
				if (i != 1 && !k.scuffs.empty () && k.scuffs[0].start != first.scuffs[0].start)
					distinct = true;
			}
			else
				for (const auto& g : selectorGrit (i))
					knobsOk = knobsOk && g.radius > 0 && g.radius <= 1.0 && g.alpha <= kMaxMarkAlpha;
		}
		check (knobsOk, "every knob's marks are on its cap or in its knurling, and faint");
		check (distinct, "every knob wears differently");
		check (controlIndex (kCutoff) >= 0 && kControls[controlIndex (kCutoff)].tag == kCutoff && controlIndex (999999) == -1,
		       "a control's wear is found by its tag");

		const int screens = static_cast<int> (sizeof (kScreens) / sizeof (kScreens[0]));
		int prints = 0;
		for (int i = 0; i < screens; ++i)
		{
			const GlassWear g = glassWear (i);
			const Rect s = kScreens[i];
			auto on = [&] (double x, double y) { return x >= s.left && x <= s.right && y >= s.top && y <= s.bottom; };
			bool ok = !g.dust.empty ();
			for (const auto& d : g.dust)
				ok = ok && on (d.x, d.y) && d.alpha <= kMaxMarkAlpha;
			for (const auto& l : g.fibres)
				for (const auto& q : l.pts)
					ok = ok && on (q.x, q.y);
			for (const auto& p : g.prints)
			{
				++prints;
				for (const auto& ridge : p.ridges)
				{
					ok = ok && ridge.alpha <= kMaxPrintAlpha;
					for (const auto& q : ridge.pts)
						ok = ok && on (q.x, q.y);
				}
			}
			check (ok, "screen " + std::to_string (i) + ": its glass's marks are on its glass, and faint");
		}
		check (prints == 2, "two fingerprints: the filter screen and the amp screen");
	}

	std::printf ("------------------------\n");
	std::printf ("%d checks, %d failures\n", gChecks, gFailures);
	return (gFailures == 0) ? 0 : 1;
}
