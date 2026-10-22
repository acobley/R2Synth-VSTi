//------------------------------------------------------------------------
// R2Synth - the panel's views
//
// WRITTEN FOR THE "SIGNAL FLOW" PANEL (branch UpdatedUI, 4 Oct 2026) and
// REDRESSED AS THE BLACK PANEL (branch PolivoksUI, 22 Oct 2026), from
// Designs/R2Synth-Polivoks-1c-OLED.png: matte black, white printing,
// boxed sections, black knurled knobs with printed scales, chicken-head
// rotary selectors, small colour OLED screens, and every letter in
// Andy's 70s sci-fi lettering (SciFiLettering.h). They replace the
// FilterDrum controls in R2SynthControls.h on the panel itself; that
// file stays, because the licence dialog still uses its push button and
// fonts.
//
//   Backdrop      everything that never changes: the ground, the header
//                 and its wordmark, the corner screws, the four boxed
//                 sections with their titles, the chevrons, the rules,
//                 the screens' black cut-outs and the fixed captions -
//                 all read from R2SynthLayout.h. One view, behind the
//                 rest, mouse-disabled.
//   Knob          a black knurled knob with a white line, its printed
//                 scale, its reading and its label underneath.
//   RotarySwitch  a chicken-head selector with its positions printed
//                 round it; the one that is set has a red mark.
//   Scope         a small colour OLED: true black, a dot grid, crisp
//                 traces in the section's colour - the filter response,
//                 the two envelopes, the LFO.
//   Lamp          the SELF-OSC indicator, a red lamp.
//   TextLine      one line of lettering whose words change - the engine
//                 line, the modulation info line.
//
// THE SAME TWO RULES AS EVER. No bitmaps - everything is paths, the
// lettering included, so the panel is resolution-independent and
// resource/ needs no artwork. And no numbers made here: every reading
// comes in through a formatter the editor hands over (readoutText() in
// R2SynthReadouts.cpp), and every curve through setCurves() from the
// shared trace functions.
//------------------------------------------------------------------------

#pragma once

#include "R2SynthLayout.h"
#include "SciFiLettering.h"

#include "vstgui/vstgui.h"

#include <functional>
#include <string>
#include <vector>

