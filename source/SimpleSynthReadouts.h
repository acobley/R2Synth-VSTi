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

#include "SimpleSynthDsp.h"
#include "SimpleSynthLayout.h"
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

/** The LFO display's caption - it says when LFO 2 is sweeping the
    Shape, because then the picture moves on its own. */
const char* lfoDisplayCaption (double lfo2Level);

/** The engine line: rate, cutoff ceiling, release tail. In the
    header, right of the wordmark. */
std::string engineLineText (double sampleRate, double vcaReleaseSeconds);

/** The word under a control: the layout's own when it gives one ("Env
    Amt", "A", "Cutoff" for the LFO's cutoff depth), else shortLabel(). */
std::string controlLabel (const Layout::Control& control);

/** THE FILTER DISPLAY'S DATA: the ladder's response for the Cutoff and
    Resonance KNOBS - traceFilterResponse(), so the curve is the filter's
    own transfer function set up the way process() sets it up - mapped
    to 0..1 of the display's height (Layout::kFilterTopDb at the top,
    kFilterBottomDb at the bottom, clamped), over kFilterMinHz..
    kFilterMaxHz. `markerX` / `markerY` (0..1, y up) are where the cutoff
    sits on that curve, for the dot. */
void filterDisplayData (double cutoffHz, double resonanceK, double sampleRate,
                        float* curve, int count, double& markerX, double& markerY,
                        FilterType type = FilterType::Moog);

/** What the filter display shows, in words: "live" when it is drawing
    the cutoff and resonance the processor reports (the knobs moved by
    the envelope, key tracking and LFO 1), "at the knobs" when it has
    none - no audio running, or a knob has just moved - followed by the
    chosen filter's slope, "24 dB/oct" (Moog) or "12 dB/oct" (either MS-20),
    since the switch's names alone do not say it. */
std::string filterDisplayCaption (bool live = false, FilterType type = FilterType::Moog);

/** The VCF Type, as the DSP's enum, from its parameter's internal
    value - filterTypeFromInternal(), the processor's own copy. */
inline FilterType filterTypeOf (double internal)
{
	return filterTypeFromInternal (internal);
}

/** A screen's time span as printed: "929 ms", "2.40 s". */
std::string spanText (double seconds);

/** The filter envelope's Amount WITH ITS SIGN, for its screen: an
    inverted envelope is the same shape as an upright one, so only the
    text can say which it is. */
std::string envAmountText (double octaves);

/** The filter screen's figure: the Cutoff and Resonance readings. */
std::string filterDisplayAnnotation (double cutoffNormalized, double resonanceNormalized);

/** The line along the foot of the modulation strip: LFO 1's line and
    LFO 2's, side by side. */
std::string modInfoLineText (const std::function<double (Steinberg::Vst::ParamID)>& internalOf);

} // namespace SimpleSynth
