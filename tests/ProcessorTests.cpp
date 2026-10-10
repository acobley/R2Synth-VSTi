//------------------------------------------------------------------------
// R2Synth - processor tests
//
// The DSP suite tests the engine; this tests the VST3 half around it -
// the parts a host exercises and the SDK validator does not look at
// closely: notes landing on their own sample, the state stream, 64-bit
// buffers, bypass, and the silence flags.
//
// It drives R2SynthProcessor DIRECTLY, with the SDK's own hosting
// helpers (EventList, ParameterChanges, MemoryStream), so it needs the
// SDK built as libraries. tools/linux-validate/ builds it on Linux next to
// the plug-in and runs it after the validator; on the Mac the same
// CMakeLists works with the in-tree SDK. It does not need VSTGUI.
//------------------------------------------------------------------------

#include "R2SynthProcessor.h"
#include "R2SynthParams.h"

#include "public.sdk/source/common/memorystream.h"
#include "public.sdk/source/vst/hosting/eventlist.h"
#include "public.sdk/source/vst/hosting/parameterchanges.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

using namespace Steinberg;
using namespace Steinberg::Vst;
using namespace R2Synth;

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
	IPtr<R2SynthProcessor> proc = owned (new R2SynthProcessor);
	std::vector<float>  l32 = std::vector<float> (kBlock), r32 = std::vector<float> (kBlock);
	std::vector<double> l64 = std::vector<double> (kBlock), r64 = std::vector<double> (kBlock);
	float*  ch32[2];
	double* ch64[2];
	AudioBusBuffers out {};
	EventList events;
	ParameterChanges params;
	ParameterChanges outParams;
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
		data.outputParameterChanges = &outParams;
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
		outParams.clearQueue ();
		const tresult r = proc->process (data);
		events.clear ();
		params.clearQueue ();
		return r;
	}

	/** The value the last block published for `id`, if it published one. */
	bool published (ParamID id, double& value)
	{
		for (int32 i = 0; i < outParams.getParameterCount (); ++i)
		{
			IParamValueQueue* q = outParams.getParameterData (i);
			int32 offset = 0;
			if (q && q->getParameterId () == id && q->getPointCount () > 0 &&
			    q->getPoint (q->getPointCount () - 1, offset, value) == kResultTrue)
				return true;
		}
		return false;
	}

	double peak32 (int from = 0, int to = kBlock) const
	{
		double p = 0.0;
		for (int i = from; i < to; ++i)
			p = std::max (p, (double) std::fabs (l32[i]));
		return p;
	}
};

std::vector<char> stateOf (R2SynthProcessor* proc)
{
	auto stream = owned (new MemoryStream);
	proc->getState (stream);
	return std::vector<char> (stream->getData (), stream->getData () + stream->getSize ());
}

} // anonymous namespace

