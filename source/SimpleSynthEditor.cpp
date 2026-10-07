//------------------------------------------------------------------------
// SimpleSynth - editor implementation
//------------------------------------------------------------------------

#include "SimpleSynthEditor.h"
#include "LicenceViews.h"
#include "SimpleSynthController.h"
#include "SimpleSynthDsp.h"
#include "SimpleSynthReadouts.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <vector>

using namespace VSTGUI;
using namespace Steinberg;
using namespace Steinberg::Vst;

namespace SimpleSynth {

//------------------------------------------------------------------------
namespace {

/** How many LFO cycles the screen shows. Two, so the join between one
    cycle and the next - where a sawtooth resets - is in the middle of
    the picture rather than at its edges. */
constexpr double kLfoDisplayCycles = 2.0;

CRect toCRect (const Layout::Rect& r)
{
	return CRect (r.left, r.top, r.right, r.bottom);
}


} // anonymous namespace

//------------------------------------------------------------------------
SimpleSynthEditor::SimpleSynthEditor (SimpleSynthController* controller)
: VSTGUIEditor (controller)
, mController (controller)
{
	ViewRect rect (0, 0, kEditorWidth, kEditorHeight);
	setRect (rect);
}

//------------------------------------------------------------------------
CTextLabel* SimpleSynthEditor::addTextLine (const Layout::Rect& r, CFontRef font,
                                            const CColor& colour, CHoriTxtAlign align)
{
	auto* label = new CTextLabel (toCRect (r), "");
	label->setFont (font);
	label->setFontColor (colour);
	label->setTransparency (true);
	label->setStyle (CParamDisplay::kNoFrame);
	label->setHoriAlign (align);
	label->setMouseEnabled (false);
	frame->addView (label);
	return label;
}

//------------------------------------------------------------------------
bool PLUGIN_API SimpleSynthEditor::open (void* parent, const PlatformType& platformType)
{
	if (frame)
		return false;

	CRect frameSize (0, 0, kEditorWidth, kEditorHeight);
	frame = new CFrame (frameSize, this);
	frame->setBackgroundColor (Panel::Palette::kGround);
	frame->open (parent, platformType);

	// ---- everything that never changes, behind everything else ----------
	frame->addView (new Panel::Backdrop (frameSize));

	// ---- the screens -------------------------------------------------------
	auto addScope = [this] (const Layout::Rect& r, Layout::Accent main, const CColor& companion, bool fill) {
		auto* scope = new Panel::Scope (toCRect (r));
		scope->setColours (Panel::Palette::accent (main), companion);
		scope->setFill (fill);
		frame->addView (scope);
		return scope;
	};
	mFilterView    = addScope (Layout::kFilterView, Layout::Accent::Filter, Panel::Palette::kDim, true);
	mFilterEnvView = addScope (Layout::kFilterEnvView, Layout::Accent::Filter, Panel::Palette::kMuted, true);
	mAmpEnvView    = addScope (Layout::kAmpEnvView, Layout::Accent::Amp, Panel::Palette::kMuted, true);
	mLfoView       = addScope (Layout::kLfoView, Layout::Accent::Mod, Panel::Palette::kDim, false);

	// ---- the lamp, and the two lines of text that change -------------------
	mSelfOscLamp = new Panel::Lamp (toCRect (Layout::kSelfOscLamp), "Self-osc");
	frame->addView (mSelfOscLamp);

	mEngineLabel  = addTextLine (Layout::kEngineLine, Panel::fontSubtitle (), Panel::Palette::kSubtle, kLeftText);
	mModInfoLabel = addTextLine (Layout::kModInfoLine, Panel::fontValue (), Panel::Palette::kSubtle, kLeftText);

	// ---- the controls -----------------------------------------------------
	for (const auto& c : Layout::kControls)
		addControl (c);

	refreshAllReadouts ();

	// ---- the licence ---------------------------------------------------
	//
	// OPENING THE EDITOR is the only thing that starts the evaluation
	// clock or shows the reminder: a host scanning plug-ins, or auval,
	// never opens an editor, so never sees either. Nothing here touches
	// the audio path; an unlicensed SimpleSynth sounds exactly the same.
	{
		const auto decision = mLicence.editorOpened (R2Licence::LicenceStore::today ());
		mLicenceBadge = new LicenceBadge (toCRect (Layout::kBadge));
		mLicenceBadge->setPill (true);
		mLicenceBadge->setDecision (decision);
		mLicenceBadge->setHandler ([this] { openLicenceDialog (); });
		frame->addView (mLicenceBadge);

		// At most once per session, however many instances are opened.
		if (decision.reminderDue && claimSessionReminder ())
			openLicenceDialog ();
	}

	return true;
}

//------------------------------------------------------------------------
void SimpleSynthEditor::openLicenceDialog ()
{
	if (!frame || mHaveLicenceSession)
		return;

	const auto decision = mLicence.current (R2Licence::LicenceStore::today ());
	auto* dialog = new LicenceDialog (CRect (0, 0, kEditorWidth, kEditorHeight), mLicence,
	                                  [this] { requestCloseLicenceDialog (); });
	dialog->showPage (decision.licensed ? LicenceDialog::Page::Licensed : LicenceDialog::Page::Reminder);

	// The frame takes the dialog: endModalViewSession removes and frees it.
	if (auto session = frame->beginModalViewSession (dialog))
	{
		mLicenceSession = *session;
		mHaveLicenceSession = true;
	}
	else
		dialog->forget ();
}

void SimpleSynthEditor::requestCloseLicenceDialog ()
{
	// Called from a button INSIDE the dialog: removing the dialog now
	// would free the button while its mouse-up is still running. So the
	// frame does it once the event is over.
	if (!frame || !frame->doAfterEventProcessing ([this] { closeLicenceDialog (); }))
		closeLicenceDialog ();
}

void SimpleSynthEditor::closeLicenceDialog ()
{
	if (frame && mHaveLicenceSession)
		frame->endModalViewSession (mLicenceSession);
	mHaveLicenceSession = false;

	if (mLicenceBadge)
		mLicenceBadge->setDecision (mLicence.current (R2Licence::LicenceStore::today ()));
}

//------------------------------------------------------------------------
void PLUGIN_API SimpleSynthEditor::close ()
{
	// The map holds RAW pointers to views the FRAME owns. Clearing it
	// before forgetting the frame, and never dereferencing it after, is
	// what keeps that safe - see SimpleSynthController::editorDestroyed
	// for the other half.
	if (frame && mHaveLicenceSession)
		frame->endModalViewSession (mLicenceSession);
	mHaveLicenceSession = false;
	mLicenceBadge = nullptr;

	mControls.clear ();
	mFilterView = nullptr;
	mFilterEnvView = nullptr;
	mAmpEnvView = nullptr;
	mLfoView = nullptr;
	mSelfOscLamp = nullptr;
	mEngineLabel = nullptr;
	mModInfoLabel = nullptr;

	if (frame)
	{
		frame->forget ();
		frame = nullptr;
	}
}

//------------------------------------------------------------------------
void SimpleSynthEditor::addControl (const Layout::Control& c)
{
	const CRect r = toCRect (Layout::controlRect (c));
	const ParamID tag = c.tag;

	if (c.kind == Layout::Kind::Switch)
	{
		auto* sw = new Panel::SegmentSwitch (r, this, static_cast<int32_t> (tag), c.accent);

		// THE NAMES COME FROM enumName(), the same list the controller's
		// toString reads, so the panel and the host's automation lane
		// say the same word.
		std::vector<std::string> names;
		for (int i = 0; i <= paramDef (tag).stepCount; ++i)
			names.push_back (enumName (tag, i));
		sw->setNames (names);
		registerControl (tag, sw);
		return;
	}

	auto* knob = new Panel::Knob (r, this, static_cast<int32_t> (tag), c.accent, c.diameter, c.bipolar);
	knob->setLabel (controlLabel (c));

	// THE PANEL BORROWS readoutFor() rather than formatting in the
	// control, so every number on the panel has one source.
	knob->setFormatter ([this, tag] (float /*normalized*/) { return readoutFor (tag); });
	registerControl (tag, knob);
}

//------------------------------------------------------------------------
void SimpleSynthEditor::registerControl (ParamID tag, CControl* control)
{
	mControls[tag] = control;

	// Double-click on a knob returns here: the table's default, the
	// same value a fresh instance opens with - EXCEPT THE THREE TUNES,
	// which return to 0 st, in tune. VCO 2's table default is -12 st
	// (the default patch has it an octave down), and a double-click that
	// sent one Tune to 0 and another to -12 read as a bug (Andy, 6 Oct).
	// A fresh instance still opens with VCO 2 an octave down.
	const bool isTune = (tag == kVco1Tune || tag == kVco2Tune || tag == kVco3Tune);
	control->setDefaultValue (static_cast<float> (isTune ? paramDef (tag).toNormalized (0.0)
	                                                     : paramDef (tag).defaultNormalized ()));

	if (mController)
		control->setValueNormalized (static_cast<float> (mController->getParamNormalized (tag)));
	else
		control->setValueNormalized (static_cast<float> (paramDef (tag).defaultNormalized ()));
	frame->addView (control);
}

//------------------------------------------------------------------------
double SimpleSynthEditor::normalizedOf (ParamID tag) const
{
	auto it = mControls.find (tag);
	if (it != mControls.end () && it->second != nullptr)
		return it->second->getValueNormalized ();

	if (mController)
		return mController->getParamNormalized (tag);

	return paramDef (tag).defaultNormalized ();
}

double SimpleSynthEditor::internalOf (ParamID tag) const
{
	return paramDef (tag).toInternal (normalizedOf (tag));
}

double SimpleSynthEditor::sampleRate () const
{
	return mController ? mController->getHostSampleRate () : 44100.0;
}

//------------------------------------------------------------------------
void SimpleSynthEditor::valueChanged (CControl* control)
{
	if (!control || !mController || mUpdating)
		return;

	const ParamID    tag   = static_cast<ParamID> (control->getTag ());
	const ParamValue value = control->getValueNormalized ();

	// BOTH CALLS, in this order. setParamNormalized updates the
	// controller's own copy so the panel and the host agree immediately;
	// performEdit is what tells the HOST, and is what makes the move
	// automatable and undoable.
	mController->setParamNormalized (tag, value);
	mController->performEdit (tag, value);

	control->invalid ();
	refreshDependents (tag);
}

//------------------------------------------------------------------------
void SimpleSynthEditor::updateControl (ParamID tag, ParamValue normalized)
{
	auto it = mControls.find (tag);
	if (it != mControls.end () && it->second != nullptr)
	{
		mUpdating = true;
		it->second->setValueNormalized (static_cast<float> (normalized));
		it->second->invalid ();
		mUpdating = false;
	}

	refreshDependents (tag);
}

//------------------------------------------------------------------------
void SimpleSynthEditor::refreshDependents (ParamID tag)
{
	switch (tag)
	{
		case kCutoff:
			// The knob moved: draw the knob until the processor's next
			// live pair, so a turn shows at once even with no audio.
			mHaveLiveFilter = false;
			refreshFilterDisplay ();
			break;

		case kResonance:
			mHaveLiveFilter = false;
			mHaveLiveResonance = false;
			refreshResonanceLamp ();
			refreshFilterDisplay ();
			break;

		case kVcfType:
			// The other filter's curve and slope - the live pair keeps
			// coming, so the screen follows it on the new filter at once.
			refreshFilterDisplay ();
			break;

		case kCutoffLive:
			// ONLY the screen: these arrive up to 60 times a second.
			mLiveCutoffHz = paramDef (kCutoff).toInternal (normalizedOf (kCutoffLive));
			mHaveLiveFilter = true;
			refreshFilterDisplay ();
			break;

		case kResonanceLive:
			mLiveResonance = paramDef (kResonance).toInternal (normalizedOf (kResonanceLive));
			mHaveLiveFilter = true;
			mHaveLiveResonance = true;
			refreshResonanceLamp ();    // the lamp follows LFO 1 too
			refreshFilterDisplay ();
			break;

		case kLfoShapeLive:
			// ONLY the screen: it arrives up to 60 times a second.
			mLiveShape = paramDef (kLfoShape).toInternal (normalizedOf (kLfoShapeLive));
			mHaveLiveShape = true;
			refreshLfoDisplay ();
			break;

		case kLfoShape:
			// The knob moved: draw the knob until the processor's next
			// live value, so a turn shows at once even with no audio.
			mHaveLiveShape = false;
			refreshLfoDisplay ();
			refreshInfoLines ();
			break;

		case kLfoRate:
		case kLfoOutput:
		case kLfo2Level:
			refreshLfoDisplay ();
			refreshInfoLines ();
			break;

		case kLfo2Rate:
			refreshInfoLines ();
			break;

		case kVcfAttack: case kVcfDecay: case kVcfSustain: case kVcfRelease:
		case kVcaAttack: case kVcaDecay: case kVcaSustain:
		case kVcfEnvAmount:
			refreshEnvelopeDisplays ();
			break;

		case kVcaRelease:
			refreshEnvelopeDisplays ();
			refreshInfoLines ();    // the release tail is on the engine line
			break;

		default:
			break;
	}
}

//------------------------------------------------------------------------
std::string SimpleSynthEditor::readoutFor (ParamID tag) const
{
	// ONE COPY OF THE FORMATTING, in SimpleSynthReadouts.cpp, which the
	// panel picture's tool calls too.
	return readoutText (tag, normalizedOf (tag));
}

std::string SimpleSynthEditor::modInfoLine () const
{
	return modInfoLineText ([this] (ParamID id) { return internalOf (id); });
}

std::string SimpleSynthEditor::engineLine () const
{
	return engineLineText (sampleRate (), internalOf (kVcaRelease));
}

//------------------------------------------------------------------------
void SimpleSynthEditor::refreshInfoLines ()
{
	auto set = [] (CTextLabel* label, const std::string& text) {
		if (label)
		{
			label->setText (text.c_str ());
			label->invalid ();
		}
	};
	set (mModInfoLabel, modInfoLine ());
	set (mEngineLabel, engineLine ());
}

//------------------------------------------------------------------------
double SimpleSynthEditor::displayShape () const
{
	return mHaveLiveShape ? mLiveShape : internalOf (kLfoShape);
}

//------------------------------------------------------------------------
void SimpleSynthEditor::refreshFilterDisplay ()
{
	if (!mFilterView)
		return;

	// THE KNOBS' CURVE always; THE LIVE CURVE over it when the processor
	// has sent one - the filter as the envelope, key tracking and LFO 1
	// have moved it. Live, the knobs' curve stays as a dim companion, so
	// the modulation shows as the distance between the two.
	float knobs[Layout::kDisplayPoints];
	double knobX = 0.0, knobY = 0.0;
	const FilterType type = filterTypeOf (internalOf (kVcfType));
	filterDisplayData (internalOf (kCutoff), internalOf (kResonance), sampleRate (),
	                   knobs, Layout::kDisplayPoints, knobX, knobY, type);

	mFilterView->setLegend ("20 Hz", "20 kHz");
	if (mHaveLiveFilter)
	{
		float live[Layout::kDisplayPoints];
		double liveX = 0.0, liveY = 0.0;
		filterDisplayData (mLiveCutoffHz, mLiveResonance, sampleRate (),
		                   live, Layout::kDisplayPoints, liveX, liveY, type);

		mFilterView->setCaption (filterDisplayCaption (true, type));
		mFilterView->setAnnotation (filterDisplayAnnotation (normalizedOf (kCutoffLive),
		                                                     normalizedOf (kResonanceLive)));
		mFilterView->setMarker (liveX, liveY);
		mFilterView->setCurves (live, knobs, Layout::kDisplayPoints);
		return;
	}

	mFilterView->setCaption (filterDisplayCaption (false, type));
	mFilterView->setAnnotation (filterDisplayAnnotation (normalizedOf (kCutoff), normalizedOf (kResonance)));
	mFilterView->setMarker (knobX, knobY);
	mFilterView->setCurves (knobs, nullptr, Layout::kDisplayPoints);
}

//------------------------------------------------------------------------
void SimpleSynthEditor::refreshLfoDisplay ()
{
	if (!mLfoView)
		return;

	// lfoDisplayData() - shared with the panel picture's tool - traces
	// the PATCHED output and a companion, through the same
	// lfoOutputValue() the running LFO uses.
	float patched[Layout::kDisplayPoints];
	float companion[Layout::kDisplayPoints];
	std::string patchedLegend, companionLegend;
	lfoDisplayData (internalOf (kLfoOutput), displayShape (), patched, companion,
	                Layout::kDisplayPoints, kLfoDisplayCycles, patchedLegend, companionLegend);

	// The screen is small: no caption - the period is on the info line
	// under it - and the two legends along the foot, the patched one in
	// capitals.
	mLfoView->setLegend (patchedLegend, companionLegend);
	mLfoView->setCurves (patched, companion, Layout::kDisplayPoints);
}

//------------------------------------------------------------------------
void SimpleSynthEditor::refreshEnvelopeDisplays ()
{
	if (!mFilterEnvView || !mAmpEnvView)
		return;

	// Built from PARAMETERS through toInternal() - the same conversion
	// the processor applies - and drawn by real Adsr objects in
	// traceEnvelopes(). The editor cannot see the DSP's envelopes: they
	// live in the processor, which in some hosts is another process.
	AdsrSpec vcf, vca;
	vcf.attack  = internalOf (kVcfAttack);
	vcf.decay   = internalOf (kVcfDecay);
	vcf.sustain = internalOf (kVcfSustain);
	vcf.release = internalOf (kVcfRelease);
	vca.attack  = internalOf (kVcaAttack);
	vca.decay   = internalOf (kVcaDecay);
	vca.sustain = internalOf (kVcaSustain);
	vca.release = internalOf (kVcaRelease);

	float vcfCurve[Layout::kDisplayPoints];
	float vcaCurve[Layout::kDisplayPoints];
	double gateOff = 0.0;
	const double span = traceEnvelopes (vcf, vca, vcfCurve, vcaCurve, Layout::kDisplayPoints, &gateOff);
	const double gate = (span > 0.0) ? gateOff / span : -1.0;

	// THE TWO SCREENS SHARE ONE TIME AXIS - traceEnvelopes() runs both
	// through one note - so a filter release that outlasts the amp's
	// still shows as the longer of the two, as it did on the old single
	// display. The amp screen prints the span; the filter screen the
	// amount, WITH ITS SIGN, because an inverted envelope is the same
	// shape as an upright one and only the text can say so.
	mFilterEnvView->setAnnotation (envAmountText (internalOf (kVcfEnvAmount)));
	mFilterEnvView->setGateMarker (gate);
	mFilterEnvView->setCurves (vcfCurve, nullptr, Layout::kDisplayPoints);

	mAmpEnvView->setCaption ("key up at the dotted line");
	mAmpEnvView->setAnnotation (spanText (span));
	mAmpEnvView->setGateMarker (gate);
	mAmpEnvView->setCurves (vcaCurve, nullptr, Layout::kDisplayPoints);
}

//------------------------------------------------------------------------
void SimpleSynthEditor::refreshResonanceLamp ()
{
	// selfOscillating() ON THE INTERNAL VALUE - loop gain, not per cent -
	// the predicate the DSP's own table is built around.
	//
	// THE LIVE RESONANCE when there is one (6 Oct): the knob moved by
	// LFO 1, as the processor publishes it, so the lamp flashes when the
	// LFO swings the filter into self-oscillation. The knob alone until
	// the first live value, and again for a moment after the knob moves,
	// so a turn shows at once with no audio running - the filter
	// screen's rule. The live value arrives at up to 60 a second, so an
	// LFO faster than that can cross 1.0 between two of them.
	if (mSelfOscLamp)
	{
		const double k = mHaveLiveResonance ? mLiveResonance : internalOf (kResonance);
		mSelfOscLamp->setOn (selfOscillating (k));
	}
}

//------------------------------------------------------------------------
void SimpleSynthEditor::refreshAllReadouts ()
{
	for (auto& entry : mControls)
		if (entry.second != nullptr)
			entry.second->invalid ();

	refreshResonanceLamp ();
	refreshFilterDisplay ();
	refreshLfoDisplay ();
	refreshEnvelopeDisplays ();
	refreshInfoLines ();
}

//------------------------------------------------------------------------
} // namespace SimpleSynth
