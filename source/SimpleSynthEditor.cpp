//------------------------------------------------------------------------
// SimpleSynth - editor implementation
//------------------------------------------------------------------------

#include "SimpleSynthEditor.h"
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

/** The LFO display draws -1..+1 into this much of its height, centred,
    so a square at full swing does not sit on the plate's frame. */
constexpr double kLfoDisplaySwing = 0.42;

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
	const Layout::Rect r = Layout::groupRect (group);
	frame->addView (new SpyGroupBox (CRect (r.left, r.top, r.right, r.bottom),
	                                 group.title, Colours::kGroupFrame));
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

	const int fullWidth = kEditorWidth - Layout::kMargin - Layout::kContentX;

	addTextLine (Layout::kContentX, Layout::kTitleY, fullWidth,
	             "SimpleSynth   -   three VCOs, an MS-20 LFO, a Moog ladder and two ADSRs",
	             Colours::kValue, false);

	// The signal path, in the green the section labels use. The lamp is
	// explained here because it is the one thing on the panel that
	// lights on its own.
	addTextLine (Layout::kContentX, Layout::kFlowY, fullWidth,
	             "VCO 1-3 -> mixer -> Moog ladder VCF -> VCA -> output      "
	             "(mono, last note wins; lamp = self-oscillating)",
	             Colours::kLabel, false);

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

	// ---- the readout lines beside the LFO ------------------------------
	mLfoLabel  = addTextLine (Layout::kReadoutX, Layout::kReadout1Y, Layout::kReadoutWidth,
	                          "", Colours::kTrace, true);
	mRateLabel = addTextLine (Layout::kReadoutX, Layout::kReadout2Y, Layout::kReadoutWidth,
	                          "", Colours::kValue, true);

	// The one fact about playing it that the panel cannot otherwise show.
	addTextLine (Layout::kReadoutX, Layout::kReadout3Y, Layout::kReadoutWidth,
	             "Every new note retriggers both envelopes; letting go returns to the last key held.",
	             Colours::kLabel, true);

	refreshAllReadouts ();

	return true;
}

//------------------------------------------------------------------------
void PLUGIN_API SimpleSynthEditor::close ()
{
	// The map holds RAW pointers to views the FRAME owns. Clearing it
	// before forgetting the frame, and never dereferencing it after, is
	// what keeps that safe - see SimpleSynthController::editorDestroyed
	// for the other half.
	mControls.clear ();
	mLfoView = nullptr;
	mEnvView = nullptr;
	mLfoLabel = nullptr;
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
		// say the same word. selectorText() puts the label in front,
		// because SpySelector draws no label of its own.
		std::vector<std::string> names;
		for (int i = 0; i <= paramDef (tag).stepCount; ++i)
			names.push_back (selectorText (tag, i));
		sel->setNames (names);
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

		case kLfoRate:
		case kLfoShape:
		case kLfoOutput:
			refreshLfoDisplay ();
			refreshInfoLines ();
			break;

		case kLfoPitch:
		case kVcfLfo:
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

//------------------------------------------------------------------------
std::string SimpleSynthEditor::engineLine () const
{
	const double rate = mController ? mController->getHostSampleRate () : 44100.0;
	return engineLineText (rate, internalOf (kVcaRelease));
}

//------------------------------------------------------------------------
void SimpleSynthEditor::refreshInfoLines ()
{
	if (mLfoLabel)
	{
		mLfoLabel->setText (lfoLine ().c_str ());
		mLfoLabel->invalid ();
	}
	if (mRateLabel)
	{
		mRateLabel->setText (engineLine ().c_str ());
		mRateLabel->invalid ();
	}
}

//------------------------------------------------------------------------
void SimpleSynthEditor::refreshLfoDisplay ()
{
	if (!mLfoView)
		return;

	// DRAWN BY traceLfo(), which calls the same lfoShapeValue() and
	// lfoSquareValue() the running LFO does - so the picture is the
	// wave, not a drawing of one.
	float shapeCurve[Layout::kDisplayPoints];
	float squareCurve[Layout::kDisplayPoints];
	traceLfo (internalOf (kLfoShape), shapeCurve, squareCurve, Layout::kDisplayPoints,
	          kLfoDisplayCycles);

	// -1..+1 into the middle of the plot; the view's scale is 0..1.
	for (int i = 0; i < Layout::kDisplayPoints; ++i)
	{
		shapeCurve[i]  = static_cast<float> (0.5 + kLfoDisplaySwing * shapeCurve[i]);
		squareCurve[i] = static_cast<float> (0.5 + kLfoDisplaySwing * squareCurve[i]);
	}

	const double rate = internalOf (kLfoRate);
	const bool square = internalOf (kLfoOutput) >= 0.5;

	char annotation[32] = {};
	std::snprintf (annotation, sizeof (annotation), "%.2f s", kLfoDisplayCycles / rate);

	// THE PATCHED OUTPUT IS IN CAPITALS with an arrow, the other in
	// lower case: both are drawn, because the Shape knob moves both, but
	// only one of them is reaching anything.
	mLfoView->setCaption ("LFO  (2 cycles)");
	mLfoView->setAnnotation (annotation);
	mLfoView->setLegend (square ? "shape" : "> SHAPE", square ? "SQUARE <" : "square");
	mLfoView->setCurves (shapeCurve, squareCurve, Layout::kDisplayPoints);
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
