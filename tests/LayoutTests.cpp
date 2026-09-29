//------------------------------------------------------------------------
// SimpleSynth - layout and panel-text tests
//
// The panel cannot be opened from the session that writes it - there is
// no host - so what CAN be checked about it is checked here, from the
// same tables and the same text functions the editor uses:
//
//   * every parameter is on the panel exactly once;
//   * nothing overlaps, everything is inside its box, every box and
//     display is inside the panel;
//   * every reading fits its control at both ends of its range and in
//     the middle.
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
#include <cstdio>
#include <string>

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

/** A generous estimate of the width of panelFont()'s smallest face: the
    controls drop a size rather than overflow, so this is the point past
    which even the smallest face would not fit. */
constexpr int kTinyCharWidth = 5;

/** The same for panelFont()'s main face (Arial 11), which the title and
    the flow line use. */
constexpr int kMainCharWidth = 6;

} // anonymous namespace

//------------------------------------------------------------------------
int main ()
{
	std::printf ("SimpleSynth layout tests\n");
	std::printf ("------------------------\n");

	const Rect panel { 0, 0, kEditorWidth, kEditorHeight };
	const Rect lfoView { kDisplayX, kLfoViewTop, kDisplayX + kDisplayWidth, kLfoViewBottom };
	const Rect envView { kDisplayX, kEnvViewTop, kDisplayX + kDisplayWidth, kEnvViewBottom };
	const Rect readouts { kReadoutX, kReadout1Y, kReadoutX + kReadoutWidth, kReadout3Y + kLabelHeight };

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
		check ((paramDef (c.tag).type == ParamType::Enum) == (c.kind == Kind::Selector),
		       name (c) + ": a switch is a selector and a range is a slider");

	std::printf ("nothing overlaps\n");
	for (int i = 0; i < kNumControls; ++i)
		for (int j = i + 1; j < kNumControls; ++j)
			check (!overlaps (controlRect (kControls[i]), controlRect (kControls[j])),
			       name (kControls[i]) + " clear of " + name (kControls[j]));

	for (int i = 0; i < kNumGroups; ++i)
		for (int j = i + 1; j < kNumGroups; ++j)
			check (!overlaps (groupRect (kGroups[i]), groupRect (kGroups[j])),
			       std::string (kGroups[i].title) + " box clear of " + kGroups[j].title);

	for (const auto& g : kGroups)
	{
		check (inside (groupRect (g), panel), std::string (g.title) + " box inside the panel");
		check (!overlaps (groupRect (g), lfoView) && !overlaps (groupRect (g), envView),
		       std::string (g.title) + " box clear of the displays");
		check (!overlaps (groupRect (g), readouts), std::string (g.title) + " box clear of the readout lines");
	}
	check (inside (lfoView, panel) && inside (envView, panel), "both displays inside the panel");

	// THE TITLE MUST BE INSIDE THE BOX'S VIEW - VSTGUI clips a view to its
	// own rectangle, and a title drawn above it (FilterDrum's way) is cut
	// in half. SpyGroupBox draws 11 pixels of lettering from 5.5 above
	// the frame line, which sits kGroupTitleBand + 0.5 down the view.
	check (kGroupTitleBand + 0.5 - 5.5 >= 0.0, "every box title starts inside its view");
	check (kGroupTitleBand + 0.5 - 5.5 + 11.0 <= kGroupPadTop + 1.0,
	       "and ends above the readings of the sliders in the box");
	check (!overlaps (lfoView, envView), "the displays do not overlap");
	check (inside (readouts, panel), "the readout lines inside the panel");

	std::printf ("every control inside its own box\n");
	for (const auto& c : kControls)
	{
		int boxes = 0;
		for (const auto& g : kGroups)
			if (g.row == c.row && c.column >= g.firstColumn && c.column < g.firstColumn + g.columns &&
			    g.slot == c.slot && inside (controlRect (c), groupRect (g)))
				++boxes;
		check (boxes == 1, name (c) + " is inside exactly one box");
	}

	std::printf ("the displays line up with the boxes\n");
	{
		int lfoTop = 1 << 30, envBottom = 0;
		for (const auto& g : kGroups)
		{
			if (g.row == 0) lfoTop = std::min (lfoTop, groupRect (g).top);
			if (g.row == 3) envBottom = std::max (envBottom, groupRect (g).bottom);
		}
		check (lfoTop + kGroupTitleBand == kLfoViewTop, "the LFO display's top is the VCO boxes' frame");
		check (envBottom == kEnvViewBottom, "the envelope display's bottom is the ENV boxes' bottom");
		for (const auto& g : kGroups)
			if (g.stretchToRowRight)
				check (groupRect (g).right == kRowRight, std::string (g.title) + " box ends on the common right edge");
	}

	std::printf ("every reading fits\n");
	for (const auto& c : kControls)
	{
		const ParamDef& def = paramDef (c.tag);
		if (c.kind == Kind::Selector)
		{
			for (int p = 0; p <= def.stepCount; ++p)
			{
				const std::string t = selectorText (c.tag, p);
				check (!std::string (enumName (c.tag, p)).empty (), name (c) + " position " + std::to_string (p) + " has a name");
				check (static_cast<int> (t.size ()) * kTinyCharWidth <= kColumnWidth,
				       name (c) + " \"" + t + "\" fits");
			}
			// A compact selector draws its label like a slider does.
			const std::string l = shortLabel (c.tag);
			check (static_cast<int> (l.size ()) * kTinyCharWidth <= kColumnWidth, name (c) + " label \"" + l + "\" fits");
			continue;
		}
		for (double n : { 0.0, 0.001, 0.5, 0.999, 1.0 })
		{
			const std::string t = readoutText (c.tag, n);
			check (!t.empty (), name (c) + " has a reading at " + std::to_string (n));
			check (static_cast<int> (t.size ()) * kTinyCharWidth <= kColumnWidth,
			       name (c) + " \"" + t + "\" fits");
		}
		const std::string l = shortLabel (c.tag);
		check (static_cast<int> (l.size ()) * kTinyCharWidth <= kColumnWidth, name (c) + " label \"" + l + "\" fits");
	}

	// THE THREE LFO LINES at their longest, over every Output position
	// and a spread of Shape and Level: the rates at their slowest (most
	// digits), every depth at full.
	std::printf ("the readout lines fit\n");
	{
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
					for (const std::string& line : { lfoLineText (at), lfoDepthsLineText (at), lfo2LineText (at) })
					{
						longest = std::max (longest, line.size ());
						if (static_cast<int> (line.size ()) * kTinyCharWidth > kReadoutWidth)
							check (false, "readout line fits beside the LFO 2 box: " + line);
					}
				}
		std::printf ("  longest LFO line: %d chars in %d px\n", static_cast<int> (longest), kReadoutWidth);
		check (static_cast<int> (longest) * kTinyCharWidth <= kReadoutWidth, "every LFO line fits beside the LFO 2 box");

		const std::string eng = engineLineText (192000.0, 10.0);
		check (static_cast<int> (eng.size ()) * kTinyCharWidth <= kEngineLineWidth,
		       "the engine line fits on the title row: " + eng);
		const std::string title = panelTitleText ();
		check (static_cast<int> (title.size ()) * kMainCharWidth <= kTitleWidth,
		       "the title stops short of the engine line (" + std::to_string (title.size ()) + " chars)");
		const std::string flow = panelFlowText ();
		check (static_cast<int> (flow.size ()) * kMainCharWidth <= kFlowWidth,
		       "the flow line fits the panel (" + std::to_string (flow.size ()) + " chars)");
		check (kEngineLineX + kEngineLineWidth <= kEditorWidth - kMargin, "the engine line ends inside the margin");
		// The licence badge: 40 characters (R2Licence::kBadgeMaxChars,
		// tested in LicenceTests) at the tiny font, below everything.
		check (40 * kTinyCharWidth <= kBadgeWidth, "the licence badge's longest line fits");
		check (kBadgeY >= kEnvViewBottom && kBadgeY + kBadgeHeight <= kEditorHeight &&
		           kBadgeX + kBadgeWidth <= kEditorWidth - kMargin,
		       "the licence badge sits in the bottom margin, clear of the envelope display");
		check (kTitleY + kLabelHeight <= kFlowY && kFlowY + kLabelHeight <= kLfoViewTop,
		       "the title row and the flow line are above every box");

		// LFO 2's line from the SAME clamp the DSP runs: knob +60 %, full
		// Level, reaches -40 % and parks at the rising saw.
		auto clamped = [] (Steinberg::Vst::ParamID id) {
			if (id == kLfoShape)  return 0.6;
			if (id == kLfo2Level) return 1.0;
			if (id == kLfo2Rate)  return 0.5;
			return paramDef (id).toInternal (paramDef (id).defaultNormalized ());
		};
		const std::string l2 = lfo2LineText (clamped);
		check (l2.find ("-40..+100") != std::string::npos, "LFO 2's line prints the clamped range: " + l2);
		check (lfo2LineText ([] (Steinberg::Vst::ParamID id) {
		           return paramDef (id).toInternal (paramDef (id).defaultNormalized ()); }).find ("off") != std::string::npos,
		       "at its default Level LFO 2's line says it is off");
	}

	std::printf ("------------------------\n");
	std::printf ("%d checks, %d failures\n", gChecks, gFailures);
	return (gFailures == 0) ? 0 : 1;
}
