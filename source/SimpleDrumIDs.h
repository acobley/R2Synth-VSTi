//------------------------------------------------------------------------
// SimpleDrum - class UIDs and message identifiers
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

namespace SimpleDrum {

static const Steinberg::FUID kSimpleDrumProcessorUID  (0x6D69A51B, 0xD84BE44A, 0x4927F783, 0x18477FAD);
static const Steinberg::FUID kSimpleDrumControllerUID (0x935E5313, 0x9EF72A9E, 0xA1A91CCB, 0x968140F1);

// The plug-in category is declared once, in SimpleDrumEntry.cpp, using the
// SDK's own PlugType::kInstrumentDrum - not repeated as a string here.

//------------------------------------------------------------------------
// Processor <-> controller messages
//
// RESERVED SPACE, and the rule that governs it. Every message must
// travel on the UI THREAD. sendMessage from process() returns success
// and is then silently discarded by the host's connection proxy - it
// does nothing and tells you nothing, so a value the DSP produces per
// block can never come out this way. Anything the DSP computes per block
// that the controller (or, later, a panel) needs goes out through data.outputParameterChanges
// instead, as a hidden read-only parameter; see the note beside kBypass
// in SimpleDrumParams.h.
//
// Messages ARE legitimate from setActive, setState and notify. Add them
// here, each with a comment saying which of those it is sent from.
//
// The other limitation to know before adding one: if a host never
// connects the two components, a message is never delivered and nothing
// says so. That is why parameters, which cannot be lost this way, carry
// everything that can be expressed as a number.
//
// THERE ARE NONE YET, deliberately. With no editor there is nothing on
// the controller side to receive one; the first candidate is the sample
// rate the DSP runs at (processor -> controller, from setActive), which
// a panel needs as soon as it draws anything that is a function of f/fs.
// ../FilterDrum-VSTi/source/FilterDrumIDs.h has the worked version.
//------------------------------------------------------------------------

//------------------------------------------------------------------------
} // namespace SimpleDrum
