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
// design Designs/R2Synth-ConceptA-SignalFlow.png. It replaces the
// FilterDrum grid of bar sliders: the same 30 parameters, laid out the
// way the sound travels. (Portamento, 6 Oct, joined Output in the Amp
// section: the 31st. The VCF Type switch, 7 Oct, took the Filter
// section's subtitle place: the 32nd.)
//
//   +--------------------------------------------------------------+
//   | R2SYNTH | engine line                     [licence pill] |  header
//   +--------------------------------------------------------------+
//   | OSCILLATORS  >  FILTER (curve, 4 knobs,   >  AMP (envelope,  |
//   | 3 rows: wave,    filter env + ADSR)           ADSR, output)   |
//   +--------------------------------------------------------------+
//   | MODULATION: LFO 1 (scope, rate, shape, output) | LFO 2 |      |
//   |             LFO 1 depth: pitch, cutoff, resonance, amp        |
//   +--------------------------------------------------------------+
//
// ONE COLOUR PER SECTION (oscillators amber, filter blue, amp mint,
// modulation violet), carried by the knobs' value arcs, so a knob's
// colour says which part of the voice it moves. THE FOUR LFO 1 DEPTHS
// ARE TOGETHER in the modulation strip, in violet, rather than each at
// its destination as on the MS-20 grid: one place to see everything the
// LFO does.
//
// KNOBS, NOT BAR SLIDERS. A knob is drawn top-centred in its rectangle
// with its reading and its label under it, and the WHOLE rectangle takes
// the drag, so the text is part of the target. Vertical drag; Shift is
// fine; double-click returns the default; the wheel steps.
//
// The panel is 1200 x 640 - bigger than the old 1270 x 302 strip, and
// closer to the shape of a screen.
//------------------------------------------------------------------------

#pragma once

#include "R2SynthParams.h"

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
constexpr int kSectionRadius   = 10;

/** Under every knob: two pixels, the reading, the label. */
constexpr int kKnobGap       = 2;
constexpr int kKnobValueH    = 13;
constexpr int kKnobLabelH    = 12;
constexpr int kKnobTextBand  = kKnobGap + kKnobValueH + kKnobLabelH;

/** A segmented switch's height - and the shorter one that fits a
    section's title row (the VCF Type switch). */
constexpr int kSwitchHeight = 30;
constexpr int kTitleSwitchHeight = 24;

/** CHARACTER WIDTHS for the fit tests - generous averages for the faces
    the editor asks for (Menlo 10 for readings, Helvetica Neue bold 10
    in capitals for labels). The tests use them to check that the widest
    reading of every knob fits the knob's rectangle. */
constexpr int kValueCharWidth  = 6;
constexpr int kLabelCharWidth  = 7;
constexpr int kNoteCharWidth   = 6;

//------------------------------------------------------------------------
// Sections
//------------------------------------------------------------------------
enum class Accent
{
	Osc,      // amber
	Filter,   // blue
	Amp,      // mint
	Mod       // violet
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
	// No subtitle: the VCF Type switch sits where it would be.
	{ "FILTER",      "",                              Accent::Filter, kFilterRect },
	{ "AMP",         "envelope + output",             Accent::Amp,    kAmpRect    },
	{ "MODULATION",  "",                              Accent::Mod,    kModRect    },
};
constexpr int kNumSections = static_cast<int> (sizeof (kSections) / sizeof (kSections[0]));

/** The top of a section's contents: under its title row. */
constexpr int contentTop (const Rect& section)
{
	return section.top + kSectionPadTop + kSectionTitleH + 10;
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
	Knob,     // PanelKnob - vertical drag
	Switch    // PanelSwitch - one segment per named position
};

struct Control
{
	Steinberg::Vst::ParamID tag;
	Kind   kind;
	Accent accent;
	int    x, y;        // top left of the control's rectangle
	int    width;
	int    diameter;    // knobs only; 0 for a switch
	bool   bipolar;     // the arc grows from the top, not from the left end
	const char* label;  // nullptr: shortLabel() - the title less its section
	int    height = 0;  // switches only: 0 = kSwitchHeight
};

constexpr Rect controlRect (const Control& c)
{
	return { c.x, c.y, c.x + c.width,
	         c.y + ((c.kind == Kind::Knob) ? c.diameter + kKnobTextBand
	                                       : (c.height > 0 ? c.height : kSwitchHeight)) };
}

