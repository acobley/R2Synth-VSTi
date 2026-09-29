//------------------------------------------------------------------------
// SimpleSynth - DSP tests
//
// SDK-FREE, compiled directly against SimpleSynthDsp.cpp:
//
//   c++ -std=c++17 -O2 -ffp-contract=off -I../source DspTests.cpp
//       ../source/SimpleSynthDsp.cpp -o /tmp/dsptests  &&  /tmp/dsptests
//
// -ffp-contract=off MATTERS: see the ladder's bit-identity tests.
//
// (tools/run-tests.sh does that, and the layout suite.)
//
// Everything that can be asserted without a host is asserted here, and
// MEASURED rather than assumed wherever a number can be measured: the
// VCOs' pitch by counting cycles, the ladder's tuning by finding its
// resonant peak, the self-oscillation threshold by ringing the filter
// either side of it, the envelopes' times by timing them.
//
// The suite ends by MEASURING THE DEFAULT PATCH'S OUTPUT LEVEL and
// printing it in dBFS, with the worst case alongside it. A level that
// clips masks other faults and sends you chasing the wrong bug.
//------------------------------------------------------------------------

#include "SimpleSynthDsp.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

using namespace SimpleSynth;

//------------------------------------------------------------------------
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

void checkClose (double got, double want, double tolerance, const std::string& what)
{
	++gChecks;
	if (!(std::fabs (got - want) <= tolerance))
	{
		++gFailures;
		std::printf ("  FAIL  %s: got %.9g, want %.9g (tol %.3g)\n",
		             what.c_str (), got, want, tolerance);
	}
}

std::string num (double v, const char* fmt = "%g")
{
	char b[48];
	std::snprintf (b, sizeof (b), fmt, v);
	return b;
}

const double kRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0 };

double peakOf (const std::vector<float>& block, size_t from = 0)
{
	float peak = 0.f;
	for (size_t i = from; i < block.size (); ++i)
		peak = std::max (peak, std::fabs (block[i]));
	return peak;
}

double peakDbFS (const std::vector<float>& block, size_t from = 0)
{
	const double p = peakOf (block, from);
	return (p <= 0.0) ? -std::numeric_limits<double>::infinity () : 20.0 * std::log10 (p);
}

/** Frequency by counting upward zero crossings, interpolated. */
double measureFrequency (const std::vector<double>& x, double sampleRate)
{
	double first = -1.0, last = -1.0;
	int crossings = 0;
	for (size_t i = 1; i < x.size (); ++i)
	{
		if (x[i - 1] < 0.0 && x[i] >= 0.0)
		{
			const double t = (i - 1) + (-x[i - 1]) / (x[i] - x[i - 1]);
			if (first < 0.0)
				first = t;
			else
				++crossings;
			last = t;
		}
	}
	return (crossings > 0) ? crossings * sampleRate / (last - first) : 0.0;
}

/** THE PATCH THE PARAMETER TABLE SHIPS WITH, in internal units - kept in
    step with SimpleSynthParams.cpp by hand, because this suite does not
    include SDK headers. If one moves, move the other. */
void applyDefaultPatch (SimpleSynthDsp& d)
{
	d.setOutputLevelDb (0.0);
	d.setVcoWave (0, Vco::Wave::Saw);    d.setVcoTune (0, 0.0);   d.setVcoLevel (0, 1.0);
	d.setVcoWave (1, Vco::Wave::Square); d.setVcoTune (1, -12.0); d.setVcoLevel (1, 0.5);
	d.setVcoWave (2, Vco::Wave::Saw);    d.setVcoTune (2, 0.0);   d.setVcoLevel (2, 0.0);
	d.setLfoRate (4.0);
	d.setLfoShape (0.0);
	d.setLfoSquareOutput (false);
	d.setLfoPitchDepth (0.0);
	d.setCutoffHz (1200.0);
	d.setResonance (0.20 * kMaxResonanceK);
	d.setKeyTrack (0.5);
	d.setLfoCutoffDepth (0.0);
	d.setVcfEnvAmount (3.0);
	d.setVcfEnvelope ({ 0.005, 0.300, 0.30, 0.300 });
	d.setLfoVcaDepth (0.0);
	d.setVcaEnvelope ({ 0.003, 0.500, 0.80, 0.250 });
}

/** SpaceDub's DelayLine filter loop, copied verbatim from
    Spacedub-VSTi/source/DelayLine.cpp (less the noise and the taps) -
    the reference MoogLadder is held bit-identical to at zero feedback. */
struct SpaceDubReference
{
	double mIn1 = 0, mIn2 = 0, mIn3 = 0, mIn4 = 0, mOut1 = 0, mOut2 = 0, mOut3 = 0, mOut4 = 0;

	static double scrub (double v)
	{
		if (!std::isfinite (v)) return 0.0;
		if (v > -1.0e-15 && v < 1.0e-15) return 0.0;
		return std::clamp (v, -16.0, 16.0);
	}

	double tick (double eqSample, double mFrequency, double mResonance)
	{
		const double f  = mFrequency * 1.16;
		const double fb = mResonance * (1.0 - 0.15 * f * f);
		eqSample -= mOut4 * fb;
		eqSample *= 0.35013 * (f * f) * (f * f);
		mOut1 = scrub (eqSample + 0.3 * mIn1 + (1.0 - f) * mOut1); mIn1 = eqSample;
		mOut2 = scrub (mOut1 + 0.3 * mIn2 + (1.0 - f) * mOut2);    mIn2 = mOut1;
		mOut3 = scrub (mOut2 + 0.3 * mIn3 + (1.0 - f) * mOut3);    mIn3 = mOut2;
		mOut4 = scrub (mOut3 + 0.3 * mIn4 + (1.0 - f) * mOut4);    mIn4 = mOut3;
		return mOut4;
	}
};

} // anonymous namespace

