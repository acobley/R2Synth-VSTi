//------------------------------------------------------------------------
// SimpleSynth - audio processor implementation
//------------------------------------------------------------------------

#include "SimpleSynthProcessor.h"
#include "SimpleSynthIDs.h"

#include "base/source/fstreamer.h"
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <algorithm>
#include <cmath>

using namespace Steinberg;
using namespace Steinberg::Vst;

namespace SimpleSynth {

//------------------------------------------------------------------------
/** Bumped whenever the layout of the state stream changes. setState
    reads it first and refuses a stream from the future rather than
    guessing at it.

    Still 1: the scaffold's version-1 stream had one parameter (the
    trim, now Output Level, still id 0), and a version-1 stream is
    COUNTED - it says how many parameters follow - so a one-parameter
    stream loads here as Output Level plus defaults for everything else.
    That is exactly what "append, never insert" buys. */
static const int32 kStateVersion = 1;

//------------------------------------------------------------------------
SimpleSynthProcessor::SimpleSynthProcessor ()
{
	setControllerClass (kSimpleSynthControllerUID);

	for (int i = 0; i < kParamCount; ++i)
		mNormalized[i] = kParams[i].defaultNormalized ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API SimpleSynthProcessor::initialize (FUnknown* context)
{
	const tresult result = AudioEffect::initialize (context);
	if (result != kResultOk)
		return result;

	// An instrument: notes in, audio out, NO AUDIO INPUT. The empty
	// input side is what makes this an aumu rather than an aufx, and it
	// is why resource/au-info.plist says 0 in / 2 out.
	addAudioOutput (STR16 ("Stereo Out"), SpeakerArr::kStereo);

	// 16 channels, the MIDI convention. Without it the plug-in is an
	// instrument nobody can play, and the symptom is silence.
	addEventInput (STR16 ("Event In"), 16);

	// EVERY PARAMETER TO THE DSP ONCE, NOW, so the engine starts on the
	// table's defaults rather than on whatever its constructor guessed.
	// A host that never calls setState - a fresh instance - would
	// otherwise play the DSP's own idea of a patch.
	for (int i = 0; i < kParamCount; ++i)
		applyParam (kParams[i].id, mNormalized[i]);
	mDsp.reset ();

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SimpleSynthProcessor::terminate ()
{
	return AudioEffect::terminate ();
}

//------------------------------------------------------------------------
tresult PLUGIN_API SimpleSynthProcessor::setBusArrangements (SpeakerArrangement* inputs, int32 numIns,
                                                             SpeakerArrangement* outputs, int32 numOuts)
{
	// Stereo out, nothing in, and nothing else accepted. This must agree
	// with the AudioComponents entry in resource/au-info.plist, which
	// lists exactly one layout - 0 in / 2 out - or auval rejects the AU.
	if (numIns != 0 || numOuts != 1)
		return kResultFalse;

	if (outputs[0] != SpeakerArr::kStereo)
		return kResultFalse;

	return AudioEffect::setBusArrangements (inputs, numIns, outputs, numOuts);
}

//------------------------------------------------------------------------
tresult PLUGIN_API SimpleSynthProcessor::canProcessSampleSize (int32 symbolicSampleSize)
{
	// BOTH, because hosts offer 64-bit buffers and one that is refused
	// silently gets a plug-in the host will not load.
	if (symbolicSampleSize == kSample32 || symbolicSampleSize == kSample64)
		return kResultTrue;
	return kResultFalse;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SimpleSynthProcessor::setupProcessing (ProcessSetup& setup)
{
	mSampleRate = setup.sampleRate;

	// ALLOCATION BELONGS HERE, not in process() - this is the only place
	// the host says how big a block to expect.
	mDsp.setSampleRate (mSampleRate);
	mDsp.setMaxBlockSize (setup.maxSamplesPerBlock);

	mScratch.assign (static_cast<size_t> (setup.maxSamplesPerBlock) * kChannelCount, 0.f);

	return AudioEffect::setupProcessing (setup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API SimpleSynthProcessor::setActive (TBool state)
{
	if (state)
	{
		mDsp.reset ();
		mLastLiveShape = -1.0;

		// THE UI THREAD, which is the only thread a message may be sent
		// from. Also the only moment that makes a panel opened LATER
		// show the truth rather than its initial guess.
		sendSampleRateToController ();
	}

	return AudioEffect::setActive (state);
}

//------------------------------------------------------------------------
void SimpleSynthProcessor::sendSampleRateToController ()
{
	if (auto* message = allocateMessage ())
	{
		FReleaser releaser (message);
		message->setMessageID (kSimpleSynthSampleRateMessage);
		message->getAttributes ()->setFloat (kSimpleSynthSampleRateAttribute, mSampleRate);
		sendMessage (message);
	}
}

//------------------------------------------------------------------------
uint32 PLUGIN_API SimpleSynthProcessor::getTailSamples ()
{
	// The VCA release CONVERTED TO THE TIME THE ENVELOPE REALLY RUNS FOR
	// (5/3 of the knob - see releaseTailSeconds), plus half a second for
	// the filter's own ringing. Only the VCA's release counts: the VCF
	// envelope is behind the VCA and cannot be heard once it has closed.
	//
	// ROUNDED UP AND GENEROUS ON PURPOSE. Too long costs a host a few
	// blocks of silence; too short truncates the release, which is a
	// click.
	const double seconds = mDsp.tailSeconds () + 0.5;
	return static_cast<uint32> (seconds * mSampleRate + 0.5);
}

//------------------------------------------------------------------------
void SimpleSynthProcessor::applyEnvelope (bool vcf)
{
	const int base = vcf ? kVcfAttack : kVcaAttack;
	auto internal = [this] (int id) { return kParams[id].toInternal (mNormalized[id]); };

	AdsrSpec spec;
	spec.attack  = internal (base + 0);
	spec.decay   = internal (base + 1);
	spec.sustain = internal (base + 2);
	spec.release = internal (base + 3);

	if (vcf)
		mDsp.setVcfEnvelope (spec);
	else
		mDsp.setVcaEnvelope (spec);
}

//------------------------------------------------------------------------
void SimpleSynthProcessor::applyParam (ParamID id, double normalized)
{
	// kBypass FIRST, because it is 1000 and is not in the table. Every
	// id is checked before anything indexes kParams.
	if (id == kBypass)
	{
		mBypass = (normalized >= 0.5);
		return;
	}

	if (!isTableParam (id))
		return;

	mNormalized[id] = normalized;

	// toInternal(), not toPlain(): the DSP is handed internal units
	// (seconds, 0..1, loop gain) and the two differ for most rows.
	const double v = paramDef (id).toInternal (normalized);

	switch (id)
	{
		case kOutputLevel:   mDsp.setOutputLevelDb (v); break;

		// THE THREE VCOs through ONE set of cases, by stride - see
		// vcoParam() in SimpleSynthParams.h.
		case kVco1Wave: case kVco2Wave: case kVco3Wave:
			mDsp.setVcoWave ((id - kVco1Wave) / kVcoParamStride,
			                 (v >= 0.5) ? Vco::Wave::Square : Vco::Wave::Saw);
			break;
		case kVco1Tune: case kVco2Tune: case kVco3Tune:
			mDsp.setVcoTune ((id - kVco1Tune) / kVcoParamStride, v);
			break;
		case kVco1Level: case kVco2Level: case kVco3Level:
			mDsp.setVcoLevel ((id - kVco1Level) / kVcoParamStride, v);
			break;

		case kLfoRate:       mDsp.setLfoRate (v); break;
		case kLfoShape:      mDsp.setLfoShape (v); break;
		case kLfoOutput:     mDsp.setLfoOutput (static_cast<LfoOutput> (static_cast<int> (v))); break;
		case kLfoPitch:      mDsp.setLfoPitchDepth (v); break;
		case kLfo2Rate:      mDsp.setLfo2Rate (v); break;
		case kLfo2Level:     mDsp.setLfo2Level (v); break;

		case kCutoff:        mDsp.setCutoffHz (v); break;
		case kResonance:     mDsp.setResonance (v); break;
		case kKeyTrack:      mDsp.setKeyTrack (v); break;
		case kVcfLfo:        mDsp.setLfoCutoffDepth (v); break;
		case kVcfLfoRes:     mDsp.setLfoResonanceDepth (v); break;
		case kVcfEnvAmount:  mDsp.setVcfEnvAmount (v); break;

		case kVcfAttack: case kVcfDecay: case kVcfSustain: case kVcfRelease:
			applyEnvelope (true);
			break;

		case kVcaLfo:        mDsp.setLfoVcaDepth (v); break;

		case kVcaAttack: case kVcaDecay: case kVcaSustain: case kVcaRelease:
			applyEnvelope (false);
			break;

		default:
			// A parameter in the table that nothing reads. Appending one
			// and forgetting this switch is the quiet failure; the
			// compiler cannot catch it with a table, so it is named here.
			break;
	}
}

//------------------------------------------------------------------------
void SimpleSynthProcessor::writeOutput (ProcessData& data, int32 offset, int32 numSamples)
{
	if (data.numOutputs < 1 || data.outputs[0].numChannels < kChannelCount || numSamples <= 0)
		return;

	const float* const left  = mScratch.data ();
	const float* const right = mScratch.data () + numSamples;

	if (data.symbolicSampleSize == kSample64)
	{
		double** out = data.outputs[0].channelBuffers64;
		for (int32 i = 0; i < numSamples; ++i)
		{
			out[0][offset + i] = static_cast<double> (left[i]);
			out[1][offset + i] = static_cast<double> (right[i]);
		}
	}
	else
	{
		float** out = data.outputs[0].channelBuffers32;
		std::copy (left,  left  + numSamples, out[0] + offset);
		std::copy (right, right + numSamples, out[1] + offset);
	}
}

//------------------------------------------------------------------------
tresult PLUGIN_API SimpleSynthProcessor::process (ProcessData& data)
{
	// PARAMETERS FIRST, ALWAYS - before the early return below, or a
	// parameter-only block would be thrown away along with the block.
	//
	// THE LAST POINT IN THE BLOCK, not sample-accurate: the DSP's own
	// smoothers are what keep a mid-block move from being audible.
	if (IParameterChanges* changes = data.inputParameterChanges)
	{
		const int32 count = changes->getParameterCount ();
		for (int32 i = 0; i < count; ++i)
		{
			IParamValueQueue* queue = changes->getParameterData (i);
			if (queue == nullptr)
				continue;
			const int32 points = queue->getPointCount ();
			int32 offset = 0;
			ParamValue value = 0.0;
			if (points > 0 && queue->getPoint (points - 1, offset, value) == kResultTrue)
				applyParam (queue->getParameterId (), value);
		}
	}

	const int32 numSamples = data.numSamples;

	//--------------------------------------------------------------------
	// The notes, collected with their offsets and SORTED. Hosts deliver
	// them in order, but nothing in the specification promises it, and
	// an event out of order would make the segment loop below render
	// backwards.
	//--------------------------------------------------------------------
	struct NoteEvent { int32 offset; int16 pitch; bool on; };
	NoteEvent events[kMaxEventsPerBlock];
	int eventCount = 0;

	if (data.inputEvents != nullptr)
	{
		const int32 count = data.inputEvents->getEventCount ();
		for (int32 i = 0; i < count && eventCount < kMaxEventsPerBlock; ++i)
		{
			Event e = {};
			if (data.inputEvents->getEvent (i, e) != kResultOk)
				continue;

			NoteEvent n {};
			n.offset = std::max<int32> (0, std::min<int32> (e.sampleOffset,
			                                                 std::max<int32> (0, numSamples - 1)));
			if (e.type == Event::kNoteOnEvent)
			{
				n.pitch = e.noteOn.pitch;
				// A NOTE-ON AT VELOCITY ZERO IS A NOTE-OFF - the MIDI
				// running-status convention, which some hosts pass
				// through to VST3 unchanged.
				n.on = (e.noteOn.velocity > 0.f);
			}
			else if (e.type == Event::kNoteOffEvent)
			{
				n.pitch = e.noteOff.pitch;
				n.on = false;
			}
			else
			{
				continue;
			}
			events[eventCount++] = n;
		}
	}

	std::stable_sort (events, events + eventCount,
	                  [] (const NoteEvent& a, const NoteEvent& b) { return a.offset < b.offset; });

	// A PARAMETER-ONLY BLOCK: numSamples == 0, or no output bus at all.
	// Its notes still count - a key released in an empty block must
	// still release - so they are applied before returning.
	if (numSamples <= 0 || data.numOutputs < 1)
	{
		for (int i = 0; i < eventCount; ++i)
			events[i].on ? mDsp.noteOn (events[i].pitch, 1.f) : mDsp.noteOff (events[i].pitch);
		return kResultOk;
	}

	// NEVER TRUST THE BLOCK SIZE. Growing the scratch here would be an
	// allocation on the audio thread, so an oversize block is refused.
	if (mScratch.size () < static_cast<size_t> (numSamples) * kChannelCount)
		return kResultFalse;

	//--------------------------------------------------------------------
	// THE SEGMENT LOOP. Each segment is [position, next event), so a
	// note takes effect on its own sample.
	//--------------------------------------------------------------------
	int32 position = 0;
	int next = 0;
	while (position < numSamples)
	{
		while (next < eventCount && events[next].offset <= position)
		{
			const NoteEvent& e = events[next++];
			e.on ? mDsp.noteOn (e.pitch, 1.f) : mDsp.noteOff (e.pitch);
		}

		const int32 end = (next < eventCount) ? events[next].offset : numSamples;
		const int32 length = end - position;

		// The segment renders into the START of the scratch, laid out
		// as length samples of left then length of right, and is copied
		// out at its offset - so the scratch only ever needs one block.
		float* const left  = mScratch.data ();
		float* const right = mScratch.data () + length;

		if (mBypass)
		{
			// AN INSTRUMENT'S BYPASS IS SILENCE, not a dry path.
			std::fill (left,  left  + length, 0.f);
			std::fill (right, right + length, 0.f);
		}
		else
		{
			mDsp.render (left, right, length);
		}

		writeOutput (data, position, length);
		position = end;
	}

	// SILENCE FLAGS only when it really is silent: bypassed, or the VCA
	// envelope idle with no key down. A bus flagged silent that is not
	// is far worse than one that is not flagged, so anything else
	// clears them.
	data.outputs[0].silenceFlags = 0;
	if (mBypass || !mDsp.voiceActive ())
		for (int32 c = 0; c < data.outputs[0].numChannels; ++c)
			data.outputs[0].silenceFlags |= static_cast<uint64> (1) << c;

	mSamplesSinceLive += numSamples;
	if (!mBypass)
		publishLiveShape (data);

	return kResultOk;
}

//------------------------------------------------------------------------
void SimpleSynthProcessor::publishLiveShape (ProcessData& data)
{
	// THROUGH outputParameterChanges, the one route from process() to
	// the panel that is not silently dropped - see SimpleSynthIDs.h.
	if (data.outputParameterChanges == nullptr || data.numSamples <= 0)
		return;

	const bool forced = (mLastLiveShape < 0.0);
	if (!forced && mSamplesSinceLive < mSampleRate / kLiveShapeUpdatesPerSecond)
		return;

	// kLfoShape's own normalisation: -1..+1 to 0..1.
	const double normalized = std::clamp (0.5 * (mDsp.lastLfoShape () + 1.0), 0.0, 1.0);
	if (!forced && std::fabs (normalized - mLastLiveShape) < 1.0e-3)
		return;

	int32 index = 0;
	if (IParamValueQueue* queue = data.outputParameterChanges->addParameterData (kLfoShapeLive, index))
	{
		int32 point = 0;
		queue->addPoint (data.numSamples - 1, normalized, point);
		mLastLiveShape = normalized;
		mSamplesSinceLive = 0.0;
	}
}

//------------------------------------------------------------------------
tresult PLUGIN_API SimpleSynthProcessor::getState (IBStream* state)
{
	if (state == nullptr)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);

	streamer.writeInt32 (kStateVersion);

	// THE COUNT IS WRITTEN, so a stream from an older build with fewer
	// parameters can be read by a newer one. This is the other half of
	// "append, never insert".
	streamer.writeInt32 (kNumParams);

	// NORMALISED, not plain. The plain range is allowed to change between
	// versions, and a stream full of plain values would silently rescale.
	for (int i = 0; i < kParamCount; ++i)
		streamer.writeDouble (mNormalized[i]);

	streamer.writeInt32 (mBypass ? 1 : 0);

	return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API SimpleSynthProcessor::setState (IBStream* state)
{
	if (state == nullptr)
		return kResultFalse;

	IBStreamer streamer (state, kLittleEndian);

	int32 version = 0;
	if (!streamer.readInt32 (version))
		return kResultFalse;
	if (version > kStateVersion)
		return kResultFalse;   // written by a newer build - do not guess

	int32 count = 0;
	if (!streamer.readInt32 (count))
		return kResultFalse;

	//--------------------------------------------------------------------
	// EVERYTHING THE STREAM DOES NOT MENTION GOES BACK TO ITS DEFAULT.
	//
	// A host reuses one plug-in instance as the user loads project after
	// project. If setState only applied what the stream contained, a
	// short stream - written by an older build, or truncated - would
	// leave every unmentioned parameter holding the PREVIOUS project's
	// value. The controller's setComponentState does exactly the same
	// thing in the same order, reading the identical layout.
	//--------------------------------------------------------------------
	for (int i = 0; i < kParamCount; ++i)
		applyParam (kParams[i].id, kParams[i].defaultNormalized ());
	mBypass = false;

	for (int32 i = 0; i < count; ++i)
	{
		double v = 0.0;
		if (!streamer.readDouble (v))
			return kResultFalse;
		if (i < kParamCount)
			applyParam (static_cast<ParamID> (i), v);
	}

	// OPTIONAL BY DESIGN, so a later version can append fields without
	// every older reader failing.
	int32 bypass = 0;
	if (streamer.readInt32 (bypass))
		mBypass = (bypass != 0);

	// The smoothers must not GLIDE to a loaded project's values from the
	// last one's - that is an audible swoop on every project load.
	mDsp.reset ();

	return kResultOk;
}

//------------------------------------------------------------------------
} // namespace SimpleSynth
