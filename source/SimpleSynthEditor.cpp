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

/** The panel's background - FilterDrum's. Darker than the controls' bar
    fill so the bars read as raised, which is what the DXi's Draw3dRect
    did. */
const CColor kPanelBack (44, 48, 44, 255);

/** How many LFO cycles the display shows. Two, so the join between one
    cycle and the next - where a sawtooth resets - is in the middle of
    the picture rather than at its edges. */
constexpr double kLfoDisplayCycles = 2.0;

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
CTextLabel* SimpleSynthEditor::addTextLine (int x, int y, int width, const char* text,
                                            const CColor& colour, bool small)
{
	CRect r (x, y, x + width, y + Layout::kLabelHeight);
	auto* label = new CTextLabel (r, text);
	label->setFont (small ? panelFontSmall () : panelFont ());
	label->setFontColor (colour);
	label->setBackColor (kPanelBack);
	label->setFrameColor (kPanelBack);
	label->setHoriAlign (kLeftText);
	label->setMouseEnabled (false);
	frame->addView (label);
	return label;
}

//------------------------------------------------------------------------
void SimpleSynthEditor::addGroupBox (const Layout::Group& group)
{
	// ADDED BEFORE THE CONTROLS IT ENCLOSES, so it is behind them. It is
	// mouse-disabled and never fills, so a click inside it reaches
	// whatever is really there.
	//
	// THE VIEW STARTS AT THE TOP OF THE TITLE and the frame line is drawn
	// kGroupTitleBand down it - see the note on kGroupTitleBand in
	// SimpleSynthLayout.h for why the title used to be cut off.
	const Layout::Rect r = Layout::groupRect (group);
	auto* box = new SpyGroupBox (CRect (r.left, r.top, r.right, r.bottom),
	                             group.title, Colours::kGroupFrame);
	box->setTitleBand (Layout::kGroupTitleBand);
	frame->addView (box);
}