//------------------------------------------------------------------------
// 1. The shared conversions
//------------------------------------------------------------------------
static void testConversions ()
{
	std::printf ("conversions\n");

	check (dbToGain (0.0) == 1.0, "0 dB is exactly unity");
	checkClose (dbToGain (-48.0), 0.003981072, 1e-9, "-48 dB (bottom of Output Level)");
	checkClose (dbToGain (6.0),   1.995262315, 1e-9, "+6 dB (top of Output Level)");
	for (double db : { -48.0, -12.0, 0.0, 6.0 })
		checkClose (gainToDb (dbToGain (db)), db, 1e-9, "dB round trip at " + num (db));
	check (gainToDb (0.0) == -180.0, "zero gain reads -180 dB, not -inf");

	checkClose (noteToHz (69.0), 440.0, 1e-9, "A4 is 440 Hz");
	checkClose (noteToHz (57.0), 220.0, 1e-9, "an octave down halves it");
	checkClose (noteToHz (81.0), 880.0, 1e-9, "an octave up doubles it");

	checkClose (cutoffCeilingHz (44100.0), 17640.0, 1e-9, "cutoff ceiling at 44.1 k");
	checkClose (cutoffCeilingHz (48000.0), 19200.0, 1e-9, "cutoff ceiling at 48 k");

	checkClose (releaseTailSeconds (0.3), 0.5, 1e-9, "a release runs 5/3 of its knob");

	check (!selfOscillating (0.999) && selfOscillating (1.0), "self-oscillation starts at loop gain 1");
}

//------------------------------------------------------------------------
// 2. The smoother never steps hard enough to click
//------------------------------------------------------------------------
static void testSmootherStep ()
{
	std::printf ("smoother step limit\n");

	for (double rate : kRates)
	{
		Smoother s;
		s.setSampleRate (rate, kTrimSmoothingSeconds);
		s.snap (0.f);
		s.setTarget (1.f);

		check (s.coefficient () <= 0.01f, "coefficient <= 1% at " + num (rate));

		float previous = s.value (), worst = 0.f;
		for (int i = 0; i < static_cast<int> (rate); ++i)
		{
			const float v = s.next ();
			worst = std::max (worst, std::fabs (v - previous));
			previous = v;
		}
		check (worst <= 0.011f, "no sample steps more than ~1% at " + num (rate));
		checkClose (previous, 1.0, 1e-3, "reaches the target within a second at " + num (rate));
	}
}

//------------------------------------------------------------------------
// 3. The output stage: unity is BIT-IDENTICAL, and half gain is not
//------------------------------------------------------------------------
static void testOutputStage ()
{
	std::printf ("output stage passes the signal through untouched at 0 dB\n");

	const int kBlock = 512;
	std::vector<float> reference (kBlock);
	for (int i = 0; i < kBlock; ++i)
		reference[i] = static_cast<float> (std::sin (i * 0.037) * 0.9 + ((i % 7) - 3) * 1e-6);
	reference[0] = 1.f; reference[1] = -1.f; reference[2] = 1e-7f;

	for (double rate : kRates)
	{
		SimpleSynthDsp dsp;
		dsp.setSampleRate (rate);
		dsp.setOutputLevelDb (0.0);
		dsp.reset ();

		std::vector<float> l (reference), r (reference);
		dsp.applyOutputLevel (l.data (), r.data (), kBlock);
		check (std::memcmp (l.data (), reference.data (), kBlock * sizeof (float)) == 0 &&
		       std::memcmp (r.data (), reference.data (), kBlock * sizeof (float)) == 0,
		       "bit-identical at 0 dB, " + num (rate));
	}

	// THE NEGATIVE CONTROL. The same comparison at half gain MUST fail -
	// a guard that has never failed is a guess.
	{
		SimpleSynthDsp dsp;
		dsp.setSampleRate (48000.0);
		dsp.setOutputLevelDb (gainToDb (0.5));
		dsp.reset ();
		std::vector<float> l (reference), r (reference);
		dsp.applyOutputLevel (l.data (), r.data (), kBlock);
		check (std::memcmp (l.data (), reference.data (), kBlock * sizeof (float)) != 0,
		       "NEGATIVE CONTROL: half gain is NOT bit-identical");
		checkClose (l[0], 0.5, 1e-6, "half gain really halves full scale");
		check (l[kBlock - 1] == r[kBlock - 1], "both channels get the identical gain");
	}
}

