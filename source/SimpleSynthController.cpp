//------------------------------------------------------------------------
// SimpleSynth - edit controller implementation
//------------------------------------------------------------------------

#include "SimpleSynthController.h"
#include "SimpleSynthEditor.h"
#include "SimpleSynthIDs.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/base/ustring.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace SimpleSynth {

//------------------------------------------------------------------------
namespace {

/** A RangeParameter that formats its own text, so the host's generic
    editor and automation lanes show the same numbers the panel does. */
class SimpleSynthParameter : public RangeParameter
{
public:
	SimpleSynthParameter (const ParamDef& def)
	// TITLE AND UNITS MUST NOT BE NULL. RangeParameter passes them
	// straight to UString::assign, which dereferences both without a
	// null check. The symptom is not a crash in the plug-in: it is the
	// SDK VALIDATOR segfaulting during EditController::initialize, in
	// the post-build step, which sends you looking at the build system.
	// Empty string, never null.
	: RangeParameter (USTRING (def.title), def.id, USTRING (def.units), def.plainMin, def.plainMax,
	                  def.plainDefault, def.stepCount, ParameterInfo::kCanAutomate,
	                  kRootUnitId, USTRING (def.title))
	, mDef (def)
	{
		// RangeParameter's own toPlain is LINEAR. A Log row would then
		// have the host's slider and the DSP disagree about what every
		// position means, so the table's mapping is used instead - see
		// toPlain / toNormalized below.
		//
		// AND THE DEFAULT HAS TO BE SET AGAIN HERE. RangeParameter's
		// constructor computes it with toNormalized(), but a virtual call
		// from a base-class constructor reaches the BASE's version - the
		// linear one - so every Log row got a linear default: LFO Rate's
		// 4 Hz came out at 0.196 instead of 0.696, which a host's "reset
		// to default" would then play as 0.28 Hz. The SDK validator's
		// parameter scan is what showed it.
		info.defaultNormalizedValue = def.defaultNormalized ();
		setNormalized (info.defaultNormalizedValue);
	}

	ParamValue toPlain (ParamValue normalized) const SMTG_OVERRIDE
	{
		return mDef.toPlain (normalized);
	}

	ParamValue toNormalized (ParamValue plain) const SMTG_OVERRIDE
	{
		return mDef.toNormalized (plain);
	}

	void toString (ParamValue normalized, String128 string) const SMTG_OVERRIDE
	{
		char text[64] = {};
		const double plain = mDef.toPlain (normalized);

		if (mDef.type == ParamType::Enum)
		{
			// THE SAME NAMES THE PANEL DRAWS, from enumName().
			std::snprintf (text, sizeof (text), "%s",
			               enumName (mDef.id, static_cast<int> (mDef.toInternal (normalized))));
		}
		else if (mDef.type == ParamType::Bool)
		{
			std::snprintf (text, sizeof (text), "%s", (normalized >= 0.5) ? "On" : "Off");
		}
		else if (mDef.id == kCutoff)
		{
			// Hz below 1 k, kHz above - the panel's rule.
			if (plain >= 1000.0)
				std::snprintf (text, sizeof (text), "%.2f k", plain / 1000.0);
			else
				std::snprintf (text, sizeof (text), "%.0f", plain);
		}
		else if (mDef.id == kLfoRate)
		{
			std::snprintf (text, sizeof (text), (plain < 10.0) ? "%.2f" : "%.1f", plain);
		}
		else if (mDef.type == ParamType::Log && std::strcmp (mDef.units, "ms") == 0)
		{
			// Four decades: the decimals follow the magnitude.
			if (plain < 10.0)
				std::snprintf (text, sizeof (text), "%.2f", plain);
			else if (plain < 100.0)
				std::snprintf (text, sizeof (text), "%.1f", plain);
			else
				std::snprintf (text, sizeof (text), "%.0f", plain);
		}
		else if (std::strcmp (mDef.units, "st") == 0 || std::strcmp (mDef.units, "oct") == 0)
		{
			// Signed where the range is: a tune or an envelope amount is
			// read from the centre.
			std::snprintf (text, sizeof (text), (mDef.plainMin < 0.0) ? "%+.2f" : "%.2f", plain);
		}
		else if (mDef.id == kOutputLevel || mDef.id == kLfoShape)
		{
			std::snprintf (text, sizeof (text), "%+.1f", plain);
		}
		else
		{
			// The per-cent parameters. Whole numbers.
			std::snprintf (text, sizeof (text), "%.0f", plain);
		}

		UString (string, str16BufferSize (String128)).assign (USTRING (text));
	}

	/** The inverse of toString, for a host that lets the user type a
	    value. Numbers in the parameter's own units; the two Enum rows
	    also accept their names. */
	bool fromString (const TChar* string, ParamValue& normalized) const SMTG_OVERRIDE
	{
		if (string == nullptr)
			return false;

		// UTF-16 to ASCII by hand: every character a value can be typed
		// with is ASCII, and anything else simply fails the parse.
		char text[64] = {};
		for (int i = 0; i < 63 && string[i] != 0; ++i)
			text[i] = (string[i] < 128) ? static_cast<char> (string[i]) : '?';

		if (mDef.type == ParamType::Enum)
		{
			for (int i = 0; i <= mDef.stepCount; ++i)
			{
				if (std::strcmp (text, enumName (mDef.id, i)) == 0)
				{
					normalized = (mDef.stepCount > 0) ? static_cast<double> (i) / mDef.stepCount : 0.0;
					return true;
				}
			}
		}

		double plain = 0.0;
		if (std::sscanf (text, "%lf", &plain) != 1)
			return false;

		// "2.5 k" typed into the cutoff means 2500.
		if (mDef.id == kCutoff && std::strchr (text, 'k') != nullptr)
			plain *= 1000.0;

		normalized = std::clamp (mDef.toNormalized (plain), 0.0, 1.0);
		return true;
	}

private:
	ParamDef mDef;
};

} // anonymous namespace

