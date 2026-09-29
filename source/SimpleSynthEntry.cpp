//------------------------------------------------------------------------
// SimpleSynth - plug-in factory
//------------------------------------------------------------------------

#include "SimpleSynthController.h"
#include "SimpleSynthIDs.h"
#include "SimpleSynthProcessor.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"

#define stringPluginName "SimpleSynth"

using namespace Steinberg::Vst;
using namespace SimpleSynth;

//------------------------------------------------------------------------
BEGIN_FACTORY_DEF (stringCompanyName, "https://github.com/", "mailto:aecobley@googlemail.com")

	// SimpleSynth is an INSTRUMENT, and specifically a synth:
	// PlugType::kInstrumentSynth. This string is what a host reads to
	// decide which list to put it in, and it has to agree with the
	// 'aumu' type code in resource/au-info.plist and with the buses the
	// processor adds - notes in, stereo out, no audio input.
	DEF_CLASS2 (INLINE_UID_FROM_FUID (kSimpleSynthProcessorUID),
	            PClassInfo::kManyInstances,
	            kVstAudioEffectClass,
	            stringPluginName,
	            Vst::kDistributable,
	            PlugType::kInstrumentSynth,
	            FULL_VERSION_STR,
	            kVstVersionString,
	            SimpleSynthProcessor::createInstance)

	DEF_CLASS2 (INLINE_UID_FROM_FUID (kSimpleSynthControllerUID),
	            PClassInfo::kManyInstances,
	            kVstComponentControllerClass,
	            stringPluginName "Controller",
	            0,
	            "",
	            FULL_VERSION_STR,
	            kVstVersionString,
	            SimpleSynthController::createInstance)

END_FACTORY