//------------------------------------------------------------------------
// 4. The VCOs: in tune, band-limited, the right shape
//------------------------------------------------------------------------
static void testVcos ()
{
	std::printf ("VCOs\n");

	for (double rate : { 44100.0, 96000.0 })
	{
		for (auto wave : { Vco::Wave::Saw, Vco::Wave::Square })
		{
			const char* name = (wave == Vco::Wave::Saw) ? "saw" : "square";
			for (double hz : { 55.0, 440.0, 3520.0 })
			{
				Vco v;
				v.setWave (wave);
				v.reset (0.0);
				std::vector<double> x (static_cast<size_t> (rate));
				double dc = 0.0, peak = 0.0;
				for (auto& s : x)
				{
					s = v.next (hz / rate);
					dc += s;
					peak = std::max (peak, std::fabs (s));
				}
				dc /= x.size ();

				checkClose (measureFrequency (x, rate), hz, hz * 1e-4,
				            std::string (name) + " at " + num (hz) + " Hz, " + num (rate));
				check (std::fabs (dc) < 0.01, std::string (name) + " has no DC at " + num (hz) + " Hz");
				// polyBLEP overshoots a little at the edges, never much.
				check (peak <= 1.12, std::string (name) + " peak " + num (peak, "%.3f") + " <= 1.12 at " + num (hz));
			}
		}
	}

	// BAND-LIMITING, measured: the first sample after a sawtooth's reset
	// is corrected - a naive saw would jump straight from +1 to -1.
	{
		Vco v;
		v.setWave (Vco::Wave::Saw);
		v.reset (0.0);
		const double dt = 1000.0 / 48000.0;
		double prev = v.next (dt), worstJump = 0.0;
		for (int i = 0; i < 4800; ++i)
		{
			const double s = v.next (dt);
			worstJump = std::max (worstJump, std::fabs (s - prev));
			prev = s;
		}
		check (worstJump < 1.95, "polyBLEP rounds the saw's reset (largest step " + num (worstJump, "%.3f") + ")");
	}

	// THE TUNE SPAN, through the whole engine: +12 st is exactly an
	// octave up, -12 exactly an octave down.
	for (double tune : { -12.0, 0.0, 12.0 })
	{
		SimpleSynthDsp dsp;
		dsp.setSampleRate (48000.0);
		applyDefaultPatch (dsp);
		dsp.setVcoLevel (1, 0.0);                        // VCO 1 alone
		dsp.setVcoWave (0, Vco::Wave::Square);
		dsp.setVcoTune (0, tune);
		dsp.setCutoffHz (20000.0);
		dsp.setResonance (0.0);
		dsp.setVcfEnvAmount (0.0);
		dsp.setVcaEnvelope ({ 0.0005, 0.001, 1.0, 0.1 });
		dsp.reset ();
		dsp.noteOn (57, 1.f);                            // A3, 220 Hz
		std::vector<float> l (48000), r (48000);
		dsp.render (l.data (), r.data (), 48000);
		std::vector<double> x (l.begin () + 4800, l.end ());
		const double want = 220.0 * std::exp2 (tune / 12.0);
		checkClose (measureFrequency (x, 48000.0), want, want * 2e-3,
		            "VCO 1 tune " + num (tune, "%+.0f") + " st plays " + num (want) + " Hz");
	}
}

//------------------------------------------------------------------------
// 5. The MS-20 LFO
//------------------------------------------------------------------------
static void testLfo ()
{
	std::printf ("LFO\n");

	// THE THREE NAMED SHAPES, sampled.
	checkClose (lfoShapeValue (0.0, -1.0),  1.0, 1e-12, "shape -1 starts at the top (falling saw)");
	checkClose (lfoShapeValue (0.5, -1.0),  0.0, 1e-12, "shape -1 is halfway down at half a cycle");
	checkClose (lfoShapeValue (0.99, -1.0), -0.98, 1e-9, "shape -1 ends at the bottom");
	checkClose (lfoShapeValue (0.0, 0.0),  -1.0, 1e-12, "shape 0 starts at the bottom (triangle)");
	checkClose (lfoShapeValue (0.25, 0.0),  0.0, 1e-12, "shape 0 crosses zero a quarter in");
	checkClose (lfoShapeValue (0.5, 0.0),   1.0, 1e-12, "shape 0 peaks halfway");
	checkClose (lfoShapeValue (0.75, 0.0),  0.0, 1e-12, "shape 0 is symmetrical");
	checkClose (lfoShapeValue (0.0, 1.0),  -1.0, 1e-12, "shape +1 starts at the bottom (rising saw)");
	checkClose (lfoShapeValue (0.5, 1.0),   0.0, 1e-12, "shape +1 is halfway up at half a cycle");

	// Monotonic within the cycle at the two ends - a sawtooth, not a
	// triangle with a tiny second slope.
	{
		bool falling = true, rising = true;
		double prevF = 2.0, prevR = -2.0;
		for (int i = 0; i < 1000; ++i)
		{
			const double p = i / 1000.0;
			const double f = lfoShapeValue (p, -1.0), r = lfoShapeValue (p, 1.0);
			falling = falling && (f < prevF);
			rising  = rising  && (r > prevR);
			prevF = f; prevR = r;
		}
		check (falling, "shape -1 falls throughout the cycle");
		check (rising,  "shape +1 rises throughout the cycle");
	}

	// Every shape stays in -1..+1 and is continuous across the cycle
	// except at the two sawtooth ends, where the reset IS the wave.
	for (double shape : { -0.9, -0.5, 0.0, 0.3, 0.9 })
	{
		double worst = 0.0, lo = 1.0, hi = -1.0, prev = lfoShapeValue (0.0, shape);
		for (int i = 1; i <= 10000; ++i)
		{
			const double v = lfoShapeValue ((i % 10000) / 10000.0, shape);
			worst = std::max (worst, std::fabs (v - prev));
			lo = std::min (lo, v); hi = std::max (hi, v);
			prev = v;
		}
		check (lo >= -1.0 && hi <= 1.0, "shape " + num (shape) + " stays within -1..+1");
		check (worst < 0.05, "shape " + num (shape) + " has no jump inside the sweep");
	}

	// THE SQUARE'S DUTY FOLLOWS THE SHAPE KNOB, as the MS-20's does.
	checkClose (lfoSquareDuty (0.0), 0.5, 1e-12, "square is 50% at the triangle");
	checkClose (lfoSquareDuty (0.5), 0.75, 1e-12, "square widens as the shape rises");
	checkClose (lfoSquareDuty (-1.0), kMinLfoDuty, 1e-12, "square never vanishes at the falling end");
	checkClose (lfoSquareDuty (1.0), 1.0 - kMinLfoDuty, 1e-12, "or at the rising end");

	// The running LFO: rate, output switch, slew.
	for (double rate : { 0.1, 4.0, 20.0 })
	{
		MsLfo lfo;
		lfo.setSampleRate (48000.0);
		lfo.setRate (rate);
		lfo.setShape (0.0);
		lfo.reset ();
		std::vector<double> x (static_cast<size_t> (48000.0 * std::max (2.0, 3.0 / rate)));
		for (auto& s : x) s = lfo.next ();
		checkClose (measureFrequency (x, 48000.0), rate, rate * 1e-3, "LFO runs at " + num (rate) + " Hz");
	}
	{
		MsLfo lfo;
		lfo.setSampleRate (48000.0);
		lfo.setRate (5.0);
		lfo.setShape (0.5);
		lfo.setSquareOutput (true);
		lfo.reset ();
		int high = 0, total = 48000;
		double lo = 1.0, hi = -1.0, worstStep = 0.0, prev = lfo.next ();
		for (int i = 0; i < total; ++i)
		{
			const double v = lfo.next ();
			if (v > 0.0) ++high;
			lo = std::min (lo, v); hi = std::max (hi, v);
			worstStep = std::max (worstStep, std::fabs (v - prev));
			prev = v;
		}
		checkClose (static_cast<double> (high) / total, 0.75, 0.01, "Square output: duty 75% at shape +0.5");
		check (lo < -0.99 && hi > 0.99, "Square output swings the full -1..+1");
		// THE SLEW: an edge of 2.0 spread over ~0.5 ms is well under a
		// tenth per sample at 48 k. Unslewed it would be 2.0 in one.
		check (worstStep < 0.1, "the square's edges are slewed (largest step " + num (worstStep, "%.3f") + ")");
	}
}