//------------------------------------------------------------------------
bool PLUGIN_API SimpleSynthEditor::open (void* parent, const PlatformType& platformType)
{
	if (frame)
		return false;

	CRect frameSize (0, 0, kEditorWidth, kEditorHeight);
	frame = new CFrame (frameSize, this);
	frame->setBackgroundColor (kPanelBack);
	frame->open (parent, platformType);

	// The title and the engine line share the top row.
	addTextLine (Layout::kContentX, Layout::kTitleY, Layout::kTitleWidth,
	             panelTitleText (), Colours::kValue, false);
	mRateLabel = addTextLine (Layout::kEngineLineX, Layout::kTitleY, Layout::kEngineLineWidth,
	                          "", Colours::kValue, true);

	// The signal path, in the green the section labels use, with the one
	// fact about playing it the panel cannot otherwise show. The lamp is
	// explained here because it is the one thing on the panel that
	// lights on its own.
	addTextLine (Layout::kContentX, Layout::kFlowY, Layout::kFlowWidth,
	             panelFlowText (), Colours::kLabel, false);

	// ---- the boxes, then the controls in them --------------------------
	//
	// BACK TO FRONT, because a SpyGroupBox is a line and whatever is
	// added later draws over it.
	for (const auto& g : Layout::kGroups)
		addGroupBox (g);

	for (const auto& c : Layout::kControls)
		addControl (c);

	// THE LAMP, on the resonance knob - the DXi SlideSpin's own 10 x 10
	// corner lamp, and exactly what it was for.
	if (auto* res = dynamic_cast<SpySlider*> (mControls[kResonance]))
		res->setUseIndicator (true);

	// ---- the two displays ----------------------------------------------
	{
		CRect r (Layout::kDisplayX, Layout::kLfoViewTop,
		         Layout::kDisplayX + Layout::kDisplayWidth, Layout::kLfoViewBottom);
		mLfoView = new SpyEnvelopeView (r);
		mLfoView->setShowAmounts (false);
		frame->addView (mLfoView);
	}
	{
		CRect r (Layout::kDisplayX, Layout::kEnvViewTop,
		         Layout::kDisplayX + Layout::kDisplayWidth, Layout::kEnvViewBottom);
		mEnvView = new SpyEnvelopeView (r);
		mEnvView->setShowAmounts (false);
		frame->addView (mEnvView);
	}

	// ---- the readout lines beside the two LFOs -------------------------
	mLfoLabel       = addTextLine (Layout::kReadoutX, Layout::kReadout1Y, Layout::kReadoutWidth,
	                               "", Colours::kTrace, true);
	mLfoDepthsLabel = addTextLine (Layout::kReadoutX, Layout::kReadout2Y, Layout::kReadoutWidth,
	                               "", Colours::kTrace, true);
	mLfo2Label      = addTextLine (Layout::kReadoutX, Layout::kReadout3Y, Layout::kReadoutWidth,
	                               "", Colours::kValue, true);

	refreshAllReadouts ();

	// ---- the licence ---------------------------------------------------
	//
	// OPENING THE EDITOR is the only thing that starts the evaluation
	// clock or shows the reminder: a host scanning plug-ins, or auval,
	// never opens an editor, so never sees either. Nothing here touches
	// the audio path; an unlicensed SimpleSynth sounds exactly the same.
	{
		const auto decision = mLicence.editorOpened (R2Licence::LicenceStore::today ());
		mLicenceBadge = new LicenceBadge (CRect (Layout::kBadgeX, Layout::kBadgeY,
		                                         Layout::kBadgeX + Layout::kBadgeWidth,
		                                         Layout::kBadgeY + Layout::kBadgeHeight));
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
	mLfoView = nullptr;
	mEnvView = nullptr;
	mLfoLabel = nullptr;
	mLfoDepthsLabel = nullptr;
	mLfo2Label = nullptr;
	mRateLabel = nullptr;

	if (frame)
	{
		frame->forget ();
		frame = nullptr;
	}
}

//------------------------------------------------------------------------
//------------------------------------------------------------------------
void SimpleSynthEditor::addControl (const Layout::Control& c)
{
	const Layout::Rect lr = Layout::controlRect (c);
	const CRect r (lr.left, lr.top, lr.right, lr.bottom);
	const ParamID tag = c.tag;

	if (c.kind == Layout::Kind::Selector)
	{
		auto* sel = new SpySelector (r, this, static_cast<int32_t> (tag));

		// THE NAMES COME FROM enumName(), the same list the controller's
		// toString reads, so the panel and the host's automation lane
		// say the same word.
		//
		// COMPACT: the switch is drawn the height of a slider's bar, with
		// its label underneath like a slider's - see SpySelector::
		// setCompact.
		std::vector<std::string> names;
		for (int i = 0; i <= paramDef (tag).stepCount; ++i)
			names.push_back (selectorText (tag, i));
		sel->setNames (names);
		sel->setCompact (true);
		sel->setLabel (shortLabel (tag));
		registerControl (tag, sel);
		return;
	}

	auto* slider = new SpySlider (r, this, static_cast<int32_t> (tag));
	slider->setLabel (shortLabel (tag));

	// THE PANEL BORROWS readoutFor() rather than formatting in the
	// control, so every number on the panel has one source.
	slider->setFormatter ([this, tag] (float /*normalized*/) { return readoutFor (tag); });

	registerControl (tag, slider);
}

//------------------------------------------------------------------------
void SimpleSynthEditor::registerControl (ParamID tag, CControl* control)
{
	mControls[tag] = control;
	if (mController)
		control->setValueNormalized (static_cast<float> (mController->getParamNormalized (tag)));
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
		case kResonance:
			refreshResonanceLamp ();
			break;

		case kLfoShapeLive:
			// ONLY the display: it arrives up to 60 times a second.
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
			refreshLfoDisplay ();
			refreshInfoLines ();
			break;

		case kLfo2Level:
			refreshLfoDisplay ();    // the caption says whether it is swept
			refreshInfoLines ();
			break;

		case kLfo2Rate:
		case kLfoPitch:
		case kVcfLfo:
		case kVcfLfoRes:
		case kVcaLfo:
			refreshInfoLines ();
			break;

		case kVcfAttack: case kVcfDecay: case kVcfSustain: case kVcfRelease:
		case kVcaAttack: case kVcaDecay: case kVcaSustain: case kVcaRelease:
		case kVcfEnvAmount:
			refreshEnvelopeDisplay ();
			refreshInfoLines ();
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

//------------------------------------------------------------------------
std::string SimpleSynthEditor::lfoLine () const
{
	return lfoLineText ([this] (ParamID id) { return internalOf (id); });
}

std::string SimpleSynthEditor::lfoDepthsLine () const
{
	return lfoDepthsLineText ([this] (ParamID id) { return internalOf (id); });
}

std::string SimpleSynthEditor::lfo2Line () const
{
	return lfo2LineText ([this] (ParamID id) { return internalOf (id); });
}

//------------------------------------------------------------------------
std::string SimpleSynthEditor::engineLine () const
{
	const double rate = mController ? mController->getHostSampleRate () : 44100.0;
	return engineLineText (rate, internalOf (kVcaRelease));
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
	set (mLfoLabel, lfoLine ());
	set (mLfoDepthsLabel, lfoDepthsLine ());
	set (mLfo2Label, lfo2Line ());

	if (mRateLabel)
	{
		mRateLabel->setText (engineLine ().c_str ());
		mRateLabel->invalid ();
	}
}

//------------------------------------------------------------------------
double SimpleSynthEditor::displayShape () const
{
	return mHaveLiveShape ? mLiveShape : internalOf (kLfoShape);
}

//------------------------------------------------------------------------
void SimpleSynthEditor::refreshLfoDisplay ()
{
	if (!mLfoView)
		return;

	// lfoDisplayData() - shared with the panel picture's tool - traces
	// the PATCHED output in green and a companion in red, through the
	// same lfoOutputValue() the running LFO uses.
	float patched[Layout::kDisplayPoints];
	float companion[Layout::kDisplayPoints];
	std::string patchedLegend, companionLegend;
	lfoDisplayData (internalOf (kLfoOutput), displayShape (), patched, companion,
	                Layout::kDisplayPoints, kLfoDisplayCycles, patchedLegend, companionLegend);

	const double rate = internalOf (kLfoRate);

	char annotation[32] = {};
	std::snprintf (annotation, sizeof (annotation), "%.2f s", kLfoDisplayCycles / rate);

	// THE PATCHED OUTPUT IS IN CAPITALS with an arrow, the companion in
	// lower case: both are drawn, because the Shape knob moves both, but
	// only one of them is reaching anything.
	mLfoView->setCaption (lfoDisplayCaption (internalOf (kLfo2Level)));
	mLfoView->setAnnotation (annotation);
	mLfoView->setLegend (patchedLegend, companionLegend);
	mLfoView->setCurves (patched, companion, Layout::kDisplayPoints);
}

//------------------------------------------------------------------------
void SimpleSynthEditor::refreshEnvelopeDisplay ()
{
	if (!mEnvView)
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

	char annotation[32] = {};
	if (span >= 1.0)
		std::snprintf (annotation, sizeof (annotation), "%.2f s", span);
	else
		std::snprintf (annotation, sizeof (annotation), "%.0f ms", span * 1000.0);

	// The VCF legend carries the Amount WITH ITS SIGN: an inverted
	// envelope is the same shape as an upright one, so neither the curve
	// nor anything else on the display could show the difference.
	char vcfLegend[32] = {};
	std::snprintf (vcfLegend, sizeof (vcfLegend), "VCF ENV %+.1foct", internalOf (kVcfEnvAmount));

	mEnvView->setCaption ("ENVELOPES  (key up at the dots)");
	mEnvView->setAnnotation (annotation);
	mEnvView->setLegend (vcfLegend, "VCA ENV");
	mEnvView->setGateMarker ((span > 0.0) ? gateOff / span : -1.0);
	mEnvView->setCurves (vcfCurve, vcaCurve, Layout::kDisplayPoints);
}

//------------------------------------------------------------------------
void SimpleSynthEditor::refreshResonanceLamp ()
{
	auto it = mControls.find (kResonance);
	if (it == mControls.end () || it->second == nullptr)
		return;

	if (auto* slider = dynamic_cast<SpySlider*> (it->second))
	{
		// selfOscillating() ON THE INTERNAL VALUE - loop gain, not per
		// cent - the predicate the DSP's own table is built around.
		slider->setIndicator (selfOscillating (internalOf (kResonance)));
		slider->invalid ();
	}
}

//------------------------------------------------------------------------
void SimpleSynthEditor::refreshAllReadouts ()
{
	for (auto& entry : mControls)
		if (entry.second != nullptr)
			entry.second->invalid ();

	refreshResonanceLamp ();
	refreshLfoDisplay ();
	refreshEnvelopeDisplay ();
	refreshInfoLines ();
}

//------------------------------------------------------------------------
} // namespace SimpleSynth
