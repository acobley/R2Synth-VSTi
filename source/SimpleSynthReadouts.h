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

/** What a selector shows as its reading: the position's name, from
    enumName() - the same word the host's automation lane uses. The
    label ("Wave", "Output") is drawn under it like a slider's, by the
    compact SpySelector. */
std::string selectorText (Steinberg::Vst::ParamID tag, int position);

/** THE THREE READOUT LINES beside the two LFO groups. `internalOf`
    gives a parameter's current value in internal units.

      lfoLineText        LFO 1's period and which output is patched
      lfoDepthsLineText  where it goes - pitch, cutoff, VCA
      lfo2LineText       LFO 2's period and the Shape range it sweeps */
std::string lfoLineText (const std::function<double (Steinberg::Vst::ParamID)>& internalOf);
std::string lfoDepthsLineText (const std::function<double (Steinberg::Vst::ParamID)>& internalOf);
std::string lfo2LineText (const std::function<double (Steinberg::Vst::ParamID)>& internalOf);

/** The two fixed lines across the top of the panel - here so the panel
    picture's tool prints the same words. */
const char* panelTitleText ();
const char* panelFlowText ();

/** THE LFO DISPLAY'S DATA, shared by the editor and the panel picture:
    `cycles` cycles of the PATCHED output (drawn green) and of a
    companion (drawn red) - the square when the Shape output is patched,
    otherwise the shape output, because that is what the Shape knob is
    doing to the triangle underneath. Both 0..1 for the view, and the two
    legend words, the patched one in capitals. */
void lfoDisplayData (double outputPosition, double shape, float* patched, float* companion,
                     int count, double cycles, std::string& patchedLegend,
                     std::string& companionLegend);

/** The engine line: rate, cutoff ceiling, release tail. On the title
    row, right of the title. */
std::string engineLineText (double sampleRate, double vcaReleaseSeconds);

} // namespace SimpleSynth
