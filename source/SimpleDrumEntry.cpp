//------------------------------------------------------------------------
// SimpleDrum - plug-in factory
//------------------------------------------------------------------------

#include "SimpleDrumController.h"
#include "SimpleDrumIDs.h"
#include "SimpleDrumProcessor.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"

#define stringPluginName "SimpleDrum"

using namespace Steinberg::Vst;
using namespace SimpleDrum;

//------------------------------------------------------------------------
BEGIN_FACTORY_DEF (stringCompanyName, "https://github.com/", "mailto:aecobley@googlemail.com")

	// SimpleDrum is an INSTRUMENT, and specifically a drum one:
	// PlugType::kInstrumentDrum. This string is what a host reads to
	// decide which list to put it in, and it has to agree with the
	// 'aumu' type code in resource/au-info.plist and with the buses the
	// processor adds - notes in, stereo out, no audio input.
	DEF_CLASS2 (INLINE_UID_FROM_FUID (kSimpleDrumProcessorUID),
	            PClassInfo::kManyInstances,
	            kVstAudioEffectClass,
	            stringPluginName,
	            Vst::kDistributable,
	            PlugType::kInstrumentDrum,
	            FULL_VERSION_STR,
	            kVstVersionString,
	            SimpleDrumProcessor::createInstance)

	DEF_CLASS2 (INLINE_UID_FROM_FUID (kSimpleDrumControllerUID),
	            PClassInfo::kManyInstances,
	            kVstComponentControllerClass,
	            stringPluginName "Controller",
	            0,
	            "",
	            FULL_VERSION_STR,
	            kVstVersionString,
	            SimpleDrumController::createInstance)

END_FACTORY
