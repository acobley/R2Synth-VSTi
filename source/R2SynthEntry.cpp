//------------------------------------------------------------------------
// R2Synth - plug-in factory
//------------------------------------------------------------------------

#include "R2SynthController.h"
#include "R2SynthIDs.h"
#include "R2SynthProcessor.h"
#include "version.h"

#include "public.sdk/source/main/pluginfactory.h"

#define stringPluginName "R2Synth"

using namespace Steinberg::Vst;
using namespace R2Synth;

//------------------------------------------------------------------------
BEGIN_FACTORY_DEF (stringCompanyName, "https://github.com/", "mailto:aecobley@googlemail.com")

	// R2Synth is an INSTRUMENT, and specifically a synth:
	// PlugType::kInstrumentSynth. This string is what a host reads to
	// decide which list to put it in, and it has to agree with the
	// 'aumu' type code in resource/au-info.plist and with the buses the
	// processor adds - notes in, stereo out, no audio input.
	DEF_CLASS2 (INLINE_UID_FROM_FUID (kR2SynthProcessorUID),
	            PClassInfo::kManyInstances,
	            kVstAudioEffectClass,
	            stringPluginName,
	            Vst::kDistributable,
	            PlugType::kInstrumentSynth,
	            FULL_VERSION_STR,
	            kVstVersionString,
	            R2SynthProcessor::createInstance)

	DEF_CLASS2 (INLINE_UID_FROM_FUID (kR2SynthControllerUID),
	            PClassInfo::kManyInstances,
	            kVstComponentControllerClass,
	            stringPluginName "Controller",
	            0,
	            "",
	            FULL_VERSION_STR,
	            kVstVersionString,
	            R2SynthController::createInstance)

END_FACTORY
