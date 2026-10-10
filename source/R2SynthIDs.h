//------------------------------------------------------------------------
// R2Synth - class UIDs and message identifiers
//
// These UIDs were generated fresh for this plug-in from os.urandom.
// NEVER CHANGE THEM once a build has been shipped: hosts store them in
// the project file, so a changed UID means every existing session
// silently loses the plug-in. The same is true of the four-character
// codes in resource/au-info.plist and of both bundle identifiers in
// CMakeLists.txt.
//------------------------------------------------------------------------

#pragma once

#include "pluginterfaces/base/funknown.h"
#include "pluginterfaces/vst/vsttypes.h"

namespace R2Synth {

static const Steinberg::FUID kR2SynthProcessorUID  (0x6D69A51B, 0xD84BE44A, 0x4927F783, 0x18477FAD);
static const Steinberg::FUID kR2SynthControllerUID (0x935E5313, 0x9EF72A9E, 0xA1A91CCB, 0x968140F1);

// The plug-in category is declared once, in R2SynthEntry.cpp, using the
// SDK's own PlugType::kInstrumentSynth - not repeated as a string here.

//------------------------------------------------------------------------
// Processor <-> controller messages
//
// RESERVED SPACE, and the rule that governs it. Every message must
// travel on the UI THREAD. sendMessage from process() returns success
// and is then silently discarded by the host's connection proxy - it
// does nothing and tells you nothing, so a value the DSP produces per
// block can never come out this way. Anything the DSP computes per block
// that the panel needs goes out through data.outputParameterChanges
// instead, as a hidden read-only parameter; see the note beside kBypass
// in R2SynthParams.h.
//
// Messages ARE legitimate from setActive, setState and notify, which is
// where the one below is sent from. Add new ones here, each with a
// comment saying which of those it is sent from.
//
// The other limitation to know before adding one: if a host never
// connects the two components, a message is never delivered and nothing
// says so. That is why parameters, which cannot be lost this way, carry
// everything that can be expressed as a number - and why the panel's
// readouts fall back to 44.1 k rather than failing when this one never
// arrives.
//------------------------------------------------------------------------

/** Processor -> controller, from setActive: the sample rate the DSP is
    actually running at.

    The panel needs it because the cutoff CEILING is a fraction of it
    (cutoffCeilingHz in R2SynthDsp.h): the top of the Cutoff knob is
    20 kHz, and at 44.1 k the filter stops at 17.6 k. A readout that
    assumed one rate while the DSP ran at another would be quoting a
    ceiling nobody is hearing. */
static const char* const kR2SynthSampleRateMessage   = "R2SynthSampleRate";
static const char* const kR2SynthSampleRateAttribute = "SampleRate";

//------------------------------------------------------------------------
} // namespace R2Synth