//------------------------------------------------------------------------
// 6. The Moog ladder: SpaceDub's arithmetic, tuned
//------------------------------------------------------------------------
static void testLadder ()
{
	std::printf ("Moog ladder\n");

	// AT ZERO RESONANCE IT IS SPACEDUB'S FILTER, TO THE BIT.
	{
		SpaceDubReference ref;
		MoogLadder ladder;
		bool identical = true;
		for (int i = 0; i < 20000; ++i)
		{
			const double x = std::sin (i * 0.0123) + 0.3 * ((i * 7919) % 13 - 6) / 6.0;
			const double coeff = 0.05 + 0.4 * (0.5 + 0.5 * std::sin (i * 0.001));
			identical = identical && (ladder.tick (x, coeff, 0.0) == ref.tick (x, coeff, 0.0));
		}
		check (identical, "at zero resonance the ladder is SpaceDub's DelayLine filter, bit for bit");

		// And with a quiet signal and real resonance, still - the
		// feedback limiter's knee is above anything a small signal
		// reaches.
		//
		// ONLY WITH -ffp-contract=off. SpaceDub's `x -= out4 * fb` can be
		// fused into one FMA; ours goes through limitFeedback() and
		// cannot, so on arm64 (the Mac, and its Linux VM) the two differ
		// in the last bit unless contraction is off. That is codegen, not
		// arithmetic - which is what this is here to check.
		SpaceDubReference ref2;
		MoogLadder ladder2;
		bool quietIdentical = true;
		for (int i = 0; i < 20000; ++i)
		{
			const double x = 0.05 * std::sin (i * 0.02);
			quietIdentical = quietIdentical && (ladder2.tick (x, 0.2, 2.5) == ref2.tick (x, 0.2, 2.5));
		}
		check (quietIdentical, "a quiet signal at resonance 2.5 is still SpaceDub's filter to the bit");

		// NEGATIVE CONTROL: a loud one at high resonance is NOT - the
		// limiter must actually be doing something.
		SpaceDubReference ref3;
		MoogLadder ladder3;
		bool loudIdentical = true;
		for (int i = 0; i < 20000; ++i)
		{
			const double x = 8.0 * std::sin (i * 0.02);
			loudIdentical = loudIdentical && (ladder3.tick (x, 0.2, 3.9) == ref3.tick (x, 0.2, 3.9));
		}
		check (!loudIdentical, "NEGATIVE CONTROL: a loud signal at resonance 3.9 is limited");
	}

	// DC GAIN IS UNITY at zero resonance.
	{
		MoogLadder ladder;
		double y = 0.0;
		for (int i = 0; i < 48000; ++i)
			y = ladder.tick (0.5, 0.1, 0.0);
		checkClose (y, 0.5, 1e-3, "DC passes at unity with no resonance");
	}

	// THE TUNING, MEASURED: ring the filter just under self-oscillation
	// from an impulse and count its frequency. The resonant peak must be
	// where the Cutoff says, at every rate.
	const LadderTuning& tuning = ladderTuning ();
	for (double rate : { 44100.0, 48000.0, 96000.0 })
	{
		double worstCents = 0.0;
		for (double hz : { 50.0, 110.0, 440.0, 1000.0, 3000.0, 8000.0, 0.95 * cutoffCeilingHz (rate) })
		{
			double coeff = 0.0, unity = 0.0;
			tuning.lookup (hz / rate, coeff, unity);
			MoogLadder ladder;
			const int n = static_cast<int> (rate * 2);
			std::vector<double> x (n);
			x[0] = ladder.tick (1.0, coeff, 0.999 * unity);
			for (int i = 1; i < n; ++i)
				x[i] = ladder.tick (0.0, coeff, 0.999 * unity);
			std::vector<double> tail (x.begin () + n / 4, x.end ());
			const double got = measureFrequency (tail, rate);
			worstCents = std::max (worstCents, std::fabs (1200.0 * std::log2 (got / hz)));
		}
		std::printf ("  worst tuning error at %.0f Hz: %.2f cents\n", rate, worstCents);
		check (worstCents < 5.0, "the resonant peak is within 5 cents of the Cutoff at " + num (rate));
	}

	// For the record: how far off SpaceDub's own mapping would have been.
	{
		const double rate = 48000.0, hz = 440.0;
		MoogLadder ladder;
		const double coeff = hz / (rate / 4.0);
		std::vector<double> x (96000);
		x[0] = ladder.tick (1e-3, coeff, 3.99);
		for (size_t i = 1; i < x.size (); ++i)
			x[i] = ladder.tick (0.0, coeff, 3.99);
		std::vector<double> tail (x.begin () + 24000, x.end ());
		std::printf ("  (SpaceDub's Hz/(fs/4) mapping puts a 440 Hz request at %.1f Hz)\n",
		             measureFrequency (tail, rate));
	}

	// THE THRESHOLD, MEASURED: at loop gain 0.97 the ring dies; at 1.03
	// it grows - at every cutoff. That is the promise the lamp makes.
	for (double hz : { 100.0, 1000.0, 8000.0 })
	{
		double coeff = 0.0, unity = 0.0;
		tuning.lookup (hz / 48000.0, coeff, unity);
		// From a small impulse: below 1 the ring dies away; above 1 it
		// grows until the limiter holds it. (The growth is slow low
		// down - a hundred cycles of 100 Hz is a second - hence four.)
		auto ring = [&] (double k, double& early, double& late) {
			MoogLadder ladder;
			ladder.tick (1e-3, coeff, k * unity);
			early = late = 0.0;
			for (int i = 0; i < 192000; ++i)
			{
				const double y = std::fabs (ladder.tick (0.0, coeff, k * unity));
				if (i > 4800 && i < 9600) early = std::max (early, y);
				if (i > 182000) late = std::max (late, y);
			}
		};
		double e = 0.0, l = 0.0;
		ring (0.97, e, l);
		check (l < 0.01 * e, "loop gain 0.97 dies away at " + num (hz) + " Hz");
		ring (1.03, e, l);
		check (l >= 0.99 * e && l > 0.01, "loop gain 1.03 sustains at " + num (hz) + " Hz (" + num (l, "%.3f") + ")");
	}

	// SELF-OSCILLATION HAS AN AMPLITUDE, set by the limiter, and it is
	// bounded - not SpaceDub's clamp at 16.
	{
		double coeff = 0.0, unity = 0.0;
		tuning.lookup (1000.0 / 48000.0, coeff, unity);
		MoogLadder ladder;
		ladder.tick (1e-6, coeff, kMaxResonanceK * unity);
		double peak = 0.0;
		for (int i = 0; i < 480000; ++i)
		{
			const double y = ladder.tick (0.0, coeff, kMaxResonanceK * unity);
			if (i > 240000) peak = std::max (peak, std::fabs (y));
		}
		std::printf ("  self-oscillation amplitude at full resonance, 1 kHz: %.3f (%.1f dBFS before the VCA)\n",
		             peak, 20.0 * std::log10 (peak));
		check (peak > 0.1 && peak < 2.0, "self-oscillation settles at a bounded amplitude");
	}
}