/** EVERY PARAMETER ON THE PANEL, once each - the tests hold it to that. */
constexpr Control kControls[] = {
	// ---- the oscillators: one row each, the same three controls -------
	{ kVco1Wave,  Kind::Switch, Accent::Osc, 64,  oscRowTop (0) + 31, 122, 0,  false, nullptr },
	{ kVco1Tune,  Kind::Knob,   Accent::Osc, 210, oscRowTop (0) + 6,  64,  52, true,  nullptr },
	{ kVco1Level, Kind::Knob,   Accent::Osc, 278, oscRowTop (0) + 6,  64,  52, false, nullptr },
	{ kVco2Wave,  Kind::Switch, Accent::Osc, 64,  oscRowTop (1) + 31, 122, 0,  false, nullptr },
	{ kVco2Tune,  Kind::Knob,   Accent::Osc, 210, oscRowTop (1) + 6,  64,  52, true,  nullptr },
	{ kVco2Level, Kind::Knob,   Accent::Osc, 278, oscRowTop (1) + 6,  64,  52, false, nullptr },
	{ kVco3Wave,  Kind::Switch, Accent::Osc, 64,  oscRowTop (2) + 31, 122, 0,  false, nullptr },
	{ kVco3Tune,  Kind::Knob,   Accent::Osc, 210, oscRowTop (2) + 6,  64,  52, true,  nullptr },
	{ kVco3Level, Kind::Knob,   Accent::Osc, 278, oscRowTop (2) + 6,  64,  52, false, nullptr },

	// ---- the filter's type, in its title row where the subtitle was:
	// the first thing to choose about a filter, and it reads as one
	// ("FILTER  [Moog | MS-20 I | MS-20 II]"). Shorter than the other
	// switches so it fits the row above the screen.
	{ kVcfType,       Kind::Switch, Accent::Filter, 478, 81, 228, 0, false, nullptr, kTitleSwitchHeight },

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

	// Portamento: a VOICE control, so it sits with Output and the note on
	// how the voice plays, in the Amp section's colour (the colour is the
	// section's - the tests hold every control to that).
	{ kPortamento,  Kind::Knob, Accent::Amp, 972,  316, 76, 56, false, nullptr },

	// ---- LFO 1 ---------------------------------------------------------
	{ kLfoRate,   Kind::Knob,   Accent::Mod, 192, 500, 64,  52, false, nullptr },
	{ kLfoShape,  Kind::Knob,   Accent::Mod, 262, 500, 64,  52, true,  nullptr },
	{ kLfoOutput, Kind::Switch, Accent::Mod, 338, 528, 174, 0,  false, nullptr },

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
// The displays
//------------------------------------------------------------------------
constexpr Rect kFilterView    { 398, 112, 832,  208 };
constexpr Rect kFilterEnvView { 398, 344, 548,  396 };
constexpr Rect kAmpEnvView    { 888, 112, 1170, 216 };
constexpr Rect kLfoView       { 30,  506, 180,  570 };

/** Points per curve. The widest display is 434 pixels; 200 points is
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
	Block,    // a block's name, small capitals in the section's colour
	Field,    // a control's heading, small capitals, grey
	Note,     // ordinary text, grey
	Badge     // an oscillator's number, in a disc
};

struct Caption
{
	const char* text;
	CaptionStyle style;
	Accent accent;
	Rect r;
};

constexpr Caption kCaptions[] = {
	{ "1", CaptionStyle::Badge, Accent::Osc, { 30, oscRowTop (0) + 34, 54, oscRowTop (0) + 58 } },
	{ "2", CaptionStyle::Badge, Accent::Osc, { 30, oscRowTop (1) + 34, 54, oscRowTop (1) + 58 } },
	{ "3", CaptionStyle::Badge, Accent::Osc, { 30, oscRowTop (2) + 34, 54, oscRowTop (2) + 58 } },

	{ "FILTER ENV", CaptionStyle::Block, Accent::Filter, { 398, 328, 548, 340 } },

	{ "Mono, last note",        CaptionStyle::Note, Accent::Amp, { 1058, 326, 1170, 340 } },
	{ "wins. Every note",       CaptionStyle::Note, Accent::Amp, { 1058, 342, 1170, 356 } },
	{ "retriggers both",        CaptionStyle::Note, Accent::Amp, { 1058, 358, 1170, 372 } },
	{ "envelopes.",             CaptionStyle::Note, Accent::Amp, { 1058, 374, 1170, 388 } },

	{ "LFO 1 - MS-20 STYLE",   CaptionStyle::Block, Accent::Mod, { 30,  488, 330, 500 } },
	{ "OUTPUT",                CaptionStyle::Field, Accent::Mod, { 338, 512, 512, 524 } },
	{ "LFO 2 -> LFO 1 SHAPE",  CaptionStyle::Block, Accent::Mod, { 552, 488, 700, 500 } },
	{ "LFO 1 DEPTH",           CaptionStyle::Block, Accent::Mod, { 726, 488, 1108, 500 } },
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

//------------------------------------------------------------------------
// The lines whose text changes, and the lamp
//------------------------------------------------------------------------
/** The header: the wordmark, then the engine line (rate, cutoff
    ceiling, release tail), then the licence pill at the right. */
constexpr Rect kWordmark   { 20,  0,  210, kHeaderHeight };
constexpr Rect kEngineLine { 230, 20, 830, 36 };
constexpr Rect kBadge      { 840, 12, 1184, 44 };

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
static_assert (kModInfoLine.bottom <= kModRect.bottom - 12, "the info line fits the modulation strip");
static_assert (kModRect.bottom + kMargin == kEditorHeight, "the strip ends a margin above the bottom");
static_assert (kAmpRect.right + kMargin == kEditorWidth, "the amp ends a margin from the right");
static_assert (kEditorWidth == 1200 && kEditorHeight == 640,
               "the panel changed size - update the banner, docs/panel.png and this assert together");

//------------------------------------------------------------------------
} // namespace Layout
} // namespace R2Synth
