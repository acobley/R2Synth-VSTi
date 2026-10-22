//------------------------------------------------------------------------
// R2Synth - the panel's layout, as data
//
// THE LAYOUT IS DATA, not positions typed into the middle of open(). It
// lives in this header, which includes NO VSTGUI, so that three things
// can read the same numbers:
//
//   * R2SynthEditor::open(), which builds the panel from the tables;
//   * tests/LayoutTests.cpp, which checks that nothing overlaps and
//     everything fits - the only way to test a layout from a session
//     that cannot run a host;
//   * tools/dump-layout.cpp + tools/render-panel.py, which draw
//     docs/panel.png from these tables.
//
// THE "SIGNAL FLOW" PANEL (branch UpdatedUI, 4 Oct 2026), from the
// design Designs/R2Synth-ConceptA-SignalFlow.png. It replaced the
// FilterDrum grid of bar sliders: the same parameters, laid out the way
// the sound travels. (Portamento, 6 Oct, joined Output in the Amp
// section: the 31st. The VCF Type switch, 7 Oct: the 32nd.)
//
// THE BLACK PANEL (branch PolivoksUI, 22 Oct 2026), from the design
// Designs/R2Synth-Polivoks-1c-OLED.png: the SAME layout, dressed as a
// hardware panel in the Soviet style - matte black, white printing,
// boxed sections, black knurled knobs with printed scales - lettered in
// Andy's 70s sci-fi face (SciFiLettering.h), with small colour OLED
// screens. What moved:
//
//   * EVERY SWITCH IS A ROTARY SELECTOR, even the two-position ones: a
//     chicken-head knob with its positions printed round it. A selector
//     needs height, so the VCF Type left the filter's title row for the
//     space left of the filter screen, and the screen starts at 516.
//   * LFO 1's OUTPUT heading went under its selector, as its name.
//   * The note beside Portamento ("Mono, last note wins...") went.
//   * The wordmark is wider, so the engine line starts further right.
//
//   +--------------------------------------------------------------+
//   | R2 SYNTH | engine line                      [licence pill] |  header
//   +--------------------------------------------------------------+
//   | OSCILLATORS  >  FILTER (type, curve,      >  AMP (envelope,  |
//   | 3 rows: wave,    4 knobs, filter env)         ADSR, output)   |
//   +--------------------------------------------------------------+
//   | MODULATION: LFO 1 (scope, rate, shape, output) | LFO 2 |      |
//   |             LFO 1 depth: pitch, cutoff, resonance, amp        |
//   +--------------------------------------------------------------+
//
// The Accent of a control still names its section - the tests hold every
// control to its section's - and now picks the colour of that section's
// SCREEN traces (filter blue, amp green, modulation violet); the panel
// itself is black and white, with red for the one thing that is set.
//
// KNOBS, NOT BAR SLIDERS. A knob is drawn top-centred in its rectangle
// with its reading and its label under it, and the WHOLE rectangle takes
// the drag, so the text is part of the target. Vertical drag; Shift is
// fine; double-click returns the default; the wheel steps.
//------------------------------------------------------------------------

#pragma once

#include "R2SynthParams.h"
#include "SciFiLettering.h"

#include <cmath>

namespace R2Synth {
namespace Layout {

//------------------------------------------------------------------------
/** A rectangle, left/top/right/bottom, in panel pixels. */
struct Rect
{
	int left, top, right, bottom;

