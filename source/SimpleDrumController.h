//------------------------------------------------------------------------
// SimpleDrum - edit controller
//
// The half of the plug-in that talks to the host about parameters. It
// does NOT share memory with the processor - they may not even be in the
// same process - which is the reason for two rules that run through this
// project:
//
//   * anything a panel displays that the DSP computes comes from one
//     shared function both call (SimpleDrumDsp.h), not from two copies
//     of the arithmetic;
//   * a value the DSP produces per block reaches here through
//     data.outputParameterChanges, never through sendMessage from
//     process() (SimpleDrumIDs.h).
//
// NO EDITOR YET, deliberately: PORTING-GUIDE.md step 6 is a silent
// plug-in validating with processor, controller and entry and nothing
// else. The host's generic editor shows the parameters. createView is
// not overridden, so the base class returns null, which is how a VST3
// says "use your own".
//
// WHEN THE EDITOR ARRIVES it needs five overrides here - createView,
// editorAttached, editorRemoved, editorDestroyed and a setParamNormalized
// that forwards to it - and one trap in editorDestroyed: EditorView's
// destructor calls it, by which point the derived editor is gone, so
// dynamic_cast<SimpleDrumEditor*> returns NULL and the entry survives in
// the list as a dangling pointer. COMPARE UPCAST POINTERS instead.
// ../FilterDrum-VSTi/source/FilterDrumController.cpp has the worked
// version; ../SpyBand-VSTi/source/SpyBandControls.{h,cpp} is the
// liftable control set (no bitmaps - a namespace rename is all it needs).
//------------------------------------------------------------------------

#pragma once

#include "SimpleDrumParams.h"

#include "public.sdk/source/vst/vsteditcontroller.h"

namespace SimpleDrum {

//------------------------------------------------------------------------
class SimpleDrumController : public Steinberg::Vst::EditControllerEx1
{
public:
	SimpleDrumController () = default;
	~SimpleDrumController () SMTG_OVERRIDE = default;

	static Steinberg::FUnknown* createInstance (void*)
	{
		return (Steinberg::Vst::IEditController*)new SimpleDrumController;
	}

	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;

	/** The PROCESSOR's state, which is the one that matters - it is what
	    the host saved. Reads the identical layout getState wrote. */
	Steinberg::tresult PLUGIN_API setComponentState (Steinberg::IBStream* state) SMTG_OVERRIDE;

	/** The CONTROLLER's own state - panel-only settings with no
	    parameter behind them. There are none yet, so both are no-ops
	    that return kResultOk rather than kResultFalse: a host that saves
	    an empty controller state and gets a failure back logs an error
	    for nothing. */
	Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;
};

//------------------------------------------------------------------------
} // namespace SimpleDrum
