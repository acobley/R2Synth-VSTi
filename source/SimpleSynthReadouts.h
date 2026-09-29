//------------------------------------------------------------------------
// SimpleSynth - the panel's text
//
// EVERY STRING THE PANEL SHOWS THAT DEPENDS ON A VALUE is made here, with
// no VSTGUI in sight, for the same reason the layout is in its own
// header: so that the thing that draws docs/panel.png
// (tools/dump-layout.cpp) prints the SAME text the editor does, rather
// than a Python guess at it, and so the text can be exercised without a
// host.
//
// The numbers come from the parameter table and from the shared
// functions in SimpleSynthDsp.h - never from arithmetic written here.
//------------------------------------------------------------------------

#pragma once

#include "SimpleSynthParams.h"

#include <functional>
#include <string>

namespace SimpleSynth {

/** The reading drawn across the top of a slider. */
std::string readoutText (Steinberg::Vst::ParamID tag, double normalized);

/** The label under a control: its title with the section prefix
    stripped - "VCF Attack" is "Attack" inside the VCF ENV box. */
std::string shortLabel (Steinberg::Vst::ParamID tag);

/** What a selector shows: its label and the position's name, from
    enumName() - "Wave: Saw". The label is in the text because
    SpySelector draws no label of its own, and "Square" alone could be
    either of the two switches that have a Square. */
std::string selectorText (Steinberg::Vst::ParamID tag, int position);

/** The LFO line beside the LFO group. `internalOf` gives a parameter's
    current value in internal units. */
std::string lfoLineText (const std::function<double (Steinberg::Vst::ParamID)>& internalOf);

/** The engine line: rate, cutoff ceiling, release tail. */
std::string engineLineText (double sampleRate, double vcaReleaseSeconds);

} // namespace SimpleSynth