//------------------------------------------------------------------------
// 7. The ADSRs
//------------------------------------------------------------------------
static void testAdsr ()
{
	std::printf ("ADSR\n");

	const double rate = 48000.0;
	auto secondsUntil = [&] (Adsr& e, auto predicate, double limit) {
		for (long n = 0; n < static_cast<long> (limit * rate); ++n)
			if (predicate (e.next ()))
				return n / rate;
		return -1.0;
	};

	for (double attack : { 0.001, 0.05, 2.0 })
	{
		Adsr e;
		e.setSampleRate (rate);
		e.setSpec ({ attack, 0.1, 0.5, 0.1 });
		e.reset ();
		e.gateOn ();
		const double t = secondsUntil (e, [] (float v) { return v >= 1.f; }, attack * 3 + 0.01);
		checkClose (t, attack, attack * 0.01 + 2.0 / rate, "attack " + num (attack) + " s reaches the top on time");
	}

	for (double decay : { 0.01, 0.3, 3.0 })
	{
		Adsr e;
		e.setSampleRate (rate);
		e.setSpec ({ 0.0005, decay, 0.25, 0.1 });
		e.reset ();
		e.gateOn ();
		secondsUntil (e, [] (float v) { return v >= 1.f; }, 1.0);
		// -60 dB of the distance from 1.0 to the sustain.
		const double target = 0.25 + 0.75 * kReleaseTargetLevel;
		const double t = secondsUntil (e, [&] (float v) { return v <= target; }, decay * 3);
		checkClose (t, decay, decay * 0.01 + 2.0 / rate, "decay " + num (decay) + " s is the time to -60 dB of the way");
	}

	for (double release : { 0.01, 0.3, 3.0 })
	{
		Adsr e;
		e.setSampleRate (rate);
		e.setSpec ({ 0.0005, 0.001, 1.0, release });
		e.reset ();
		e.gateOn ();
		secondsUntil (e, [] (float v) { return v >= 1.f; }, 1.0);
		e.gateOff ();
		const double t = secondsUntil (e, [] (float v) { return v <= kReleaseTargetLevel; }, release * 3);
		checkClose (t, release, release * 0.01 + 2.0 / rate, "release " + num (release) + " s reaches -60 dB on time");

		const double idle = secondsUntil (e, [&] (float) { return !e.active (); }, release * 3);
		checkClose (t + idle, releaseTailSeconds (release), release * 0.02 + 2.0 / rate,
		            "and goes idle when releaseTailSeconds says, " + num (release) + " s");
	}

	// SUSTAIN IS TRACKED: moving it during a held note moves the level.
	{
		Adsr e;
		e.setSampleRate (rate);
		e.setSpec ({ 0.0005, 0.01, 0.8, 0.1 });
		e.reset ();
		e.gateOn ();
		for (int i = 0; i < 4800; ++i) e.next ();
		checkClose (e.level (), 0.8, 1e-3, "settles on the sustain");
		e.setSpec ({ 0.0005, 0.01, 0.3, 0.1 });
		for (int i = 0; i < 4800; ++i) e.next ();
		checkClose (e.level (), 0.3, 1e-3, "follows the sustain knob while held");
	}

	// RETRIGGER FROM WHERE IT IS: no step back to zero, and the attack
	// resumes from the current level.
	{
		Adsr e;
		e.setSampleRate (rate);
		e.setSpec ({ 0.02, 0.2, 0.5, 0.3 });
		e.reset ();
		e.gateOn ();
		for (int i = 0; i < 9600; ++i) e.next ();
		e.gateOff ();
		for (int i = 0; i < 2400; ++i) e.next ();
		const double before = e.level ();
		e.gateOn ();
		const double after = e.next ();
		check (after >= before && after - before < 0.01,
		       "a retrigger continues from " + num (before, "%.3f") + " rather than restarting at zero");
	}
}