//------------------------------------------------------------------------
tresult PLUGIN_API SimpleSynthController::initialize (FUnknown* context)
{
	const tresult result = EditControllerEx1::initialize (context);
	if (result != kResultOk)
		return result;

	for (int i = 0; i < kParamCount; ++i)
		parameters.addParameter (new SimpleSynthParameter (kParams[i]));

	// Standard VST3 bypass, id 1000 and not part of the table.
	//
	// A STRING LIST, not the plain addParameter (title, units, 1, ...)
	// FilterDrum uses: that one prints "Off" but will not parse "Off"
	// back, and the SDK validator's parameter scan flags the round trip.
	auto* bypass = new StringListParameter (STR16 ("Bypass"), kBypass, nullptr,
	                                        ParameterInfo::kCanAutomate | ParameterInfo::kIsBypass |
	                                            ParameterInfo::kIsList);
	bypass->appendString (STR16 ("Off"));
	bypass->appendString (STR16 ("On"));
	parameters.addParameter (bypass);

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SimpleSynthController::terminate ()
{
	mEditors.clear ();
	return EditControllerEx1::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API SimpleSynthController::setComponentState (IBStream* state)
{
	if (!state)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);

	int32 version = 0;
	if (!streamer.readInt32 (version))
		return kResultFalse;

	int32 count = 0;
	if (!streamer.readInt32 (count))
		return kResultFalse;

	// DEFAULTS FIRST, exactly as the processor's setState does, and for
	// the same reason: a short stream must not leave the last project's
	// values showing. The two halves read the identical layout in the
	// identical order.
	for (int i = 0; i < kParamCount; ++i)
		setParamNormalized (kParams[i].id, kParams[i].defaultNormalized ());
	setParamNormalized (kBypass, 0.0);

	for (int32 i = 0; i < count; ++i)
	{
		double v = 0.0;
		if (!streamer.readDouble (v))
			return kResultFalse;
		if (i < kParamCount)
			setParamNormalized (static_cast<ParamID> (i), v);
	}

	int32 bypass = 0;
	if (streamer.readInt32 (bypass))
		setParamNormalized (kBypass, bypass ? 1.0 : 0.0);

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SimpleSynthController::setState (IBStream* /*state*/)
{
	return kResultOk;
}

tresult PLUGIN_API SimpleSynthController::getState (IBStream* /*state*/)
{
	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SimpleSynthController::setParamNormalized (ParamID tag, ParamValue value)
{
	const tresult result = EditControllerEx1::setParamNormalized (tag, value);
	if (result != kResultOk)
		return result;

	for (auto* editor : mEditors)
		editor->updateControl (tag, value);

	return result;
}

//------------------------------------------------------------------------
IPlugView* PLUGIN_API SimpleSynthController::createView (FIDString name)
{
	if (name && FIDStringsEqual (name, ViewType::kEditor))
		return new SimpleSynthEditor (this);
	return nullptr;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SimpleSynthController::notify (IMessage* message)
{
	if (message && FIDStringsEqual (message->getMessageID (), kSimpleSynthSampleRateMessage))
	{
		double sampleRate = 0.0;
		if (message->getAttributes ()->getFloat (kSimpleSynthSampleRateAttribute, sampleRate) == kResultOk &&
		    sampleRate > 1000.0 && sampleRate != mHostSampleRate)
		{
			mHostSampleRate = sampleRate;
			for (auto* editor : mEditors)
				editor->refreshAllReadouts ();
		}
		return kResultOk;
	}
	return EditControllerEx1::notify (message);
}

//------------------------------------------------------------------------
void SimpleSynthController::editorAttached (EditorView* editor)
{
	if (auto* e = dynamic_cast<SimpleSynthEditor*> (editor))
	{
		if (std::find (mEditors.begin (), mEditors.end (), e) == mEditors.end ())
			mEditors.push_back (e);
	}
}

//------------------------------------------------------------------------
void SimpleSynthController::editorRemoved (EditorView* editor)
{
	editorDestroyed (editor);
}

//------------------------------------------------------------------------
void SimpleSynthController::editorDestroyed (EditorView* editor)
{
	// DO NOT dynamic_cast HERE. EditorView::~EditorView() calls this,
	// and by then the SimpleSynthEditor sub-object has already been
	// destroyed: the object's dynamic type is plain EditorView, so
	// dynamic_cast<SimpleSynthEditor*> yields NULL and the entry would
	// silently survive in mEditors as a dangling pointer. The next
	// setParamNormalized - which walks that list unconditionally -
	// follows it.
	//
	// Comparing the UPCAST pointer is well defined at every point in the
	// destruction sequence, so it works on both paths.
	mEditors.erase (std::remove_if (mEditors.begin (), mEditors.end (),
	                                [editor] (SimpleSynthEditor* e) {
		                                return static_cast<EditorView*> (e) == editor;
	                                }),
	                mEditors.end ());
}

//------------------------------------------------------------------------
} // namespace SimpleSynth
