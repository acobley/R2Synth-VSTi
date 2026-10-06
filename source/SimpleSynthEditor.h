//------------------------------------------------------------------------
// SimpleSynth - editor
//
// THE "SIGNAL FLOW" PANEL (branch UpdatedUI): oscillators, filter and amp
// left to right in the order the sound goes through them, the modulation
// underneath, a header across the top. SimpleSynthLayout.h has the
// picture and the numbers; SimpleSynthPanelViews.h has the knobs,
// switches and screens. It replaced the FilterDrum grid of bar sliders -
// the parameters, the controller and the processor are unchanged.
//
// TWO RULES THIS PANEL KEEPS, and they are the ones that get broken
// first:
//
//   1. Every number shown comes from the parameter table or from a
//      function in SimpleSynthDsp.h that the AUDIO PATH also calls -
//      never from arithmetic written a second time here. The
//      self-oscillation lamp lights from selfOscillating(); the filter
//      curve is traceFilterResponse(), the ladder's own transfer function
//      set up as process() sets it up; the LFO screen is drawn by
//      traceLfo() through lfoDisplayData(); the envelope screens drive
//      real Adsr objects through traceEnvelopes().
//
//   2. The layout is DATA, in SimpleSynthLayout.h, not positions typed
//      into the middle of open().
//
// NO BITMAPS. Everything is drawn with paths and text, so the panel is
// resolution-independent and resource/ needs no artwork.
//
// TOOLTIPS MAY NEVER APPEAR ON macOS - see PORTING-GUIDE.md section 6.
// So what the LFOs are doing is printed along the foot of the
// modulation strip rather than hidden in a hover.
//------------------------------------------------------------------------

#pragma once

#include "LicenceStore.h"
#include "SimpleSynthLayout.h"
#include "SimpleSynthPanelViews.h"
#include "SimpleSynthParams.h"

#include "public.sdk/source/vst/vstguieditor.h"

#include <map>
#include <string>

namespace SimpleSynth {

class SimpleSynthController;
class LicenceBadge;

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

	/** The line along the foot of the modulation strip, and the engine
	    line in the header. */
	std::string modInfoLine () const;
	std::string engineLine () const;

	static const int kEditorWidth  = Layout::kEditorWidth;
	static const int kEditorHeight = Layout::kEditorHeight;

private:
	void addControl (const Layout::Control& control);
	VSTGUI::CTextLabel* addTextLine (const Layout::Rect& r, VSTGUI::CFontRef font,
	                                 const VSTGUI::CColor& colour, VSTGUI::CHoriTxtAlign align);
	void registerControl (Steinberg::Vst::ParamID tag, VSTGUI::CControl* control);

	/** Everything that depends on a parameter besides the control that
	    shows it: the lamp, the screens, the lines of text. Named per
	    parameter so a drag does not redraw the whole panel. */
	void refreshDependents (Steinberg::Vst::ParamID tag);

	/** The current normalised value of a parameter. Falls back to the
	    table's default when there is no controller. */
	double normalizedOf (Steinberg::Vst::ParamID tag) const;
	double internalOf (Steinberg::Vst::ParamID tag) const;
	double sampleRate () const;

	void refreshResonanceLamp ();
	void refreshFilterDisplay ();
	void refreshLfoDisplay ();
	void refreshEnvelopeDisplays ();
	void refreshInfoLines ();

	SimpleSynthController* mController = nullptr;

	std::map<Steinberg::Vst::ParamID, VSTGUI::CControl*> mControls;

	/** Raw pointers to views the FRAME owns, like everything in
	    mControls; close() forgets them at the same time. */
	Panel::Scope*       mFilterView    = nullptr;
	Panel::Scope*       mFilterEnvView = nullptr;
	Panel::Scope*       mAmpEnvView    = nullptr;
	Panel::Scope*       mLfoView       = nullptr;
	Panel::Lamp*        mSelfOscLamp   = nullptr;
	VSTGUI::CTextLabel* mEngineLabel   = nullptr;
	VSTGUI::CTextLabel* mModInfoLabel  = nullptr;

	/** The Shape LFO 1 is really running at, from kLfoShapeLive, and
	    whether one has arrived since the Shape knob last moved. Without
	    one - audio not running, or a host that drops output parameters -
	    the display falls back to the knob. */
	double mLiveShape     = 0.0;
	bool   mHaveLiveShape = false;

	/** The Shape the LFO display draws: the live one when there is one. */
	double displayShape () const;

	/** The cutoff (Hz) and resonance (loop gain) the FILTER is really
	    running at, from kCutoffLive / kResonanceLive - the knobs moved by
	    the envelope, key tracking and LFO 1 - and whether a pair has
	    arrived since either knob last moved. Without one the filter
	    screen shows the knobs alone, as the LFO screen falls back to the
	    Shape knob. */
	double mLiveCutoffHz   = 1000.0;
	double mLiveResonance  = 0.0;
	bool   mHaveLiveFilter = false;

	/** The SELF-OSC lamp's own flag: true once a live resonance has
	    arrived since the Resonance knob last moved. Separate from
	    mHaveLiveFilter, which a live CUTOFF alone can set. */
	bool   mHaveLiveResonance = false;

	// ---- the licence (LicenceViews.h, LicenceStore.h) ------------------
	/** Opens the licence dialog as a modal view session: the reminder
	    page, or "Licensed to ..." once licensed. */
	void openLicenceDialog ();
	/** From the dialog's buttons: ends the session AFTER the click. */
	void requestCloseLicenceDialog ();
	void closeLicenceDialog ();

	R2Licence::LicenceStore mLicence;
	LicenceBadge* mLicenceBadge = nullptr;  // owned by the frame
	bool mHaveLicenceSession = false;
	VSTGUI::ModalViewSessionID mLicenceSession = 0;

	/** Set while the controller is pushing a value INTO a control, so
	    valueChanged does not send it straight back out as an edit - a
	    host automating a parameter would otherwise get a feedback loop. */
	bool mUpdating = false;
};

//------------------------------------------------------------------------
} // namespace SimpleSynth