//------------------------------------------------------------------------
// 8. Mono note handling
//------------------------------------------------------------------------
static void testNotes ()
{
	std::printf ("mono, last-note priority\n");

	SimpleSynthDsp dsp;
	dsp.setSampleRate (48000.0);
	applyDefaultPatch (dsp);
	dsp.reset ();

	check (dsp.currentNote () == -1 && !dsp.gateOpen (), "nothing held after a reset");

	dsp.noteOn (60, 1.f);
	dsp.noteOn (64, 1.f);
	dsp.noteOn (67, 1.f);
	check (dsp.currentNote () == 67, "the last key down sounds");

	dsp.noteOff (64);
	check (dsp.currentNote () == 67 && dsp.gateOpen (), "releasing a key that is not sounding changes nothing");

	dsp.noteOff (67);
	check (dsp.currentNote () == 60 && dsp.gateOpen (), "releasing the sounding key returns to the last one held");

	dsp.noteOff (60);
	check (!dsp.gateOpen (), "the gate closes only when no key is down");

	dsp.noteOff (61);
	check (!dsp.gateOpen (), "a note-off for a key never pressed is harmless");

	// Seventeen keys: the oldest is forgotten, not the newest.
	for (int p = 40; p < 57; ++p)
		dsp.noteOn (p, 1.f);
	for (int p = 56; p > 40; --p)
		dsp.noteOff (p);
	check (!dsp.gateOpen (), "the seventeenth key pushed the first out of the stack");

	// A key pressed twice is listed once.
	dsp.noteOn (50, 1.f);
	dsp.noteOn (50, 1.f);
	dsp.noteOff (50);
	check (!dsp.gateOpen (), "a doubled note-on is one key, released by one note-off");

	// THE RELEASE REALLY SOUNDS AND REALLY ENDS.
	dsp.reset ();
	dsp.noteOn (60, 1.f);
	std::vector<float> l (24000), r (24000);
	dsp.render (l.data (), r.data (), 24000);
	dsp.noteOff (60);
	dsp.render (l.data (), r.data (), 2400);
	check (peakOf (l) > 0.01, "the release is audible just after key-up");
	std::vector<float> tail (static_cast<size_t> (48000 * dsp.tailSeconds ()) + 480), tr (tail.size ());
	dsp.render (tail.data (), tr.data (), static_cast<int> (tail.size ()));
	check (!dsp.voiceActive (), "the voice is idle once tailSeconds has passed");
	check (peakOf (tail, tail.size () - 480) == 0.0, "and silent");
}

//------------------------------------------------------------------------
// 9. Modulation reaches the cutoff, and the VCA
//------------------------------------------------------------------------
static void testModulation ()
{
	std::printf ("modulation routing\n");

	// The VCF envelope: +3 octaves at the top of the attack.
	{
		SimpleSynthDsp dsp;
		dsp.setSampleRate (48000.0);
		applyDefaultPatch (dsp);
		dsp.setKeyTrack (0.0);
		dsp.setVcfEnvelope ({ 0.0005, 5.0, 1.0, 0.1 });
		dsp.reset ();
		dsp.noteOn (60, 1.f);
		std::vector<float> l (4800), r (4800);
		dsp.render (l.data (), r.data (), 4800);
		checkClose (dsp.lastCutoffHz (), 1200.0 * 8.0, 1200.0 * 8.0 * 0.01, "the VCF envelope adds its 3 octaves");
	}

	// Key tracking at 100 %: an octave up the keyboard is an octave up
	// the cutoff.
	{
		SimpleSynthDsp dsp;
		dsp.setSampleRate (48000.0);
		applyDefaultPatch (dsp);
		dsp.setVcfEnvAmount (0.0);
		dsp.setKeyTrack (1.0);
		dsp.reset ();
		std::vector<float> l (480), r (480);
		dsp.noteOn (72, 1.f);
		dsp.render (l.data (), r.data (), 480);
		checkClose (dsp.lastCutoffHz (), 2400.0, 1.0, "full key tracking doubles the cutoff an octave up");
	}

	// The LFO on the cutoff: the cutoff swings its depth either way.
	{
		SimpleSynthDsp dsp;
		dsp.setSampleRate (48000.0);
		applyDefaultPatch (dsp);
		dsp.setVcfEnvAmount (0.0);
		dsp.setKeyTrack (0.0);
		dsp.setLfoCutoffDepth (1.0);
		dsp.setLfoRate (10.0);
		dsp.reset ();
		dsp.noteOn (60, 1.f);
		double lo = 1e9, hi = 0.0;
		float l = 0.f, r = 0.f;
		for (int i = 0; i < 48000; ++i)
		{
			dsp.render (&l, &r, 1);
			lo = std::min (lo, dsp.lastCutoffHz ());
			hi = std::max (hi, dsp.lastCutoffHz ());
		}
		checkClose (hi / lo, 4.0, 0.05, "one octave of LFO swings the cutoff an octave either way");
	}

	// The LFO on the VCA: at 100 % it reaches silence, and never lifts
	// the level above the envelope.
	{
		SimpleSynthDsp dsp;
		dsp.setSampleRate (48000.0);
		applyDefaultPatch (dsp);
		dsp.setLfoVcaDepth (1.0);
		dsp.setLfoRate (2.0);
		dsp.reset ();
		dsp.noteOn (48, 1.f);
		std::vector<float> l (48000), r (48000);
		dsp.render (l.data (), r.data (), 48000);
		// Envelope-follow in 5 ms windows.
		double quietest = 1e9, loudest = 0.0;
		for (int w = 4800; w + 240 <= 48000; w += 240)
		{
			float p = 0.f;
			for (int i = w; i < w + 240; ++i) p = std::max (p, std::fabs (l[i]));
			quietest = std::min (quietest, (double) p);
			loudest = std::max (loudest, (double) p);
		}
		check (quietest < 0.02 * loudest, "100% LFO on the VCA reaches near silence");

		SimpleSynthDsp plain;
		plain.setSampleRate (48000.0);
		applyDefaultPatch (plain);
		plain.reset ();
		plain.noteOn (48, 1.f);
		std::vector<float> pl (48000), pr (48000);
		plain.render (pl.data (), pr.data (), 48000);
		check (loudest <= peakOf (pl, 4800) * 1.001, "tremolo never lifts the level above the envelope");
	}

	// The LFO on the pitch: vibrato of 1 semitone moves the pitch a
	// semitone either way.
	{
		SimpleSynthDsp dsp;
		dsp.setSampleRate (48000.0);
		applyDefaultPatch (dsp);
		dsp.setVcoLevel (1, 0.0);
		dsp.setVcoWave (0, Vco::Wave::Square);
		dsp.setCutoffHz (20000.0);
		dsp.setResonance (0.0);
		dsp.setVcfEnvAmount (0.0);
		dsp.setLfoPitchDepth (1.0);
		dsp.setLfoSquareOutput (true);        // two pitches, easy to measure
		dsp.setLfoRate (0.5);
		dsp.reset ();
		dsp.noteOn (69, 1.f);
		std::vector<float> l (48000), r (48000);
		dsp.render (l.data (), r.data (), 48000);   // first second: +1 st
		std::vector<double> up (l.begin () + 4800, l.begin () + 43000);
		dsp.render (l.data (), r.data (), 48000);   // second: -1 st
		std::vector<double> down (l.begin () + 4800, l.begin () + 43000);
		checkClose (measureFrequency (up, 48000.0),   440.0 * std::exp2 (1.0 / 12.0), 1.0, "vibrato +1 st");
		checkClose (measureFrequency (down, 48000.0), 440.0 * std::exp2 (-1.0 / 12.0), 1.0, "vibrato -1 st");
	}
}

