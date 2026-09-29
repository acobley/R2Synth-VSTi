//------------------------------------------------------------------------
// SimpleSynth - the panel's layout, as data
//
// THE LAYOUT IS DATA, not positions typed into the middle of open(). It
// lives in this header, which includes NO VSTGUI, so that three things
// can read the same numbers:
//
//   * SimpleSynthEditor::open(), which builds the panel from the tables;
//   * tests/LayoutTests.cpp, which checks that nothing overlaps and
//     everything fits - the only way to test a layout from a session
//     that cannot run a host;
//   * tools/dump-layout.cpp + tools/render-panel.py, which draw
//     docs/panel.png from these tables.
//
// It needs SimpleSynthParams.h for the tags, and that needs one header-
// only file from the SDK (pluginterfaces/vst/vsttypes.h) - so the layout
// test is compiled with -I<sdk>, unlike the DSP suite. Nothing is linked.
//
// THE GRID is FilterDrum's: 94-pixel columns eight apart, 44-pixel
// sliders, every group of controls in a SpyGroupBox with its name on the
// top edge. Four rows, in signal order:
//
//   row 0   VCO 1        | VCO 2        | VCO 3          | LFO display
//   row 1   LFO 1              | LFO 2    | readouts    |   (rows 0-1)
//   row 2   VCF (five)           | VCF ENV              | ENV display
//   row 3   VCA / OUTPUT         | VCA ENV              |   (rows 2-3)
//
// LFO 2 (29 Sep 2026) took the first two columns of what had been the
// readout area, so the readouts are three short lines now, all about
// the two LFOs; the engine line moved up to the title row, and the
// note about retriggering into the signal-flow line. THE PANEL DID NOT
// GROW: the same 1176 x 302.
//
// LFO 1 -> RESONANCE (29 Sep 2026, later) widened the VCF group to five
// columns so the depth sits at its destination next to the cutoff
// depth, and the two ADSR groups moved one column right with it. Rows
// 2-3 are now the widest (ten columns, one boundary), so kRowRight is
// theirs and THE PANEL GREW 94 pixels (one column): 1270 x 302.
//
// The two ADSR groups are one above the other so Attack sits over
// Attack; the VCF and VCA groups likewise, so the two LFO depth knobs
// that live at their destinations - the MS-20's way - are in the same
// column.
//------------------------------------------------------------------------

#pragma once

#include "SimpleSynthParams.h"

