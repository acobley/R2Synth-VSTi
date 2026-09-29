//------------------------------------------------------------------------
// SimpleSynth - audio processor
//
// SimpleSynth is an INSTRUMENT: an event input and one stereo audio
// output, no audio input. That decision reaches FOUR places and they
// must all agree, or the plug-in loads in the wrong list, or auval
// rejects the Audio Unit:
//
//   * PlugType::kInstrumentSynth in SimpleSynthEntry.cpp
//   * the buses added in initialize() below
//   * what setBusArrangements accepts
//   * the single 0-in / 2-out entry in resource/au-info.plist
//
// auval checks the last two against each other.
//
// THE BLOCK IS RENDERED IN SEGMENTS. A note that always took effect at
// offset 0 would be quantised to the block size - 512 samples is 11 ms at
// 44.1 k, which is audible on a fast run - so process() splits the block
// at every event's sampleOffset and renders the pieces in order.
//
// THE PROCESS CONTEXT IS NOT ASKED FOR, and this is the note about why
// that is a decision rather than an omission.
//
//   Since VST3 3.7 the ProcessContext is OPT-IN and the default is NO
//   FLAGS. Without a getProcessContextRequirements override,
//   data.processContext arrives with nothing valid in it: everything
//   that reads the tempo silently gets 120 in every host, and the
//   validator prints "- None" rather than complaining. An LFO synced to
//   the tempo would simply never sync and nothing would say why.
//
//   The LFO is free-running in Hz, like the MS-20's, so nothing here
//   reads the tempo. THE MOMENT ANYTHING DOES - a tempo-synced LFO rate,
//   a key-synced restart on the bar - add the override back as:
//
//       Steinberg::uint32 PLUGIN_API getProcessContextRequirements ()
//           SMTG_OVERRIDE
//       {
//           processContextRequirements.needTempo ();
//           processContextRequirements.needTransportState ();
//           return AudioEffect::getProcessContextRequirements ();
//       }
//
//   asking for only the fields actually read.
//------------------------------------------------------------------------

#pragma once

#include "SimpleSynthDsp.h"
#include "SimpleSynthParams.h"

#include "public.sdk/source/vst/vstaudioeffect.h"
#include "pluginterfaces/vst/ivstevents.h"

#include <vector>

namespace SimpleSynth {

//------------------------------------------------------------------------
class SimpleSynthProcessor : public Steinberg::Vst::AudioEffect
{
public:
	SimpleSynthProcessor ();

	static Steinberg::FUnknown* createInstance (void*)
	{
		return (Steinberg::Vst::IAudioProcessor*)new SimpleSynthProcessor;
	}

	Steinberg::tresult PLUGIN_API initialize (Steinberg::FUnknown* context) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API terminate () SMTG_OVERRIDE;

	Steinberg::tresult PLUGIN_API setBusArrangements (Steinberg::Vst::SpeakerArrangement* inputs,
	                                                  Steinberg::int32 numIns,
	                                                  Steinberg::Vst::SpeakerArrangement* outputs,
	                                                  Steinberg::int32 numOuts) SMTG_OVERRIDE;

	Steinberg::tresult PLUGIN_API canProcessSampleSize (Steinberg::int32 symbolicSampleSize) SMTG_OVERRIDE;

	Steinberg::tresult PLUGIN_API setupProcessing (Steinberg::Vst::ProcessSetup& setup) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API setActive (Steinberg::TBool state) SMTG_OVERRIDE;

	Steinberg::tresult PLUGIN_API process (Steinberg::Vst::ProcessData& data) SMTG_OVERRIDE;

	Steinberg::tresult PLUGIN_API setState (Steinberg::IBStream* state) SMTG_OVERRIDE;
	Steinberg::tresult PLUGIN_API getState (Steinberg::IBStream* state) SMTG_OVERRIDE;

	/** How long the voice keeps sounding after the last key-up.

	    DECLARED so the host feeds silence for that long and the release
	    plays out through the normal path. Without it a host is entitled
	    to stop calling process() the moment the notes stop, and every
	    release would be cut off - which sounds like a gated VCA and is
	    not one. */
	Steinberg::uint32 PLUGIN_API getTailSamples () SMTG_OVERRIDE;

private:
	/** One normalised value onto the thing that owns it. The single
	    place a parameter id turns into DSP state. */
	void applyParam (Steinberg::Vst::ParamID id, double normalized);

	/** The ADSRs take four parameters at once, so a change to any one
	    rebuilds that envelope's spec from all four. */
	void applyEnvelope (bool vcf);

	/** Tell the controller the rate the DSP is really running at. From
	    setActive - the UI thread - and never from process(). */
	void sendSampleRateToController ();

	/** Write the DSP's block out to the host's bus at `offset`,
	    converting to 64-bit if that is what it asked for. */
	void writeOutput (Steinberg::Vst::ProcessData& data, Steinberg::int32 offset,
	                  Steinberg::int32 numSamples);

	SimpleSynthDsp mDsp;

	/** The most events one block can hold. Overflowing it drops the
	    surplus rather than growing on the audio thread. */
	static constexpr int kMaxEventsPerBlock = 256;

	double mSampleRate = 44100.0;
	bool   mBypass     = false;

	/** What the host last sent for each table parameter, normalised.
	    Written by applyParam, read by getState and by applyEnvelope.
	    Initialised from the table in the constructor, so a getState
	    before any parameter has moved writes the defaults rather than
	    zeros.

	    THE ONE COPY OF A VALUE OUTSIDE THE DSP, and deliberately so:
	    reading the values back out of the DSP would need an inverse of
	    every mapping, and an inverse is a second copy of the arithmetic. */
	double mNormalized[kNumParams] = {};

	/** THE 32-BIT SCRATCH THE DSP ALWAYS RENDERS INTO. Sized in
	    setupProcessing and never resized anywhere else. A host may hand
	    over 64-bit buffers; the DSP is float throughout and writeOutput
	    widens on the way out. */
	std::vector<float> mScratch;
};

//------------------------------------------------------------------------
} // namespace SimpleSynth