namespace R2Synth {
namespace Panel {

//------------------------------------------------------------------------
// The palette - the design's tokens
//------------------------------------------------------------------------
namespace Palette {

const VSTGUI::CColor kGround     ( 18,  18,  18, 255);
const VSTGUI::CColor kHeader     ( 12,  12,  12, 255);
const VSTGUI::CColor kHeaderRule (233, 230, 220, 255);
const VSTGUI::CColor kSection    ( 21,  21,  21, 255);
const VSTGUI::CColor kSectionEdge(217, 214, 204, 255);   // box lines, chevrons, rings
const VSTGUI::CColor kInk        (244, 241, 232, 255);   // ALL the lettering: white on black
const VSTGUI::CColor kInkSoft    (185, 182, 174, 255);   // subtitles, notes
const VSTGUI::CColor kInkDim     (143, 140, 133, 255);   // a selector's positions not set
const VSTGUI::CColor kDivider    ( 90,  88,  82, 255);
const VSTGUI::CColor kRule       ( 58,  58,  58, 255);
const VSTGUI::CColor kTick       (233, 230, 220, 255);   // the printed scales
const VSTGUI::CColor kRed        (226,  52,  43, 255);   // the position that is set
const VSTGUI::CColor kScrew      ( 58,  58,  58, 255);
const VSTGUI::CColor kKnobBody   ( 10,  10,  10, 255);
const VSTGUI::CColor kKnobRim    ( 47,  47,  47, 255);
const VSTGUI::CColor kBezel      (  5,   5,   5, 255);
const VSTGUI::CColor kBezelEdge  ( 60,  60,  60, 255);
const VSTGUI::CColor kScreen     (  0,   0,   0, 255);
const VSTGUI::CColor kGridDot    ( 42,  45,  51, 255);
const VSTGUI::CColor kScreenText (141, 150, 163, 255);
const VSTGUI::CColor kCompanion  (141, 150, 163, 150);   // the second trace on a screen
const VSTGUI::CColor kLampOn     (255,  59,  47, 255);
const VSTGUI::CColor kLampOff    ( 58,  21,  18, 255);

/** The names the editor and the licence views used before the black
    panel, kept so they still mean something sensible. */
const VSTGUI::CColor kText   = kInk;
const VSTGUI::CColor kMuted  = kInkSoft;
const VSTGUI::CColor kSubtle = kInkSoft;
const VSTGUI::CColor kDim    = kInkDim;

/** A section's SCREEN colour: filter blue, amp green, modulation violet
    (the oscillators have no screen: white). */
VSTGUI::CColor accent (Layout::Accent a);

/** The same at `alpha` (0..255). */
VSTGUI::CColor accentAlpha (Layout::Accent a, uint8_t alpha);

} // namespace Palette

//------------------------------------------------------------------------
// The lettering
//------------------------------------------------------------------------
enum class Align
{
	Left,
	Centre,
	Right
};

/** DRAW A RUN OF LETTERING: `text` in `style`, its ink box vertically
    centred on `centreY`, starting at, centred on or ending at `x`.
    `strokeUnits` > 0 strokes the same glyphs at another weight without
    moving them - the wordmark's inline cut. */
void drawLettering (VSTGUI::CDrawContext* context, const std::string& text, const Lettering::Style& style,
                    const VSTGUI::CColor& colour, VSTGUI::CCoord x, VSTGUI::CCoord centreY,
                    Align align = Align::Left, double strokeUnits = -1.0);

/** The faces the licence DIALOG still draws with (it keeps its old
    look). The panel itself draws no font text. */
VSTGUI::CFontRef fontTitle ();      // Helvetica Neue, bold 13
VSTGUI::CFontRef fontValue ();      // Menlo 10

/** A string in capitals. ASCII only, which is all the labels are. */
std::string upper (const std::string& s);

//------------------------------------------------------------------------
/** Everything that never changes, from the layout tables, in one view
    behind the others. Mouse-disabled. */
class Backdrop : public VSTGUI::CView
{
public:
	explicit Backdrop (const VSTGUI::CRect& size);
	void draw (VSTGUI::CDrawContext* context) override;
	CLASS_METHODS (Backdrop, VSTGUI::CView)
};

//------------------------------------------------------------------------
/** A ROTARY KNOB: black, knurled, a white line, a printed scale of
    eleven ticks over 270 degrees (0 and 10 at the ends, or - and + on a
    BIPOLAR knob - Tune, Shape, Env Amt).

    THE WHOLE RECTANGLE IS THE TARGET, reading and label included.

      drag       vertically; up increases; 200 pixels is the whole range
      Shift      ten times finer, drag and wheel alike
      double-click   the parameter's default (setDefaultValue)
      wheel      one per cent a click

    Relative, like the old sliders: a click never jumps the value to the
    pointer. */
class Knob : public VSTGUI::CControl
{
public:
	Knob (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag,
	      Layout::Accent accent, int diameter, bool bipolar);

	void setLabel (const std::string& label) { mLabel = upper (label); }

	/** The reading, given the NORMALISED value - the editor hands over
	    readoutText() so the panel and the host cannot disagree. */
	void setFormatter (std::function<std::string (float)> formatter) { mFormatter = std::move (formatter); }