//------------------------------------------------------------------------
// 10. The displays' data
//------------------------------------------------------------------------
static void testTraces ()
{
	std::printf ("display traces\n");

	float vcf[200], vca[200];
	double gateOff = 0.0;
	const double span = traceEnvelopes ({ 0.005, 0.3, 0.3, 0.3 }, { 0.003, 0.5, 0.8, 0.25 },
	                                    vcf, vca, 200, &gateOff);
	check (span > gateOff && gateOff > 0.5, "the key is held long enough for both envelopes to settle");
	checkClose (span, gateOff + 0.3, 1e-9, "the axis runs on for the longer release");
	const int held = static_cast<int> (gateOff / span * 199) - 2;
	checkClose (vca[held], 0.8, 0.02, "the VCA trace sits on its sustain before key-up");
	checkClose (vcf[held], 0.3, 0.02, "the VCF trace sits on its sustain before key-up");
	check (vca[199] < 0.01 && vcf[199] < 0.01, "both reach the floor by the end of the axis");

	float shape[200], square[200];
	traceLfo (0.0, shape, square, 200, 2.0);
	checkClose (shape[0], -1.0, 1e-6, "the LFO trace starts at the bottom of the triangle");
	checkClose (shape[50], 1.0, 0.03, "and peaks half a cycle in");
	check (square[10] == 1.f && square[60] == -1.f, "the square is high then low");
}

//------------------------------------------------------------------------
// 11. Degenerate blocks and absurd settings
//------------------------------------------------------------------------
static void testDegenerate ()
{
	std::printf ("zero frames, null buffers, absurd settings\n");

	SimpleSynthDsp dsp;
	dsp.setSampleRate (48000.0);
	applyDefaultPatch (dsp);
	dsp.reset ();
	dsp.noteOn (60, 1.f);

	std::vector<float> l (512, 0.f), r (512, 0.f);
	dsp.render (l.data (), r.data (), 0);
	dsp.render (l.data (), r.data (), -1);
	dsp.render (nullptr, r.data (), 512);
	dsp.render (l.data (), nullptr, 512);
	dsp.applyOutputLevel (nullptr, nullptr, 512);
	check (true, "survived zero, negative and null-buffer blocks");

	dsp.setSampleRate (0.0);
	check (dsp.sampleRate () >= 1000.0, "an absurd sample rate falls back to something sane");

	// EVERY CONTROL AT BOTH EXTREMES AT ONCE - the things most likely to
	// produce a NaN that then poisons every sample forever.
	for (int corner = 0; corner < 4; ++corner)
	{
		const bool hiEnd = (corner & 1) != 0, hiRate = (corner & 2) != 0;
		SimpleSynthDsp d;
		d.setSampleRate (hiRate ? 192000.0 : 22050.0);
		applyDefaultPatch (d);
		for (int v = 0; v < kNumVcos; ++v)
		{
			d.setVcoTune (v, hiEnd ? 12.0 : -12.0);
			d.setVcoLevel (v, 1.0);
			d.setVcoWave (v, hiEnd ? Vco::Wave::Square : Vco::Wave::Saw);
		}
		d.setCutoffHz (hiEnd ? 20000.0 : 20.0);
		d.setResonance (kMaxResonanceK);
		d.setVcfEnvAmount (hiEnd ? kMaxEnvOctaves : -kMaxEnvOctaves);
		d.setLfoPitchDepth (kMaxLfoPitchSemis);
		d.setLfoCutoffDepth (kMaxLfoCutoffOctaves);
		d.setLfoVcaDepth (1.0);
		d.setLfoRate (20.0);
		d.setLfoShape (hiEnd ? 1.0 : -1.0);
		d.setKeyTrack (1.0);
		d.setOutputLevelDb (6.0);
		d.reset ();
		d.noteOn (hiEnd ? 127 : 0, 1.f);
		std::vector<float> a (48000), b (48000);
		d.render (a.data (), b.data (), 48000);
		bool finite = true;
		for (float s : a) finite = finite && std::isfinite (s);
		check (finite, "every control at an extreme stays finite (corner " + num (corner) + ")");
		check (peakOf (a) < 8.0, "and bounded (corner " + num (corner) + ", peak " + num (peakDbFS (a), "%.1f") + " dBFS)");
	}
}