	constexpr int width () const { return right - left; }
	constexpr int height () const { return bottom - top; }
};

/** The same in fractional pixels - where a run of lettering lands. */
struct Box
{
	double left, top, right, bottom;
};

//------------------------------------------------------------------------
// The panel
//------------------------------------------------------------------------
constexpr int kEditorWidth  = 1200;
constexpr int kEditorHeight = 640;
constexpr int kMargin       = 16;

constexpr int kHeaderHeight = 56;

/** Inside a section: the title row, and the padding round it. */
constexpr int kSectionPadX     = 14;
constexpr int kSectionPadTop   = 12;
constexpr int kSectionTitleH   = 18;
constexpr int kSectionRadius   = 2;

/** Under every knob: two pixels, the reading, the label. */
constexpr int kKnobGap       = 2;
constexpr int kKnobValueH    = 13;
constexpr int kKnobLabelH    = 12;
constexpr int kKnobTextBand  = kKnobGap + kKnobValueH + kKnobLabelH;

/** A rotary selector's rectangle, top to bottom: a band for the printed
    position names over the knob, the knob, and its name at the foot. */
constexpr int kSelectorTopBand = 26;
constexpr int kSelectorNameH   = 12;

/** How far the black cut-out round each screen reaches past it. */
constexpr int kScreenBezel = 3;

//------------------------------------------------------------------------
// The lettering - SciFiLettering.h's face at the panel's sizes. Cap
// heights are 0.64 of the font sizes the design was drawn with.
//------------------------------------------------------------------------
constexpr Lettering::Style kTextWordmark { 24.0, 1.7, 1.2 };
constexpr Lettering::Style kTextTitle    { 8.3, Lettering::kBold };
constexpr Lettering::Style kTextSubtitle { 5.8, Lettering::kRegular };
constexpr Lettering::Style kTextLabel    { 6.1, Lettering::kBold };     // knob and selector names
constexpr Lettering::Style kTextValue    { 6.4, Lettering::kRegular };  // every reading
constexpr Lettering::Style kTextBlock    { 6.7, Lettering::kBold };     // "LFO 1 DEPTH"
constexpr Lettering::Style kTextNote     { 6.7, Lettering::kRegular };
constexpr Lettering::Style kTextEngine   { 6.7, Lettering::kRegular };
constexpr Lettering::Style kTextInfo     { 6.1, Lettering::kRegular };
constexpr Lettering::Style kTextScreen   { 6.1, Lettering::kRegular };
constexpr Lettering::Style kTextBadge    { 7.7, Lettering::kBold };
constexpr Lettering::Style kTextBadgeSub { 5.1, Lettering::kBold };     // "VCO" under the number
constexpr Lettering::Style kTextTick     { 4.8, Lettering::kRegular };  // 0 / 10 by a knob's scale
constexpr Lettering::Style kTextPill     { 7.0, Lettering::kBold };

//------------------------------------------------------------------------
// Sections
//------------------------------------------------------------------------
enum class Accent
{
	Osc,
	Filter,
	Amp,
	Mod
};

struct Section
{
	const char* title;
	const char* subtitle;   // may be empty
	Accent accent;
	Rect r;
};

constexpr Rect kOscRect    { 16,  72,  356,  436 };
constexpr Rect kFilterRect { 384, 72,  846,  436 };
constexpr Rect kAmpRect    { 874, 72,  1184, 436 };
constexpr Rect kModRect    { 16,  448, 1184, 624 };

constexpr Section kSections[] = {
	{ "OSCILLATORS", "mixed equally into the filter", Accent::Osc,    kOscRect    },
	{ "FILTER",      "low-pass - three circuits",     Accent::Filter, kFilterRect },
	{ "AMP",         "envelope + output",             Accent::Amp,    kAmpRect    },
	{ "MODULATION",  "",                              Accent::Mod,    kModRect    },
};
constexpr int kNumSections = static_cast<int> (sizeof (kSections) / sizeof (kSections[0]));

/** The top of a section's contents: under its title row. */
constexpr int contentTop (const Rect& section)
{
	return section.top + kSectionPadTop + kSectionTitleH + 10;
}

/** The middle of a section's title row. */
constexpr double titleRowCentre (const Rect& section)
{
	return section.top + kSectionPadTop + kSectionTitleH / 2.0 + 1.0;
}

/** THE TITLE BOX: the section's name in a white-outlined box at the left
    of its title row, 8 px of black each side of the lettering. */
inline Box titleBox (const Section& s)
{
	const double left = s.r.left + kSectionPadX - 2;
	const double centre = titleRowCentre (s.r);
	return { left, centre - 11.0, left + 16.0 + Lettering::width (s.title, kTextTitle), centre + 11.0 };
}

/** Where the subtitle starts: 12 px after the title box. */
inline double subtitleLeft (const Section& s)
{
	return titleBox (s).right + 12.0;
}

/** The two chevrons between the sections: the signal goes this way. */
struct Point { int x, y; };
constexpr Point kChevrons[] = { { 370, 254 }, { 860, 254 } };

//------------------------------------------------------------------------
// The oscillator rows
//------------------------------------------------------------------------
constexpr int kOscRowTop   = contentTop (kOscRect);   // 112
constexpr int kOscRowPitch = 96;

constexpr int oscRowTop (int vco) { return kOscRowTop + vco * kOscRowPitch; }

//------------------------------------------------------------------------
// Controls
//------------------------------------------------------------------------
enum class Kind
{
	Knob,     // Panel::Knob - vertical drag
	Switch    // Panel::RotarySwitch - one printed position per name
};

struct Control
{
	Steinberg::Vst::ParamID tag;
	Kind   kind;
	Accent accent;
	int    x, y;        // top left of the control's rectangle
	int    width;
	int    diameter;    // the knob - for a selector, its chicken head's circle
	bool   bipolar;     // knobs: the scale reads - .. + rather than 0 .. 10
	const char* label;  // nullptr: shortLabel() - the title less its section
	int    height = 0;  // selectors only
	int    pivotX = 0;  // selectors only: the knob's centre; 0 = the middle
};

constexpr Rect controlRect (const Control& c)
{
	return { c.x, c.y, c.x + c.width,
	         c.y + ((c.kind == Kind::Knob) ? c.diameter + kKnobTextBand : c.height) };
}

/** EVERY PARAMETER ON THE PANEL, once each - the tests hold it to that. */
constexpr Control kControls[] = {
	// ---- the oscillators: one row each, the same three controls -------
	{ kVco1Wave,  Kind::Switch, Accent::Osc, 64,  oscRowTop (0) + 11, 122, 32, false, nullptr, 83 },
	{ kVco1Tune,  Kind::Knob,   Accent::Osc, 210, oscRowTop (0) + 6,  64,  52, true,  nullptr },
	{ kVco1Level, Kind::Knob,   Accent::Osc, 278, oscRowTop (0) + 6,  64,  52, false, nullptr },
	{ kVco2Wave,  Kind::Switch, Accent::Osc, 64,  oscRowTop (1) + 11, 122, 32, false, nullptr, 83 },
	{ kVco2Tune,  Kind::Knob,   Accent::Osc, 210, oscRowTop (1) + 6,  64,  52, true,  nullptr },
	{ kVco2Level, Kind::Knob,   Accent::Osc, 278, oscRowTop (1) + 6,  64,  52, false, nullptr },
	{ kVco3Wave,  Kind::Switch, Accent::Osc, 64,  oscRowTop (2) + 11, 122, 32, false, nullptr, 83 },
	{ kVco3Tune,  Kind::Knob,   Accent::Osc, 210, oscRowTop (2) + 6,  64,  52, true,  nullptr },
	{ kVco3Level, Kind::Knob,   Accent::Osc, 278, oscRowTop (2) + 6,  64,  52, false, nullptr },

	// ---- the filter's type: the first thing to choose about a filter,
	// so the first thing in the section - a selector left of the screen.
	// Its knob is left of the rectangle's middle: MS-20 II, printed to
	// the right, is the longest name.
	{ kVcfType,       Kind::Switch, Accent::Filter, 398, 129, 110, 34, false, nullptr, 81, 447 },

	// ---- the filter: the two big ones, then key track and the amount --
	{ kCutoff,        Kind::Knob, Accent::Filter, 412, 218, 80, 66, false, nullptr },
	{ kResonance,     Kind::Knob, Accent::Filter, 521, 218, 80, 66, false, nullptr },
	{ kKeyTrack,      Kind::Knob, Accent::Filter, 629, 232, 80, 52, false, nullptr },
	{ kVcfEnvAmount,  Kind::Knob, Accent::Filter, 738, 232, 80, 52, true,  "Env Amt" },

	// ---- the filter envelope, under its own small picture -------------
	{ kVcfAttack,  Kind::Knob, Accent::Filter, 588, 328, 56, 38, false, "A" },
	{ kVcfDecay,   Kind::Knob, Accent::Filter, 650, 328, 56, 38, false, "D" },
	{ kVcfSustain, Kind::Knob, Accent::Filter, 712, 328, 56, 38, false, "S" },
	{ kVcfRelease, Kind::Knob, Accent::Filter, 774, 328, 56, 38, false, "R" },

	// ---- the amp -------------------------------------------------------
	{ kVcaAttack,   Kind::Knob, Accent::Amp, 888,  226, 64, 44, false, nullptr },
	{ kVcaDecay,    Kind::Knob, Accent::Amp, 960,  226, 64, 44, false, nullptr },
	{ kVcaSustain,  Kind::Knob, Accent::Amp, 1033, 226, 64, 44, false, nullptr },
	{ kVcaRelease,  Kind::Knob, Accent::Amp, 1106, 226, 64, 44, false, nullptr },
	{ kOutputLevel, Kind::Knob, Accent::Amp, 888,  316, 72, 56, false, "Output" },

	// Portamento: a VOICE control, so it sits with Output in the Amp
	// section (an Amp-section control - the tests hold every control to
	// its section).
	{ kPortamento,  Kind::Knob, Accent::Amp, 972,  316, 76, 56, false, nullptr },

	// ---- LFO 1 ---------------------------------------------------------
	{ kLfoRate,   Kind::Knob,   Accent::Mod, 192, 500, 64,  52, false, nullptr },
	{ kLfoShape,  Kind::Knob,   Accent::Mod, 262, 500, 64,  52, true,  nullptr },
	{ kLfoOutput, Kind::Switch, Accent::Mod, 360, 506, 130, 32, false, nullptr, 82 },

	// ---- LFO 2, which sweeps LFO 1's Shape ------------------------------
	{ kLfo2Rate,  Kind::Knob, Accent::Mod, 552, 500, 64, 52, false, nullptr },
	{ kLfo2Level, Kind::Knob, Accent::Mod, 622, 500, 64, 52, false, nullptr },

	// ---- everything LFO 1 moves, together ------------------------------
	{ kLfoPitch,  Kind::Knob, Accent::Mod, 726, 500, 70, 52, false, "Pitch" },
	{ kVcfLfo,    Kind::Knob, Accent::Mod, 830, 500, 70, 52, false, "Cutoff" },
	{ kVcfLfoRes, Kind::Knob, Accent::Mod, 934, 500, 70, 52, false, "Resonance" },
	{ kVcaLfo,    Kind::Knob, Accent::Mod, 1038, 500, 70, 52, false, "Amp" },
};
constexpr int kNumControls = static_cast<int> (sizeof (kControls) / sizeof (kControls[0]));

//------------------------------------------------------------------------
// The rotary selectors
//------------------------------------------------------------------------
/** A selector's geometry: its knob, where its positions point (degrees
    from twelve o'clock, clockwise - 45 either side, and straight up for
    the middle of three), and the radius its position names hang from. */
struct Selector
{
	double cx, cy, radius, labelRadius;
	int positions;
	double angle[3];
	double nameCentreY;
};

inline Selector selectorOf (const Control& c, int positions)
{
	const Rect r = controlRect (c);
	Selector s {};
	s.radius = c.diameter / 2.0;
	s.cx = (c.pivotX > 0) ? c.pivotX : (r.left + r.right) / 2.0;
	s.cy = r.top + kSelectorTopBand + s.radius;
	s.labelRadius = s.radius + 12.0;
	s.positions = (positions < 2) ? 2 : (positions > 3 ? 3 : positions);
	if (s.positions == 2)
	{
		s.angle[0] = -45.0;
		s.angle[1] = 45.0;
		s.angle[2] = 0.0;
	}
	else
	{
		s.angle[0] = -45.0;
		s.angle[1] = 0.0;
		s.angle[2] = 45.0;
	}
	s.nameCentreY = r.bottom - kSelectorNameH / 2.0;
	return s;
}

/** A point at `degrees` (from twelve o'clock, clockwise) and `radius`. */
inline void selectorPoint (const Selector& s, double radius, double degrees, double& x, double& y)
{
	const double t = degrees * 3.14159265358979323846 / 180.0;
	x = s.cx + radius * std::sin (t);
	y = s.cy - radius * std::cos (t);
}

/** WHERE A POSITION'S NAME IS PRINTED: hung from labelRadius along its
    angle - above the knob for the middle one, ending short of the line
    on the left, starting past it on the right. */
inline Box selectorLabelBox (const Selector& s, int index, const std::string& text)
{
	const double t = s.angle[index];
	double px = 0.0, py = 0.0;
	selectorPoint (s, s.labelRadius, t, px, py);
	const double w = Lettering::width (text, kTextLabel);
	const double h = kTextLabel.height ();
	double left = 0.0, centreY = 0.0;
	if (std::fabs (t) < 10.0)
	{
		left = px - w / 2.0;
		centreY = py - 8.0;
	}
	else if (t < 0.0)
	{
		left = px - 2.0 - w;
		centreY = py + 2.0;
	}
	else
	{
		left = px + 2.0;
		centreY = py + 2.0;
	}
	return { left, centreY - h / 2.0, left + w, centreY + h / 2.0 };
}

/** THE POSITION A CLICK PICKS: on the knob itself, -1 (the knob steps
    on); anywhere else in the selector, the position whose angle is
    nearest the click's. */
inline int selectorPositionAt (const Selector& s, double x, double y)
{
	const double dx = x - s.cx, dy = y - s.cy;
	if (dx * dx + dy * dy <= s.radius * s.radius)
		return -1;
	const double t = std::atan2 (dx, -dy) * 180.0 / 3.14159265358979323846;
	int best = 0;
	for (int i = 1; i < s.positions; ++i)
		if (std::fabs (s.angle[i] - t) < std::fabs (s.angle[best] - t))
			best = i;
	return best;
}

//------------------------------------------------------------------------
// The displays
//------------------------------------------------------------------------
constexpr Rect kFilterView    { 516, 112, 832,  208 };
constexpr Rect kFilterEnvView { 398, 344, 548,  396 };
constexpr Rect kAmpEnvView    { 888, 112, 1170, 216 };
constexpr Rect kLfoView       { 30,  506, 180,  570 };

constexpr Rect kScreens[] = { kFilterView, kFilterEnvView, kAmpEnvView, kLfoView };

/** A screen with its black cut-out round it. */
constexpr Rect bezelOf (const Rect& r)
{
	return { r.left - kScreenBezel, r.top - kScreenBezel, r.right + kScreenBezel, r.bottom + kScreenBezel };
}

/** Points per curve. The widest display is 316 pixels; 200 points is
    one every two pixels or better on all of them. */
constexpr int kDisplayPoints = 200;

/** The filter display's axes: 20 Hz..20 kHz, +18..-48 dB. */
constexpr double kFilterMinHz    = 20.0;
constexpr double kFilterMaxHz    = 20000.0;
constexpr double kFilterTopDb    = 18.0;
constexpr double kFilterBottomDb = -48.0;

//------------------------------------------------------------------------
// Fixed lettering, rules and lines of text
//------------------------------------------------------------------------
enum class CaptionStyle
{
	Block,    // a block's name, bold
	Field,    // a control's heading, bold
	Note,     // ordinary text
	Badge     // an oscillator's number, in a ring, "VCO" under it
};

struct Caption
{
	const char* text;
	CaptionStyle style;
	Accent accent;
	Rect r;
};

/** A badge's ring fills the TOP square of its rectangle; "VCO" is
    printed under it, in the rest. */
constexpr int kBadgeSize = 24;

constexpr Caption kCaptions[] = {
	{ "1", CaptionStyle::Badge, Accent::Osc, { 30, oscRowTop (0) + 34, 54, oscRowTop (0) + 70 } },
	{ "2", CaptionStyle::Badge, Accent::Osc, { 30, oscRowTop (1) + 34, 54, oscRowTop (1) + 70 } },
	{ "3", CaptionStyle::Badge, Accent::Osc, { 30, oscRowTop (2) + 34, 54, oscRowTop (2) + 70 } },

	{ "FILTER ENV", CaptionStyle::Block, Accent::Filter, { 398, 328, 548, 340 } },

	{ "LFO 1 - MS-20 STYLE",               CaptionStyle::Block, Accent::Mod, { 30,  488, 330, 500 } },
	{ "LFO 2 \xE2\x86\x92 LFO 1 SHAPE",    CaptionStyle::Block, Accent::Mod, { 552, 488, 700, 500 } },
	{ "LFO 1 DEPTH",                       CaptionStyle::Block, Accent::Mod, { 726, 488, 1108, 500 } },
};
constexpr int kNumCaptions = static_cast<int> (sizeof (kCaptions) / sizeof (kCaptions[0]));

/** Hairlines: the rules between the oscillator rows, under the filter's
    knobs and the amp's ADSR, and between the modulation blocks. */
constexpr Rect kRules[] = {
	{ 30,  oscRowTop (0), 342, oscRowTop (0) + 1 },
	{ 30,  oscRowTop (1), 342, oscRowTop (1) + 1 },
	{ 30,  oscRowTop (2), 342, oscRowTop (2) + 1 },
	{ 398, 320, 832, 321 },
	{ 888, 306, 1170, 307 },
	{ 532, 488, 533, 580 },
	{ 706, 488, 707, 580 },
};

/** The screws in the panel's four corners. */
constexpr Point kScrews[] = { { 8, 8 }, { kEditorWidth - 8, 8 }, { 8, kEditorHeight - 8 },
                              { kEditorWidth - 8, kEditorHeight - 8 } };

//------------------------------------------------------------------------
// The lines whose text changes, and the lamp
//------------------------------------------------------------------------
/** The header: the wordmark, a divider, the engine line (rate, cutoff
    ceiling, release tail), then the licence pill at the right. */
constexpr Rect kWordmark      { 22,  0,  230, kHeaderHeight };
constexpr int  kHeaderDivider = 240;
constexpr Rect kEngineLine    { 254, 20, 830, 36 };
constexpr Rect kBadge         { 840, 12, 1184, 44 };

/** The self-oscillation lamp, at the right of the filter's title row. */
constexpr Rect kSelfOscLamp { 752, 84, 832, 102 };

/** One line along the foot of the modulation strip: what LFO 1 and
    LFO 2 are doing, in words. */
constexpr Rect kModInfoLine { 30, 592, 1170, 606 };

//------------------------------------------------------------------------
// The arithmetic, asserted rather than trusted
//------------------------------------------------------------------------
static_assert (kOscRowTop == 112, "the oscillator rows start under the section title");
static_assert (oscRowTop (2) + 52 + kKnobTextBand + 6 <= kOscRect.bottom - 12,
               "the third oscillator row fits its section");
static_assert (oscRowTop (2) + 11 + 83 <= kOscRect.bottom - 12, "the third wave selector fits its section");
static_assert (kModInfoLine.bottom <= kModRect.bottom - 12, "the info line fits the modulation strip");
static_assert (kModRect.bottom + kMargin == kEditorHeight, "the strip ends a margin above the bottom");
static_assert (kAmpRect.right + kMargin == kEditorWidth, "the amp ends a margin from the right");
static_assert (kWordmark.right < kHeaderDivider && kHeaderDivider < kEngineLine.left,
               "wordmark, divider, engine line");
static_assert (kEditorWidth == 1200 && kEditorHeight == 640,
               "the panel changed size - update the banner, docs/panel.png and this assert together");

//------------------------------------------------------------------------
} // namespace Layout
} // namespace R2Synth
