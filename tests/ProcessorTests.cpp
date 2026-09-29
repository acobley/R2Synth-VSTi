//------------------------------------------------------------------------
// SimpleSynth - processor tests
//
// The DSP suite tests the engine; this tests the VST3 half around it -
// the parts a host exercises and the SDK validator does not look at
// closely: notes landing on their own sample, the state stream, 64-bit
// buffers, bypass, and the silence flags.
//
// It drives SimpleSynthProcessor DIRECTLY, with the SDK's own hosting
// helpers (EventList, ParameterChanges, MemoryStream), so it needs the
// SDK built as libraries. tools/linux-validate/ builds it on Linux next to
// the plug-in and runs it after the validator; on the Mac the same
// CMakeLists works with the in-tree SDK. It does not need VSTGUI.
//------------------------------------------------------------------------

#include "SimpleSynthProcessor.h"
#include "SimpleSynthParams.h"

#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace SimpleSynth;

namespace {

int gChecks = 0;
int gFailures = 0;

void check (bool condition, const std::string& what)
{
	++gChecks;
	if (!condition)
	{
		++gFailures;
		std::printf ("  FAIL  %s\n", what.c_str ());
	}
}

constexpr double kRate  = 48000.0;
constexpr int    kBlock = 512;

/** One processor, set up and active, and the buffers to run it. */
struct Rig
{
	IPtr<SimpleSynthProcessor> proc = owned (new SimpleSynthProcessor);
	std::vector<float>  l32 = std::vector<float> (kBlock), r32 = std::vector<float> (kBlock);
	std::vector<double> l64 = std::vector<double> (kBlock), r64 = std::vector<double> (kBlock);
	float*  ch32[2];
	double* ch64[2];
	AudioBusBuffers out {};
	EventList events;
	ParameterChanges params;
	ProcessData data {};

	explicit Rig (bool use64 = false)
	{
		proc->initialize (nullptr);
		ProcessSetup setup { kRealtime, use64 ? kSample64 : kSample32, kBlock, kRate };
		proc->setupProcessing (setup);
		proc->setActive (true);
		proc->setProcessing (true);

		ch32[0] = l32.data (); ch32[1] = r32.data ();
		ch64[0] = l64.data (); ch64[1] = r64.data ();
		out.numChannels = 2;
		if (use64)
			out.channelBuffers64 = ch64;
		else
			out.channelBuffers32 = ch32;

		data.processMode = kRealtime;
		data.symbolicSampleSize = use64 ? kSample64 : kSample32;
		data.numSamples = kBlock;
		data.numOutputs = 1;
		data.outputs = &out;
		data.inputEvents = &events;
		data.inputParameterChanges = &params;
	}

	~Rig ()
	{
		proc->setProcessing (false);
		proc->setActive (false);
		proc->terminate ();
	}

	void note (bool on, int16 pitch, int32 offset, float velocity = 1.f)
	{
		Event e {};
		e.sampleOffset = offset;
		if (on)
		{
			e.type = Event::kNoteOnEvent;
			e.noteOn.pitch = pitch;
			e.noteOn.velocity = velocity;
			e.noteOn.noteId = -1;
		}
		else
		{
			e.type = Event::kNoteOffEvent;
			e.noteOff.pitch = pitch;
			e.noteOff.noteId = -1;
		}
		events.addEvent (e);
	}

	void param (ParamID id, double normalized)
	{
		int32 index = 0;
		if (auto* q = params.addParameterData (id, index))
		{
			int32 point = 0;
			q->addPoint (0, normalized, point);
		}
	}

	/** One block; the events and parameter changes are consumed. */
	tresult run ()
	{
		const tresult r = proc->process (data);
		events.clear ();
		params.clearQueue ();
		return r;
	}

	double peak32 (int from = 0, int to = kBlock) const
	{
		double p = 0.0;
		for (int i = from; i < to; ++i)
			p = std::max (p, (double) std::fabs (l32[i]));
		return p;
	}
};

std::vector<char> stateOf (SimpleSynthProcessor* proc)
{
	auto stream = owned (new MemoryStream);
	proc->getState (stream);
	return std::vector<char> (stream->getData (), stream->getData () + stream->getSize ());
}

} // anonymous namespace

