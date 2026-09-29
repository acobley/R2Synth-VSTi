//------------------------------------------------------------------------
// SimpleSynth - editor
//
// FilterDrum's panel, re-laid for a synth: the same controls, the same
// colours, the same group boxes and the same envelope display - see
// SimpleSynthControls.h for where they came from and the two small
// additions. Four rows in signal order with two displays in a strip on
// the right; SimpleSynthLayout.h has the picture and the numbers.
//
// TWO RULES THIS PANEL KEEPS, and they are the ones that get broken
// first:
//
//   1. Every number shown comes from the parameter table or from a
//      function in SimpleSynthDsp.h that the AUDIO PATH also calls -
//      never from arithmetic written a second time here. The
//      self-oscillation lamp lights from selfOscillating(); the LFO
//      display is drawn by traceLfo(), which calls the same
//      lfoShapeValue() the LFO runs; the envelope display drives real
//      Adsr objects through traceEnvelopes(); the Shape range LFO 2
//      sweeps is printed from lfo2ShapeRange(), which calls the
//      sweptLfoShape() the DSP runs.
//
//   2. The layout is DATA, in SimpleSynthLayout.h, not positions typed
//      into the middle of open().
//
// NO BITMAPS. Everything is drawn with rectangles and text, so the panel
// is resolution-independent and resource/ needs no artwork.
//
// TOOLTIPS MAY NEVER APPEAR ON macOS - see PORTING-GUIDE.md section 6.
// So what the LFO is doing, and why the top of the Cutoff knob may not
// be reachable, are printed on the panel rather than hidden in a hover.
//------------------------------------------------------------------------

#pragma once

#include "SimpleSynthControls.h"
#include "SimpleSynthLayout.h"
#include "SimpleSynthParams.h"

#include "public.sdk/source/vst/vstguieditor.h"

#include <map>
#include <string>

namespace SimpleSynth {

class SimpleSynthController;

//------------------------------------------------------------------------
class SimpleSynthEditor : public Steinberg::Vst::VSTGUIEditor, public VSTGUI::IControlListener
{
public:
	explicit SimpleSynthEditor (SimpleSynthController* controller);

	bool PLUGIN_API open (void* parent, const VSTGUI::PlatformType& platformType) SMTG_OVERRIDE;
	void PLUGIN_API close () SMTG_OVERRIDE;

	// IControlListener
	void valueChanged (VSTGUI::CControl* control) SMTG_OVERRIDE;

	/** Called by the controller when a parameter changes anywhere else -
	    automation, the host's generic editor, a second panel. */
	void updateControl (Steinberg::Vst::ParamID tag, Steinberg::Vst::ParamValue normalized);

	/** Rebuild every readout. Called when the sample rate changes,
	    because the cutoff ceiling is a fraction of it. */
	void refreshAllReadouts ();

	/** The text drawn on one control - readoutText() in
	    SimpleSynthReadouts.h, with this panel's current value. */
	std::string readoutFor (Steinberg::Vst::ParamID tag) const;

	/** The lines beside the two LFO groups, and the engine line on the
	    title row, likewise. */
	std::string lfoLine () const;
	std::string lfoDepthsLine () const;
	std::string lfo2Line () const;
	std::string engineLine () const;

	static const int kEditorWidth  = Layout::kEditorWidth;
	static const int kEditorHeight = Layout::kEditorHeight;

private:
	void addGroupBox (const Layout::Group& group);
	void addControl (const Layout::Control& control);
	VSTGUI::CTextLabel* addTextLine (int x, int y, int width, const char* text,
	                                 const VSTGUI::CColor& colour, bool small);
	void registerControl (Steinberg::Vst::ParamID tag, VSTGUI::CControl* control);

	/** Everything that depends on a parameter besides the control that
	    shows it: the lamp, the two displays, the readout lines. Named
	    per parameter so a drag does not redraw the whole panel. */
	void refreshDependents (Steinberg::Vst::ParamID tag);

	/** The current normalised value of a parameter. Falls back to the
	    table's default when there is no controller. */
	double normalizedOf (Steinberg::Vst::ParamID tag) const;
	double internalOf (Steinberg::Vst::ParamID tag) const;

	void refreshResonanceLamp ();
	void refreshLfoDisplay ();
	void refreshEnvelopeDisplay ();
	void refreshInfoLines ();

	SimpleSynthController* mController = nullptr;

	std::map<Steinberg::Vst::ParamID, VSTGUI::CControl*> mControls;

	/** Raw pointers to views the FRAME owns, like everything in
	    mControls; close() forgets them at the same time. */
	SpyEnvelopeView*    mLfoView    = nullptr;
	SpyEnvelopeView*    mEnvView    = nullptr;
	VSTGUI::CTextLabel* mLfoLabel       = nullptr;
	VSTGUI::CTextLabel* mLfoDepthsLabel = nullptr;
	VSTGUI::CTextLabel* mLfo2Label      = nullptr;
	VSTGUI::CTextLabel* mRateLabel      = nullptr;

	/** The Shape LFO 1 is really running at, from kLfoShapeLive, and
	    whether one has arrived since the Shape knob last moved. Without
	    one - audio not running, or a host that drops output parameters -
	    the display falls back to the knob. */
	double mLiveShape     = 0.0;
	bool   mHaveLiveShape = false;

	/** The Shape the LFO display draws: the live one when there is one. */
	double displayShape () const;

	/** Set while the controller is pushing a value INTO a control, so
	    valueChanged does not send it straight back out as an edit - a
	    host automating a parameter would otherwise get a feedback loop. */
	bool mUpdating = false;
};

//------------------------------------------------------------------------
} // namespace SimpleSynth