//------------------------------------------------------------------------
// 12. The default patch's output level
//------------------------------------------------------------------------
static void measureDefaultLevel ()
{
	std::printf ("default patch level\n");

	double loudest = -1e9;
	for (double rate : { 44100.0, 48000.0, 96000.0 })
	{
		for (int note : { 36, 48, 60, 72, 84 })
		{
			SimpleSynthDsp dsp;
			dsp.setSampleRate (rate);
			applyDefaultPatch (dsp);
			dsp.reset ();
			dsp.noteOn (note, 1.f);
			std::vector<float> l (static_cast<size_t> (rate)), r (l.size ());
			dsp.render (l.data (), r.data (), static_cast<int> (l.size ()));
			loudest = std::max (loudest, peakDbFS (l));
		}
	}
	std::printf ("  measured peak, default patch, notes C2..C6 at 44.1/48/96 k: %.2f dBFS\n", loudest);

	// THE NUMBER TO INSIST ON: comfortably below 0 dBFS, and not so low
	// that the Output Level has to be pushed to hear it.
	check (loudest < -3.0, "the default patch peaks below -3 dBFS");
	check (loudest > -12.0, "and above -12 dBFS");

	// The worst case a user can dial in: three full squares in unison,
	// full resonance, +6 dB of Output Level.
	{
		SimpleSynthDsp dsp;
		dsp.setSampleRate (48000.0);
		applyDefaultPatch (dsp);
		for (int v = 0; v < kNumVcos; ++v)
		{
			dsp.setVcoWave (v, Vco::Wave::Square);
			dsp.setVcoLevel (v, 1.0);
			dsp.setVcoTune (v, 0.0);
		}
		dsp.setResonance (kMaxResonanceK);
		dsp.setCutoffHz (500.0);
		dsp.setVcfEnvAmount (kMaxEnvOctaves);
		dsp.setOutputLevelDb (6.0);
		double worst = -1e9;
		for (int note : { 24, 48, 72 })
		{
			dsp.reset ();
			dsp.noteOn (note, 1.f);
			std::vector<float> l (96000), r (96000);
			dsp.render (l.data (), r.data (), 96000);
			worst = std::max (worst, peakDbFS (l));
		}
		std::printf ("  worst case (3 squares, full resonance, +6 dB out): %.2f dBFS  -> %.2f dBFS at 0 dB out\n",
		             worst, worst - 6.0);
		check (worst - 6.0 < 1.0, "even the worst case is within 1 dB of full scale at 0 dB Output Level");
	}

	// Self-oscillation with every VCO off - the noise floor starts it.
	{
		SimpleSynthDsp dsp;
		dsp.setSampleRate (48000.0);
		applyDefaultPatch (dsp);
		for (int v = 0; v < kNumVcos; ++v)
			dsp.setVcoLevel (v, 0.0);
		dsp.setVcfEnvAmount (0.0);
		dsp.setResonance (kMaxResonanceK);
		dsp.reset ();
		dsp.noteOn (60, 1.f);
		std::vector<float> l (96000), r (96000);
		dsp.render (l.data (), r.data (), 96000);
		std::printf ("  self-oscillation alone, VCOs off: %.2f dBFS\n", peakDbFS (l, 48000));
		check (peakDbFS (l, 48000) > -20.0, "full resonance sings with every VCO off");

		dsp.setResonance (0.0);
		dsp.reset ();
		dsp.noteOn (60, 1.f);
		dsp.render (l.data (), r.data (), 96000);
		check (peakDbFS (l, 48000) < -90.0, "NEGATIVE CONTROL: without resonance, VCOs off is silence");
	}

	// AND AT THE HARD CORNER: a 30 Hz cutoff on a 192 k engine, where
	// SpaceDub's 1e-15 state floor zeroed the noise that starts it. See
	// kStateFloor in SimpleSynthDsp.cpp.
	{
		SimpleSynthDsp dsp;
		dsp.setSampleRate (192000.0);
		applyDefaultPatch (dsp);
		for (int v = 0; v < kNumVcos; ++v)
			dsp.setVcoLevel (v, 0.0);
		dsp.setVcfEnvAmount (0.0);
		dsp.setKeyTrack (0.0);
		dsp.setCutoffHz (30.0);
		dsp.setResonance (kMaxResonanceK);
		dsp.reset ();
		dsp.noteOn (60, 1.f);
		std::vector<float> l (192000 * 8), r (l.size ());
		dsp.render (l.data (), r.data (), static_cast<int> (l.size ()));
		std::printf ("  self-oscillation at 30 Hz on a 192 k engine, after 8 s: %.2f dBFS\n",
		             peakDbFS (l, l.size () - 192000));
		check (peakDbFS (l, l.size () - 192000) > -20.0, "full resonance sings at 30 Hz on a 192 k engine");
	}
}

//------------------------------------------------------------------------
int main ()
{
	std::printf ("SimpleSynth DSP tests\n");
	std::printf ("---------------------\n");

	testConversions ();
	testSmootherStep ();
	testOutputStage ();
	testVcos ();
	testLfo ();
	testLadder ();
	testAdsr ();
	testNotes ();
	testModulation ();
	testTraces ();
	testDegenerate ();
	measureDefaultLevel ();

	std::printf ("---------------------\n");
	std::printf ("%d checks, %d failures\n", gChecks, gFailures);

	return (gFailures == 0) ? 0 : 1;
}