//------------------------------------------------------------------------
int main ()
{
	std::printf ("SimpleSynth processor tests\n");
	std::printf ("---------------------------\n");

	std::printf ("notes land on their own sample\n");
	{
		Rig rig;
		rig.run ();
		check (rig.peak32 () == 0.0, "silent before any note");
		check (rig.out.silenceFlags == 3, "and says so in the silence flags");

		rig.note (true, 60, 300);
		rig.run ();
		check (rig.peak32 (0, 300) == 0.0, "nothing before the note's offset (300)");
		check (rig.peak32 (300, kBlock) > 0.0, "sound from the note's offset on");
		check (rig.out.silenceFlags == 0, "silence flags cleared while a voice sounds");

		// Several notes in one block, out of order in the list - the
		// processor sorts them.
		rig.note (true, 72, 400);
		rig.note (true, 64, 100);
		rig.run ();
		check (rig.peak32 () > 0.0, "a block with two notes still renders");

		// Release everything and run past the tail.
		rig.note (false, 60, 0);
		rig.note (false, 64, 0);
		rig.note (false, 72, 0);
		rig.run ();
		const int tailBlocks = static_cast<int> (rig.proc->getTailSamples () / kBlock) + 2;
		for (int i = 0; i < tailBlocks; ++i)
			rig.run ();
		check (rig.peak32 () == 0.0, "silent again once getTailSamples has passed");
		check (rig.out.silenceFlags == 3, "and flagged silent again");
	}

	std::printf ("velocity 0 is a note-off\n");
	{
		Rig rig;
		rig.note (true, 60, 0);
		rig.run ();
		rig.note (true, 60, 0, 0.f);
		rig.run ();
		const int tailBlocks = static_cast<int> (rig.proc->getTailSamples () / kBlock) + 2;
		for (int i = 0; i < tailBlocks; ++i)
			rig.run ();
		check (rig.peak32 () == 0.0, "a note-on at velocity 0 released the note");
	}

	std::printf ("a parameter-only block\n");
	{
		Rig rig;
		rig.param (kOutputLevel, 0.0);
		rig.note (true, 60, 0);
		rig.data.numSamples = 0;
		check (rig.run () == kResultOk, "numSamples == 0 returns kResultOk");
		rig.data.numSamples = kBlock;
		rig.run ();
		check (rig.peak32 () > 0.0, "and its note still started");
	}

	std::printf ("bypass\n");
	{
		Rig rig;
		rig.param (kBypass, 1.0);
		rig.note (true, 60, 0);
		rig.run ();
		check (rig.peak32 () == 0.0, "an instrument's bypass is silence");
		check (rig.out.silenceFlags == 3, "and flagged as such");
	}

	std::printf ("64-bit buffers carry the same audio as 32-bit\n");
	{
		Rig a (false), b (true);
		for (Rig* r : { &a, &b })
		{
			r->note (true, 55, 17);
			r->run ();
		}
		bool same = true;
		for (int i = 0; i < kBlock; ++i)
			same = same && (static_cast<double> (a.l32[i]) == b.l64[i]) && (b.l64[i] == b.r64[i]);
		check (same, "sample for sample");
		check (a.peak32 () > 0.0, "and it is not silence being compared");
	}

	std::printf ("state\n");
	{
		Rig rig;
		rig.param (kCutoff, 0.3);
		rig.param (kVco3Level, 0.7);
		rig.param (kLfoOutput, 1.0);
		rig.param (kVcaRelease, 0.9);
		rig.param (kBypass, 1.0);
		rig.run ();
		const std::vector<char> saved = stateOf (rig.proc);
		check (saved.size () == 4 + 4 + kParamCount * 8 + 4, "the stream is version, count, the table and bypass");

		Rig other;
		auto in = owned (new MemoryStream (const_cast<char*> (saved.data ()), static_cast<TSize> (saved.size ())));
		check (other.proc->setState (in) == kResultOk, "a saved state loads");
		check (stateOf (other.proc) == saved, "and reads back identically");

		// THE SHORT STREAM: version 1 with one parameter, which is what
		// the blank scaffold wrote. Everything it does not mention must
		// go back to its default, not keep what `other` was holding.
		std::vector<char> shortStream (4 + 4 + 8);
		const int32 version = 1, count = 1;
		const double level = 0.25;
		std::memcpy (shortStream.data (), &version, 4);
		std::memcpy (shortStream.data () + 4, &count, 4);
		std::memcpy (shortStream.data () + 8, &level, 8);
		auto shortIn = owned (new MemoryStream (shortStream.data (), static_cast<TSize> (shortStream.size ())));
		check (other.proc->setState (shortIn) == kResultOk, "the scaffold's one-parameter stream loads");

		const std::vector<char> after = stateOf (other.proc);
		double v = 0.0;
		std::memcpy (&v, after.data () + 8 + kOutputLevel * 8, 8);
		check (v == 0.25, "its one value is applied");
		std::memcpy (&v, after.data () + 8 + kCutoff * 8, 8);
		check (v == kParams[kCutoff].defaultNormalized (), "and everything else is back to its default");
		int32 bypass = -1;
		std::memcpy (&bypass, after.data () + after.size () - 4, 4);
		check (bypass == 0, "including bypass");

		// A stream from the future is refused rather than guessed at.
		std::vector<char> future (saved);
		const int32 v2 = 2;
		std::memcpy (future.data (), &v2, 4);
		auto futureIn = owned (new MemoryStream (future.data (), static_cast<TSize> (future.size ())));
		check (other.proc->setState (futureIn) == kResultFalse, "a version-2 stream is refused");
	}

	std::printf ("tail\n");
	{
		Rig rig;
		const uint32 atDefault = rig.proc->getTailSamples ();
		rig.param (kVcaRelease, 1.0);
		rig.run ();
		const uint32 atMax = rig.proc->getTailSamples ();
		std::printf ("  tail at the default release: %u samples; at 10 s: %u samples\n", atDefault, atMax);
		check (atMax > atDefault, "a longer release reports a longer tail");
		check (std::fabs (atMax / kRate - (10.0 * 5.0 / 3.0 + 0.5)) < 0.01, "10 s of release is 17.2 s of tail");
	}

	std::printf ("---------------------------\n");
	std::printf ("%d checks, %d failures\n", gChecks, gFailures);
	return (gFailures == 0) ? 0 : 1;
}