namespace SimpleSynth {
namespace Layout {

//------------------------------------------------------------------------
// The grid
//------------------------------------------------------------------------
constexpr int kMargin       = 16;
constexpr int kColumnWidth  = 94;
constexpr int kColumnGap    = 8;
constexpr int kColumnPitch  = kColumnWidth + kColumnGap;
constexpr int kSliderHeight = 44;

constexpr int kTitleY       = 10;
constexpr int kFlowY        = 30;
constexpr int kLabelHeight  = 16;

/** THE TITLE IS INSIDE THE BOX'S VIEW, and the frame's top edge is
    drawn this far down it, through the middle of the title.

    It was FilterDrum's way round: the view's top WAS the frame's top
    edge and the title was drawn 5 pixels above it - outside the view,
    where VSTGUI clips every view's drawing to its own rectangle. So the
    top half of every title was cut off in the plug-in (render-panel.py
    does not clip, which is why the picture never showed it). Now the
    view starts at the top of the lettering and SpyGroupBox::
    setTitleBand() puts the frame line through it, so the whole title is
    inside what VSTGUI lets the box draw. */
constexpr int kGroupTitleBand = 5;

constexpr int kGroupPadX      = 5;    // group box -> the sliders in it
constexpr int kGroupPadTop    = 11;   // view top -> sliders: the whole title, then clear of it
constexpr int kGroupPadBottom = 3;
constexpr int kGroupGap       = 6;    // between two group boxes side by side
constexpr int kRowGap         = 2;    // between one row of boxes and the next

/** The height of a group box's VIEW. The frame drawn in it is
    kGroupTitleBand shorter: 53 pixels round a 44-pixel slider. */
constexpr int kGroupHeight  = kSliderHeight + kGroupPadTop + kGroupPadBottom;
constexpr int kRowPitch     = kGroupHeight + kRowGap;

/** Column 0's left edge. */
constexpr int kContentX     = kMargin + kGroupPadX;

/** WHAT A GROUP BOUNDARY COSTS. Two boxes side by side need their
    padding and a gap between them, which is more than the gap between
    two columns - so every group to the left of a column pushes it
    right by this much. A column's x is therefore (column, slot), where
    slot is how many group boundaries lie to its left. */
constexpr int kGroupSpacer  = 2 * kGroupPadX + kGroupGap - kColumnGap;

constexpr int columnX (int column, int slot)
{
	return kContentX + column * kColumnPitch + slot * kGroupSpacer;
}

/** The top of row `row`'s sliders. */
constexpr int kRow0Y = kFlowY + kLabelHeight + 2 + kGroupPadTop;

constexpr int rowY (int row)
{
	return kRow0Y + row * kRowPitch;
}

/** The right edge of the widest row - three VCO groups of three - and
    therefore of every row: a row with fewer boxes stretches its last
    box to here, so the right-hand edges line up and the strip of
    displays beside them is one straight edge. */
constexpr int kRowRight = ((columnX (9, 2) > columnX (10, 1)) ? columnX (9, 2) : columnX (10, 1))
                         - kColumnGap + kGroupPadX;

//------------------------------------------------------------------------
// The displays, in a strip to the right, each spanning two rows
//------------------------------------------------------------------------
constexpr int kDisplayGap   = 8;
constexpr int kDisplayX     = kRowRight + kDisplayGap;
constexpr int kDisplayWidth = 200;

/** Lined up with the GROUP BOXES' FRAMES either side, not with the
    sliders or the boxes' views, so the strip and the drawn boxes share a
    top and a bottom edge. */
constexpr int kLfoViewTop    = rowY (0) - kGroupPadTop + kGroupTitleBand;
constexpr int kLfoViewBottom = rowY (1) + kSliderHeight + kGroupPadBottom;
constexpr int kEnvViewTop    = rowY (2) - kGroupPadTop + kGroupTitleBand;
constexpr int kEnvViewBottom = rowY (3) + kSliderHeight + kGroupPadBottom;

/** Points per curve - one per pixel of plot is the most that shows. */
constexpr int kDisplayPoints = 200;

//------------------------------------------------------------------------
// The readout lines, in row 1 to the right of the LFO 2 group
//------------------------------------------------------------------------
constexpr int kReadoutX     = columnX (6, 2);
constexpr int kReadoutWidth = kRowRight - kReadoutX;
constexpr int kReadout1Y    = rowY (1) - 4;
constexpr int kReadout2Y    = kReadout1Y + 16;
constexpr int kReadout3Y    = kReadout2Y + 16;

//------------------------------------------------------------------------
// The panel
//------------------------------------------------------------------------
constexpr int kEditorWidth  = kDisplayX + kDisplayWidth + kMargin;
constexpr int kEditorHeight = kEnvViewBottom + kMargin;

/** The title row: the title from the left, the engine line from the
    readouts' x to the right margin. */
constexpr int kEngineLineX     = kReadoutX;
constexpr int kEngineLineWidth = kEditorWidth - kMargin - kEngineLineX;
constexpr int kTitleWidth      = kEngineLineX - kColumnGap - kContentX;

/** The flow line runs the full width. */
constexpr int kFlowWidth       = kEditorWidth - kMargin - kContentX;

/** THE LICENCE BADGE (LicenceViews.h): one tiny line in the bottom
    margin, right-aligned under the display strip - "Evaluation - 5 days
    left - licence..." or "Licensed to ...". Click it for the licence
    dialog. The panel did not grow for it. */
constexpr int kBadgeX      = kDisplayX;
constexpr int kBadgeWidth  = kDisplayWidth;
constexpr int kBadgeY      = kEnvViewBottom + 2;
constexpr int kBadgeHeight = 12;

//------------------------------------------------------------------------
// The tables
//------------------------------------------------------------------------
enum class Kind
{
	Slider,      // SpySlider - drag left and right
	Selector     // SpySelector - click steps through named positions
};

struct Control
{
	Steinberg::Vst::ParamID tag;
	int  row;
	int  column;
	int  slot;
	Kind kind;
};

struct Group
{
	const char* title;
	int  row;
	int  firstColumn;
	int  columns;
	int  slot;
	bool stretchToRowRight;   // see kRowRight
};

/** Every control on the panel. THE VCOs ARE ONE ROW OF THREE IDENTICAL
    GROUPS because the three VCOs are one implementation - see
    vcoParam() - so a difference between them on the panel has to be
    real. */
constexpr Control kControls[] = {
	{ kVco1Wave,     0, 0, 0, Kind::Selector },
	{ kVco1Tune,     0, 1, 0, Kind::Slider   },
	{ kVco1Level,    0, 2, 0, Kind::Slider   },
	{ kVco2Wave,     0, 3, 1, Kind::Selector },
	{ kVco2Tune,     0, 4, 1, Kind::Slider   },
	{ kVco2Level,    0, 5, 1, Kind::Slider   },
	{ kVco3Wave,     0, 6, 2, Kind::Selector },
	{ kVco3Tune,     0, 7, 2, Kind::Slider   },
	{ kVco3Level,    0, 8, 2, Kind::Slider   },

	// The LFO's own three, then its PITCH depth - the one destination
	// that has no group of its own to sit in, because it moves all three
	// VCOs at once.
	{ kLfoRate,      1, 0, 0, Kind::Slider   },
	{ kLfoShape,     1, 1, 0, Kind::Slider   },
	{ kLfoOutput,    1, 2, 0, Kind::Selector },
	{ kLfoPitch,     1, 3, 0, Kind::Slider   },

	// LFO 2 BESIDE LFO 1, because its one destination is LFO 1's Shape.
	{ kLfo2Rate,     1, 4, 1, Kind::Slider   },
	{ kLfo2Level,    1, 5, 1, Kind::Slider   },

	{ kCutoff,       2, 0, 0, Kind::Slider   },
	{ kResonance,    2, 1, 0, Kind::Slider   },
	{ kKeyTrack,     2, 2, 0, Kind::Slider   },
	{ kVcfLfo,       2, 3, 0, Kind::Slider   },
	{ kVcfLfoRes,    2, 4, 0, Kind::Slider   },   // LFO 1 -> resonance, beside the cutoff depth
	{ kVcfAttack,    2, 5, 1, Kind::Slider   },
	{ kVcfDecay,     2, 6, 1, Kind::Slider   },
	{ kVcfSustain,   2, 7, 1, Kind::Slider   },
	{ kVcfRelease,   2, 8, 1, Kind::Slider   },
	{ kVcfEnvAmount, 2, 9, 1, Kind::Slider   },

	// The VCA's LFO depth directly under the VCF's, so the two depth
	// knobs that live at their destinations share a column. The Output
	// Level takes column 0, under Cutoff: the two knobs reached for
	// first sit at the left edge, one above the other.
	{ kVcaLfo,       3, 3, 0, Kind::Slider   },
	{ kOutputLevel,  3, 0, 0, Kind::Slider   },
	{ kVcaAttack,    3, 5, 1, Kind::Slider   },
	{ kVcaDecay,     3, 6, 1, Kind::Slider   },
	{ kVcaSustain,   3, 7, 1, Kind::Slider   },
	{ kVcaRelease,   3, 8, 1, Kind::Slider   },
};

constexpr int kNumControls = static_cast<int> (sizeof (kControls) / sizeof (kControls[0]));

constexpr Group kGroups[] = {
	{ "VCO 1",        0, 0, 3, 0, false },
	{ "VCO 2",        0, 3, 3, 1, false },
	{ "VCO 3",        0, 6, 3, 2, true  },
	{ "LFO 1",        1, 0, 4, 0, false },
	{ "LFO 2 -> SHAPE", 1, 4, 2, 1, false },
	{ "VCF",          2, 0, 5, 0, false },
	{ "VCF ENV",      2, 5, 5, 1, true  },
	{ "VCA / OUTPUT", 3, 0, 5, 0, false },
	{ "VCA ENV",      3, 5, 5, 1, true  },
};

constexpr int kNumGroups = static_cast<int> (sizeof (kGroups) / sizeof (kGroups[0]));

//------------------------------------------------------------------------
/** A rectangle, left/top/right/bottom, in panel pixels. */
struct Rect
{
	int left, top, right, bottom;
};

constexpr Rect controlRect (const Control& c)
{
	return { columnX (c.column, c.slot), rowY (c.row),
	         columnX (c.column, c.slot) + kColumnWidth, rowY (c.row) + kSliderHeight };
}

constexpr Rect groupRect (const Group& g)
{
	const int left  = columnX (g.firstColumn, g.slot) - kGroupPadX;
	const int right = g.stretchToRowRight
	                    ? kRowRight
	                    : columnX (g.firstColumn + g.columns, g.slot) - kColumnGap + kGroupPadX;
	return { left, rowY (g.row) - kGroupPadTop, right, rowY (g.row) + kSliderHeight + kGroupPadBottom };
}

//------------------------------------------------------------------------
// The arithmetic, asserted rather than trusted
//------------------------------------------------------------------------
static_assert (kGroupSpacer > 0, "two group boxes side by side must not overlap");
static_assert (kReadout3Y + kLabelHeight <= kLfoViewBottom,
               "the readout lines must fit in row 1");
static_assert (kGroupPadTop >= 11,
               "a slider's reading must start below the title (11 pixels of lettering)");
static_assert (kGroupTitleBand > 0 && kGroupTitleBand < kGroupPadTop,
               "the frame line must run through the title, above the sliders");
static_assert (kBadgeY + kBadgeHeight <= kEditorHeight,
               "the licence badge must fit in the bottom margin");
static_assert (kEditorWidth == 1270 && kEditorHeight == 302,
               "the panel changed size - update the banner, docs/panel.png and this assert together");

//------------------------------------------------------------------------
} // namespace Layout
} // namespace SimpleSynth