//------------------------------------------------------------------------
int main ()
{
	std::printf ("R2Synth processor tests\n");
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

	std::printf ("the live LFO Shape, for the display\n");
	{
		// LFO 2 at full Level and 2 Hz: over a second the published Shape
		// must cover most of the range, at no more than ~60 points a
		// second, and never out of 0..1.
		Rig rig;
		rig.param (kLfo2Level, 1.0);
		rig.param (kLfo2Rate, kParams[kLfo2Rate].toNormalized (2.0));
		int blocks = 0, points = 0;
		double lo = 2.0, hi = -1.0, v = 0.0;
		for (; blocks < static_cast<int> (kRate / kBlock); ++blocks)
		{
			rig.run ();
			if (rig.published (kLfoShapeLive, v))
			{
				++points;
				lo = std::min (lo, v);
				hi = std::max (hi, v);
			}
		}
		std::printf ("  %d points in %d blocks, range %.3f..%.3f\n", points, blocks, lo, hi);
		check (points >= 20, "the swept Shape is published through the second");
		check (points <= 62, "at no more than 60 a second (plus the first)");
		check (lo >= 0.0 && hi <= 1.0, "within 0..1");
		check (lo < 0.1 && hi > 0.9, "and it sweeps nearly end to end");
	}
	{
		// LFO 2 off: ONE point - the knob - and then nothing, because
		// nothing moves.
		Rig rig;
		rig.param (kLfoShape, 0.8);
		int points = 0;
		double v = 0.0, first = -1.0;
		for (int b = 0; b < 40; ++b)
		{
			rig.run ();
			if (rig.published (kLfoShapeLive, v))
			{
				if (points == 0) first = v;
				++points;
			}
		}
		check (points == 1 && std::fabs (first - 0.8) < 1e-9,
		       "with LFO 2 off it publishes the knob once and then stays quiet (" + std::to_string (points) + ")");
	}

	std::printf ("the live cutoff and resonance, for the filter screen\n");
	{
		// A note with the default +3 oct envelope: the published cutoff
		// must climb well above the knob during the attack and come back
		// towards it - the modulation, not the knob.
		Rig rig;
		const double knob = kParams[kCutoff].defaultNormalized ();
		rig.note (true, 60, 0);
		int points = 0;
		double hi = -1.0, last = -1.0, v = 0.0;
		for (int b = 0; b < static_cast<int> (kRate / kBlock); ++b)
		{
			rig.run ();
			if (rig.published (kCutoffLive, v))
			{
				++points;
				hi = std::max (hi, v);
				last = v;
				check (v >= 0.0 && v <= 1.0, "the live cutoff stays within 0..1");
			}
		}
		const double hiHz = kParams[kCutoff].toInternal (hi);
		std::printf ("  %d points; peak %.0f Hz, last %.0f Hz (knob %.0f Hz)\n", points, hiHz,
		             kParams[kCutoff].toInternal (last), kParams[kCutoff].toInternal (knob));
		check (points >= 5 && points <= 62, "the cutoff is published through the note, at most 60 a second");
		check (hiHz > kParams[kCutoff].toInternal (knob) * 4.0, "the envelope's sweep shows (two octaves and more)");
		check (last < hi, "and falls back from its peak");
	}
	{
		// LFO 1 on the resonance: the published resonance swings round
		// the knob.
		Rig rig;
		rig.param (kVcfLfoRes, kParams[kVcfLfoRes].toNormalized (50.0));
		rig.param (kLfoRate, kParams[kLfoRate].toNormalized (4.0));
		int points = 0;
		double lo = 2.0, hi = -1.0, v = 0.0;
		for (int b = 0; b < static_cast<int> (kRate / kBlock); ++b)
		{
			rig.run ();
			if (rig.published (kResonanceLive, v))
			{
				++points;
				lo = std::min (lo, v);
				hi = std::max (hi, v);
			}
		}
		const double knob = kParams[kResonance].defaultNormalized ();
		std::printf ("  resonance: %d points, %.3f..%.3f round the knob's %.3f\n", points, lo, hi, knob);
		check (points >= 20, "the swinging resonance is published through the second");
		check (lo < knob - 0.05 && hi > knob + 0.05, "and swings both sides of the knob");
	}
	{
		// Nothing moving: each published once, then quiet.
		Rig rig;
		int cut = 0, res = 0;
		double v = 0.0;
		for (int b = 0; b < 40; ++b)
		{
			rig.run ();
			if (rig.published (kCutoffLive, v)) ++cut;
			if (rig.published (kResonanceLive, v)) ++res;
		}
		check (cut == 1 && res == 1, "with nothing moving each is published once and then stays quiet (" +
		                             std::to_string (cut) + ", " + std::to_string (res) + ")");
	}

	std::printf ("portamento reaches the voice\n");
	{
		// Key track 100 %, no envelope, no LFO: the published live cutoff
		// is the pitch, so how long it takes to settle after a jump of
		// two octaves is how long the VCOs took to slide there.
		auto settleSeconds = [] (double portamentoMs, bool detached) {
			Rig rig;
			rig.param (kKeyTrack, 1.0);
			rig.param (kVcfEnvAmount, kParams[kVcfEnvAmount].toNormalized (0.0));
			rig.param (kPortamento, kParams[kPortamento].toNormalized (portamentoMs));
			rig.note (true, 48, 0);
			for (int b = 0; b < static_cast<int> (0.3 * kRate / kBlock); ++b)
				rig.run ();
			if (detached)
			{
				rig.note (false, 48, 0);
				rig.run ();
			}
			rig.note (true, 72, 0);
			double v = 0.0, last = 0.0, settledAt = -1.0;
			for (int b = 0; b < static_cast<int> (1.5 * kRate / kBlock); ++b)
			{
				rig.run ();
				if (rig.published (kCutoffLive, v))
				{
					last = v;
					// Within 1 % of the final cutoff: 72 is an octave above
					// middle C, so 2.4 kHz from the 1.2 kHz knob.
					const double hz = paramDef (kCutoff).toInternal (v);
					if (settledAt < 0.0 && hz > 2400.0 * 0.99)
						settledAt = (b + 1) * kBlock / kRate;
				}
			}
			(void) last;
			return settledAt;
		};
		const double off = settleSeconds (kMinPortamentoSeconds * 1000.0, false);
		const double legato = settleSeconds (500.0, false);
		const double detached = settleSeconds (500.0, true);
		std::printf ("  two octaves settle in: off %.3f s, 500 ms legato %.3f s, 500 ms detached %.3f s\n",
		             off, legato, detached);
		check (off >= 0.0 && off < 0.1, "off: the pitch jumps (only the 20 ms smoothing of the knobs)");
		check (legato > 0.45 && legato < 0.6, "500 ms: overlapping notes slide in the knob's time");
		check (detached > 0.45 && detached < 0.6, "and so do notes that do not overlap");
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
