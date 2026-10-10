//------------------------------------------------------------------------
// R2Synth - the "signal flow" panel's views
//
// WRITTEN FOR THE UPDATED PANEL (branch UpdatedUI, 4 Oct 2026), from the
// design in Designs/R2Synth-ConceptA-SignalFlow.png. They replace
// the FilterDrum controls in R2SynthControls.h on the panel itself;
// that file stays, unchanged, because the licence dialog still uses its
// push button and fonts.
//
//   Backdrop       everything that never changes: the ground, the
//                  header band, the four sections with their titles,
//                  the chevrons, the rules and the fixed captions - all
//                  read from R2SynthLayout.h. One view, behind the
//                  rest, mouse-disabled.
//   Knob           a rotary control with a value arc in its section's
//                  colour, its reading and its label underneath.
//   SegmentSwitch  one segment per named position; click one.
//   Scope          a dark screen with a grid and one or two traces -
//                  the filter response, the two envelopes, the LFO.
//   Lamp           the SELF-OSC indicator.
//
// THE SAME TWO RULES AS EVER. No bitmaps - everything is paths and text,
// so the panel is resolution-independent and resource/ needs no artwork.
// And no numbers made here: every reading comes in through a formatter
// the editor hands over (readoutText() in R2SynthReadouts.cpp), and
// every curve through setCurves() from the shared trace functions.
//------------------------------------------------------------------------

#pragma once

#include "R2SynthLayout.h"

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

const VSTGUI::CColor kGround    ( 20,  21,  24, 255);
const VSTGUI::CColor kHeader    ( 26,  28,  32, 255);
const VSTGUI::CColor kBorder    ( 42,  45,  51, 255);
const VSTGUI::CColor kSection   ( 28,  30,  34, 255);
const VSTGUI::CColor kRule      ( 36,  38,  43, 255);
const VSTGUI::CColor kTrack     ( 46,  49,  55, 255);
const VSTGUI::CColor kCap       ( 42,  44,  49, 255);
const VSTGUI::CColor kCapEdge   ( 60,  64,  71, 255);
const VSTGUI::CColor kText      (236, 234, 228, 255);
const VSTGUI::CColor kTitle     (201, 204, 210, 255);
const VSTGUI::CColor kMuted     (156, 161, 170, 255);
const VSTGUI::CColor kSubtle    (126, 131, 140, 255);
const VSTGUI::CColor kDim       ( 74,  78,  86, 255);
const VSTGUI::CColor kScreen    ( 15,  16,  18, 255);
const VSTGUI::CColor kScreenEdge( 38,  41,  46, 255);
const VSTGUI::CColor kGrid      ( 26,  29,  33, 255);
const VSTGUI::CColor kSegBack   ( 18,  19,  22, 255);
const VSTGUI::CColor kLampOn    (255, 138,  61, 255);
const VSTGUI::CColor kLampOff   ( 58,  42,  32, 255);
const VSTGUI::CColor kLampEdge  ( 90,  58,  40, 255);

/** A section's colour: osc amber, filter blue, amp mint, mod violet. */
VSTGUI::CColor accent (Layout::Accent a);

/** The same at `alpha` (0..255), for fills under a trace. */
VSTGUI::CColor accentAlpha (Layout::Accent a, uint8_t alpha);

/** The text drawn ON an accent-coloured segment: a near-black of the
    same hue, so a lit segment reads at a glance. */
VSTGUI::CColor accentInk (Layout::Accent a);

} // namespace Palette

//------------------------------------------------------------------------
// The faces. On macOS CoreText falls back to the system face if one is
// missing, so a name here can never leave text undrawn.
//------------------------------------------------------------------------
VSTGUI::CFontRef fontWordmark ();   // Avenir Next Condensed, bold 23
VSTGUI::CFontRef fontTitle ();      // Helvetica Neue, bold 13
VSTGUI::CFontRef fontSubtitle ();   // Helvetica Neue 12
VSTGUI::CFontRef fontLabel ();      // Helvetica Neue, bold 10 - capitals
VSTGUI::CFontRef fontValue ();      // Menlo 10 - every reading
VSTGUI::CFontRef fontNote ();       // Helvetica Neue 11
VSTGUI::CFontRef fontBadge ();      // Menlo, bold 12

/** A string in capitals - labels are stored as words and drawn as
    small capitals. ASCII only, which is all the labels are. */
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
/** A ROTARY KNOB. The arc runs 270 degrees, from bottom left to bottom
    right through the top; the value arc is drawn over a dark track in
    the section's colour, from the left end - or, BIPOLAR, from the top,
    so a tune of 0 or a triangle Shape shows as no arc at all.

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
	Layout::Accent mAccent;
	int  mDiameter;
	bool mBipolar;
	std::string mLabel;
	std::function<std::string (float)> mFormatter;

	bool mDragging = false;
	VSTGUI::CCoord mLastY = 0.;
};

//------------------------------------------------------------------------
/** A SEGMENTED SWITCH: one segment per named position, the current one
    lit in the section's colour. A click on a segment selects it; the
    wheel steps. Positions are spread evenly over 0..1, the way an Enum
    parameter's normalised values are. */
class SegmentSwitch : public VSTGUI::CControl
{
public:
	SegmentSwitch (const VSTGUI::CRect& size, VSTGUI::IControlListener* listener, int32_t tag,
	               Layout::Accent accent);

	void setNames (const std::vector<std::string>& names) { mNames = names; invalid (); }

	void draw (VSTGUI::CDrawContext* context) override;
	void onMouseDownEvent (VSTGUI::MouseDownEvent& event) override;
	void onMouseUpEvent (VSTGUI::MouseUpEvent& event) override;
	void onMouseWheelEvent (VSTGUI::MouseWheelEvent& event) override;

	CLASS_METHODS (SegmentSwitch, VSTGUI::CControl)

private:
	int count () const { return static_cast<int> (mNames.size ()); }
	int currentIndex () const;
	void select (int index);

	Layout::Accent mAccent;
	std::vector<std::string> mNames;
};

//------------------------------------------------------------------------
/** A SCREEN: a dark rounded plate with a grid, a caption top left, a
    figure top right, and up to two traces.

    The FIRST trace is the main one - drawn 2 px in its colour with a
    translucent fill under it when setFill (true). The SECOND is a
    companion, thinner and dimmer (the LFO's unpatched output). Both are
    0..1, y up, and CLAMPED rather than scaled: the caller fixes the
    scale, as the old envelope display did, so one curve cannot rescale
    the picture.

    Optional extras: a dot on the first trace (the filter's cutoff), a
    dotted vertical line (where the key went up), a legend along the
    foot. Mouse-disabled. */
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

	/** The dot, at (x, y) 0..1 of the plot; negative x for none. */
	void setMarker (double x, double y) { mMarkerX = x; mMarkerY = y; }

	/** A dotted vertical line at `fraction` of the width; negative for
	    none. */
	void setGateMarker (double fraction) { mGate = fraction; }

	void draw (VSTGUI::CDrawContext* context) override;

	CLASS_METHODS (Scope, VSTGUI::CView)

private:
	VSTGUI::CRect plotRect () const;
	void drawTrace (VSTGUI::CDrawContext* context, const std::vector<float>& data,
	                const VSTGUI::CColor& colour, VSTGUI::CCoord width, bool fill) const;

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
/** The SELF-OSC lamp: a dot that glows when the resonance is at loop
    gain 1 or more (selfOscillating()), and its name beside it. */
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
} // namespace Panel
} // namespace R2Synth