	void draw (VSTGUI::CDrawContext* context) override;

	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseMoveEvent (VSTGUI::MouseMoveEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseCancelEvent (VSTGUI::MouseCancelEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (Knob, VSTGUI::CControl)

	/** The angle, in VSTGUI's degrees (0 = three o'clock, clockwise),
	    of a normalised value: 135 at 0, 270 (the top) at 0.5, 405 at 1. */
	static double angleOf (double normalized) { return 135.0 + 270.0 * normalized; }

private:
	/** Its section. Every knob on the black panel is black and white,
	    so nothing is coloured by it today; kept so a knob still knows. */
	[[maybe_unused]] Layout::Accent mAccent;
	int  mDiameter;
	bool mBipolar;
	std::string mLabel;
	std::function<std::string (float)> mFormatter;

	bool mDragging = false;
	VSTGUI::CCoord mLastY = 0.;
};

//------------------------------------------------------------------------
/** A ROTARY SELECTOR - every switch on the panel, two positions or
    three: a chicken-head knob, its positions printed round it (the one
    that is set in white with a red mark, the others dim), its name under
    it. Geometry from Layout::selectorOf(), which the tests read too.

      click a position's name (or anywhere off the knob towards it)
                 that position
      click the knob     the next position, round to the first again
      wheel      steps

    Positions are spread evenly over 0..1, the way an Enum parameter's
    normalised values are. */
class RotarySwitch : public VSTGUI::CControl
{
public:
	RotarySwitch (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag,
	              const Layout::Control& layout);

	void setNames (const std::vector<std::string>& names);
	void setLabel (const std::string& label) { mLabel = upper (label); }

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (RotarySwitch, VSTGUI::CControl)

private:
	int count () const { return static_cast<int> (mNames.size ()); }
	int currentIndex () const;
	void select (int index);

	Layout::Control mLayout;
	Layout::Selector mGeometry;
	std::vector<std::string> mNames;
	std::string mLabel;
};

//------------------------------------------------------------------------
/** A SCREEN - a small colour OLED: a true-black glass with a dot grid, a
    caption top left, a figure top right in the trace's colour, and up to
    two traces. Its black cut-out in the panel is the Backdrop's.

    The FIRST trace is the main one - crisp, in its colour, with a slight
    bloom and, when setFill (true), a fill that fades from the line to
    black. The SECOND is a companion, thin and grey (the knobs' curve
    behind the live one; the LFO's unpatched output). Both are 0..1, y
    up, and CLAMPED rather than scaled: the caller fixes the scale, so
    one curve cannot rescale the picture.

    Optional extras: a white ring on the first trace (the filter's
    cutoff), a dotted vertical line (where the key went up), a legend
    along the foot. Mouse-disabled. */
class Scope : public VSTGUI::CView
{
public:
	explicit Scope (const VSTGUI::CRect& size);

	void setColours (const VSTGUI::CColor& main, const VSTGUI::CColor& companion);
	void setFill (bool fill) { mFill = fill; }
	void setCurves (const float* main, const float* companion, int count);
	void setCaption (const std::string& caption) { mCaption = caption; }
	void setAnnotation (const std::string& annotation) { mAnnotation = annotation; }
	void setLegend (const std::string& main, const std::string& companion);

	/** The ring, at (x, y) 0..1 of the plot; negative x for none. */
	void setMarker (double x, double y) { mMarkerX = x; mMarkerY = y; }

	/** A dotted vertical line at `fraction` of the width; negative for
	    none. */
	void setGateMarker (double fraction) { mGate = fraction; }

	void draw (VSTGUI::CDrawContext* context) override;

	CLASS_METHODS (Scope, VSTGUI::CView)

private:
	VSTGUI::CRect plotRect () const;
	void drawTrace (VSTGUI::CDrawContext* context, const std::vector<float>& data,
	                const VSTGUI::CColor& colour, VSTGUI::CCoord width, bool fill, bool bloom) const;

	VSTGUI::CColor mMain;
	VSTGUI::CColor mCompanion;
	bool mFill = false;
	std::vector<float> mMainData;
	std::vector<float> mCompanionData;
	std::string mCaption;
	std::string mAnnotation;
	std::string mMainLegend;
	std::string mCompanionLegend;
	double mMarkerX = -1.0;
	double mMarkerY = 0.0;
	double mGate = -1.0;
};

//------------------------------------------------------------------------
/** The SELF-OSC lamp: a red lamp that lights when the resonance is at
    loop gain 1 or more (selfOscillating()), and its name beside it. */
class Lamp : public VSTGUI::CView
{
public:
	explicit Lamp (const VSTGUI::CRect& size, const std::string& name);
	void setOn (bool on);
	bool isOn () const { return mOn; }
	void draw (VSTGUI::CDrawContext* context) override;
	CLASS_METHODS (Lamp, VSTGUI::CView)

private:
	std::string mName;
	bool mOn = false;
};

//------------------------------------------------------------------------
/** ONE LINE OF LETTERING whose words change - the engine line in the
    header, the info line under the modulation strip. Left-aligned,
    vertically centred in the view. Mouse-disabled. */
class TextLine : public VSTGUI::CView
{
public:
	TextLine (const VSTGUI::CRect& size, const Lettering::Style& style, const VSTGUI::CColor& colour);
	void setText (const std::string& text);
	const std::string& getText () const { return mText; }
	void draw (VSTGUI::CDrawContext* context) override;
	CLASS_METHODS (TextLine, VSTGUI::CView)

private:
	Lettering::Style mStyle;
	VSTGUI::CColor mColour;
	std::string mText;
};

//------------------------------------------------------------------------
} // namespace Panel
} // namespace R2Synth
