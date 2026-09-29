// LINUX VALIDATION HARNESS STUB - not part of the plug-in. VSTGUI's Linux
// dependencies are not installable in this container, so the editor is
// replaced by an EditorView that draws nothing. Everything else is the
// real source.
#pragma once
#include "public.sdk/source/vst/vsteditcontroller.h"
namespace SimpleSynth {
class SimpleSynthController;
class SimpleSynthEditor : public Steinberg::Vst::EditorView
{
public:
	explicit SimpleSynthEditor (SimpleSynthController* c)
	: EditorView (reinterpret_cast<Steinberg::Vst::EditController*> (c)) {}
	void updateControl (Steinberg::Vst::ParamID, Steinberg::Vst::ParamValue) {}
	void refreshAllReadouts () {}
};
}
