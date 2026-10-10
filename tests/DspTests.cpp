//------------------------------------------------------------------------
// R2Synth - DSP tests
//
// SDK-FREE, compiled directly against R2SynthDsp.cpp:
//
//   c++ -std=c++17 -O2 -ffp-contract=off -I../source DspTests.cpp
//       ../source/R2SynthDsp.cpp -o /tmp/dsptests  &&  /tmp/dsptests
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

#include "R2SynthDsp.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

using namespace R2Synth;

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
    step with R2SynthParams.cpp by hand, because this suite does not
    include SDK headers. If one moves, move the other. */
void applyDefaultPatch (R2SynthDsp& d)
{
	d.setOutputLevelDb (0.0);
	d.setVcoWave (0, Vco::Wave::Saw);    d.setVcoTune (0, 0.0);   d.setVcoLevel (0, 1.0);
	d.setVcoWave (1, Vco::Wave::Square); d.setVcoTune (1, -12.0); d.setVcoLevel (1, 0.5);
	d.setVcoWave (2, Vco::Wave::Saw);    d.setVcoTune (2, 0.0);   d.setVcoLevel (2, 0.0);
	d.setLfoRate (4.0);
	d.setLfoShape (0.0);
	d.setLfoOutput (LfoOutput::Shape);
	d.setLfoPitchDepth (0.0);
	d.setLfo2Rate (0.5);
	d.setLfo2Level (0.0);
	d.setCutoffHz (1200.0);
	d.setResonance (0.20 * kMaxResonanceK);
	d.setKeyTrack (0.5);
	d.setLfoCutoffDepth (0.0);
	d.setLfoResonanceDepth (0.0);
	d.setVcfEnvAmount (3.0);
	d.setVcfEnvelope ({ 0.005, 0.300, 0.30, 0.300 });
	d.setLfoVcaDepth (0.0);
	d.setVcaEnvelope ({ 0.003, 0.500, 0.80, 0.250 });
	d.setPortamento (kMinPortamentoSeconds);   // off
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
		R2SynthDsp dsp;
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
		R2SynthDsp dsp;
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
		R2SynthDsp dsp;
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
// 5a. The LFO's sine output - the VCS3-style shaper
//------------------------------------------------------------------------
namespace {

/** Total harmonic distortion of one cycle, by DFT: harmonics 2..40
    against the fundamental. */
double thdOfCycle (const std::vector<double>& y)
{
	const size_t n = y.size ();
	auto magnitude = [&] (int k) {
		double re = 0.0, im = 0.0;
		for (size_t i = 0; i < n; ++i)
		{
			const double a = 2.0 * 3.14159265358979323846 * k * static_cast<double> (i) / n;
			re += y[i] * std::cos (a);
			im -= y[i] * std::sin (a);
		}
		return std::sqrt (re * re + im * im);
	};
	double harmonics = 0.0;
	for (int k = 2; k <= 40; ++k)
		harmonics += magnitude (k) * magnitude (k);
	return std::sqrt (harmonics) / magnitude (1);
}

std::vector<double> oneCycle (LfoOutput which, double shape, int n = 4096)
{
	std::vector<double> y (n);
	for (int i = 0; i < n; ++i)
		y[i] = lfoOutputValue (which, static_cast<double> (i) / n, shape);
	return y;
}

} // anonymous namespace

static void testLfoSine ()
{
	std::printf ("LFO sine (VCS3-style shaper)\n");

	// THE DRIVE IS THE MEASURED OPTIMUM, and it makes a real sine: the
	// triangle it starts from is 12 % THD.
	const double triThd  = thdOfCycle (oneCycle (LfoOutput::Shape, 0.0));
	const double sineThd = thdOfCycle (oneCycle (LfoOutput::Sine, 0.0));
	std::printf ("  THD: triangle %.2f %%, sine %.2f %%\n", triThd * 100.0, sineThd * 100.0);
	check (triThd > 0.11, "the triangle it starts from really is a triangle");
	check (sineThd < 0.015, "the shaped sine is under 1.5 % THD");

	// And kSineDrive IS the optimum: a little either way is worse.
	auto thdAtDrive = [] (double k) {
		std::vector<double> y (4096);
		for (int i = 0; i < 4096; ++i)
			y[i] = std::tanh (k * lfoShapeValue (i / 4096.0, 0.0));
		return thdOfCycle (y);
	};
	check (thdAtDrive (kSineDrive - 0.1) > sineThd && thdAtDrive (kSineDrive + 0.1) > sineThd,
	       "kSineDrive is the least-distortion drive (1.3 and 1.5 are both worse)");

	// Centred, full-scale, odd-symmetric at Shape 0.
	{
		const auto y = oneCycle (LfoOutput::Sine, 0.0);
		double lo = 1.0, hi = -1.0, mean = 0.0, asym = 0.0;
		for (size_t i = 0; i < y.size (); ++i)
		{
			lo = std::min (lo, y[i]); hi = std::max (hi, y[i]); mean += y[i];
			asym = std::max (asym, std::fabs (y[i] + y[(i + y.size () / 2) % y.size ()]));
		}
		checkClose (lo, -1.0, 1e-9, "the sine reaches -1");
		checkClose (hi, 1.0, 1e-3, "and +1");
		checkClose (mean / y.size (), 0.0, 1e-9, "no DC at Shape 0");
		check (asym < 1e-9, "the second half-cycle is the first, inverted");
	}

	// THE BALANCE, VCS3-style: at either end of the Shape knob one
	// half-cycle broadens into a dome and the other sharpens into a V.
	// Measured as time spent near each extreme.
	for (double shape : { -1.0, 1.0 })
	{
		const auto y = oneCycle (LfoOutput::Sine, shape);
		double lo = 1.0, hi = -1.0;
		int nearTop = 0, nearBottom = 0;
		for (double v : y)
		{
			lo = std::min (lo, v); hi = std::max (hi, v);
			if (v > 0.8) ++nearTop;
			if (v < -0.8) ++nearBottom;
		}
		const bool domeOnTop = nearTop > 3 * nearBottom;
		const bool domeBelow = nearBottom > 3 * nearTop;
		check ((shape > 0.0) ? domeOnTop : domeBelow,
		       "Shape " + num (shape, "%+.0f") + " domes the " + std::string (shape > 0.0 ? "top" : "bottom") +
		       " half (" + std::to_string (nearTop) + " vs " + std::to_string (nearBottom) + " samples past 0.8)");
		check (lo >= -1.0 - 1e-9 && hi <= 1.0 + 1e-9 && hi - lo > 1.999,
		       "and still swings exactly -1..+1 at Shape " + num (shape, "%+.0f"));
	}

	// The balance moves smoothly: no position of the knob jumps the wave.
	{
		double worst = 0.0;
		for (int s = -100; s < 100; ++s)
			for (int i = 0; i < 64; ++i)
			{
				const double p = i / 64.0;
				worst = std::max (worst, std::fabs (lfoSineValue (p, (s + 1) / 100.0) - lfoSineValue (p, s / 100.0)));
			}
		check (worst < 0.05, "a 1 % move of the Shape knob moves the sine by under 0.05 (" + num (worst, "%.3f") + ")");
	}

	// The running LFO on the Sine output: in tune, bounded, continuous.
	{
		MsLfo lfo;
		lfo.setSampleRate (48000.0);
		lfo.setRate (3.0);
		lfo.setShape (0.0);
		lfo.setOutput (LfoOutput::Sine);
		lfo.reset ();
		std::vector<double> x (96000);
		double worstStep = 0.0, prev = lfo.next ();
		for (auto& s : x)
		{
			s = lfo.next ();
			worstStep = std::max (worstStep, std::fabs (s - prev));
			prev = s;
		}
		checkClose (measureFrequency (x, 48000.0), 3.0, 3e-3, "Sine output runs at the LFO rate");
		check (worstStep < 0.001, "and has no steps (largest " + num (worstStep, "%.5f") + " at 3 Hz)");
	}

	// And through the engine: the Sine output on the cutoff reaches its
	// full depth either way.
	{
		R2SynthDsp dsp;
		dsp.setSampleRate (48000.0);
		applyDefaultPatch (dsp);
		dsp.setVcfEnvAmount (0.0);
		dsp.setKeyTrack (0.0);
		dsp.setLfoOutput (LfoOutput::Sine);
		dsp.setLfoCutoffDepth (1.0);
		dsp.setLfoRate (5.0);
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
		checkClose (hi / lo, 4.0, 0.05, "the Sine output swings the cutoff its full octave either way");
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
		lfo.setOutput (LfoOutput::Square);
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

	testLfoSine ();
}

//------------------------------------------------------------------------
// 5b. LFO 2 - a sine that turns LFO 1's Shape knob
//------------------------------------------------------------------------
static void testLfo2 ()
{
	std::printf ("LFO 2\n");

	// THE SWEEP ARITHMETIC, which the panel's line and the DSP share.
	for (double knob : { -1.0, -0.3, 0.0, 0.6, 1.0 })
		for (double lfo2 : { -1.0, -0.5, 0.0, 0.7, 1.0 })
			check (sweptLfoShape (knob, 0.0, lfo2) == knob,
			       "Level 0 leaves the Shape EXACTLY on the knob (" + num (knob) + ", lfo2 " + num (lfo2) + ")");
	checkClose (sweptLfoShape (0.0, 1.0, 1.0), kMaxLfo2ShapeSweep, 1e-15, "full Level from the centre reaches +sweep");
	checkClose (sweptLfoShape (0.0, 1.0, -1.0), -kMaxLfo2ShapeSweep, 1e-15, "and -sweep");
	checkClose (sweptLfoShape (0.0, 0.5, 1.0), 0.5 * kMaxLfo2ShapeSweep, 1e-15, "half Level, half the sweep");
	check (sweptLfoShape (0.6, 1.0, 1.0) == 1.0, "the sweep is clamped at the rising saw");
	check (sweptLfoShape (-0.6, 1.0, -1.0) == -1.0, "and at the falling saw");
	{
		double lo = 0.0, hi = 0.0;
		lfo2ShapeRange (0.6, 1.0, lo, hi);
		checkClose (lo, -0.4, 1e-12, "knob +0.6 at full Level sweeps down to -0.4");
		checkClose (hi, 1.0, 1e-12, "and up to the end stop");
	}

	// THE RUNNING SINE: rate, and that it IS a sine.
	for (double rate : { 0.5, 5.0, 20.0 })
	{
		SineLfo lfo;
		lfo.setSampleRate (48000.0);
		lfo.setRate (rate);
		lfo.reset ();
		std::vector<double> x (static_cast<size_t> (48000.0 * std::max (2.0, 3.0 / rate)));
		for (auto& s : x) s = lfo.next ();
		checkClose (measureFrequency (x, 48000.0), rate, rate * 1e-3, "LFO 2 runs at " + num (rate) + " Hz");
	}
	{
		SineLfo lfo;
		lfo.setSampleRate (44100.0);
		lfo.setRate (3.0);
		lfo.reset ();
		double worst = 0.0;
		for (int i = 0; i < 44100; ++i)
		{
			const double want = std::sin (2.0 * 3.14159265358979323846 * 3.0 * i / 44100.0);
			worst = std::max (worst, std::fabs (lfo.next () - want));
		}
		check (worst < 1e-9, "LFO 2 is a true sine (worst error " + num (worst, "%.2e") + ")");
	}

	// AT LEVEL 0 LFO 2 IS INAUDIBLE: the Shape is the knob on every
	// sample, and the audio is bit-identical whatever LFO 2's rate - so a
	// patch that ignores LFO 2 sounds exactly as it did before LFO 2.
	{
		auto render = [] (double lfo2Rate, std::vector<float>& out, bool& shapeOnKnob) {
			R2SynthDsp d;
			d.setSampleRate (48000.0);
			applyDefaultPatch (d);
			d.setLfoShape (0.3);
			d.setLfoOutput (LfoOutput::Square);
			d.setLfoCutoffDepth (2.0);
			d.setLfoPitchDepth (1.0);
			d.setLfo2Rate (lfo2Rate);
			d.setLfo2Level (0.0);
			d.reset ();
			d.noteOn (48, 1.f);
			out.assign (48000, 0.f);
			std::vector<float> r (out.size ());
			shapeOnKnob = true;
			for (size_t i = 0; i < out.size (); i += 480)
			{
				d.render (&out[i], &r[i], 480);
				shapeOnKnob = shapeOnKnob && (d.lastLfoShape () == 0.3);
			}
		};
		std::vector<float> a, b;
		bool knobA = false, knobB = false;
		render (0.5, a, knobA);
		render (13.0, b, knobB);
		check (knobA && knobB, "at Level 0 LFO 1 runs at the Shape knob exactly");
		check (a == b, "at Level 0 the output is bit-identical whatever LFO 2's rate");
	}

	// AT FULL LEVEL THE SHAPE SWEEPS - from the centre, end to end; from
	// +0.6, down to -0.4 and parked at the rising saw at the top.
	for (double knob : { 0.0, 0.6 })
	{
		R2SynthDsp d;
		d.setSampleRate (48000.0);
		applyDefaultPatch (d);
		d.setLfoShape (knob);
		d.setLfo2Rate (2.0);
		d.setLfo2Level (1.0);
		d.reset ();
		double lo = 2.0, hi = -2.0, worstStep = 0.0, prev = d.lastLfoShape ();
		int parked = 0;
		float l[1], r[1];
		const int n = 48000;
		for (int i = 0; i < n; ++i)
		{
			d.render (l, r, 1);
			const double s = d.lastLfoShape ();
			lo = std::min (lo, s); hi = std::max (hi, s);
			worstStep = std::max (worstStep, std::fabs (s - prev));
			if (s == 1.0) ++parked;
			prev = s;
		}
		double wantLo = 0.0, wantHi = 0.0;
		lfo2ShapeRange (knob, 1.0, wantLo, wantHi);
		checkClose (lo, wantLo, 1e-3, "knob " + num (knob) + ": the Shape reaches " + num (wantLo));
		checkClose (hi, wantHi, 1e-3, "knob " + num (knob) + ": and " + num (wantHi));
		// A 2 Hz sine of amplitude 1 moves at most 2 pi 2 / 48000 a sample.
		check (worstStep < 3e-4, "the sweep is smooth (largest step " + num (worstStep, "%.2e") + ")");
		if (knob > 0.0)
		{
			// sin > 0.4 for (pi - 2 asin 0.4) / 2 pi of the cycle = 36.9 %.
			checkClose (static_cast<double> (parked) / n, 0.369, 0.01,
			            "from +0.6 it dwells on the rising saw for 37 % of the cycle");
		}
	}

	// THE SWEEP REACHES THE SOUND: LFO 1's square, patched to the
	// cutoff, has its duty swept - so the filter spends a different
	// fraction of the time up in each half of LFO 2's cycle.
	{
		R2SynthDsp d;
		d.setSampleRate (48000.0);
		applyDefaultPatch (d);
		d.setLfoOutput (LfoOutput::Square);
		d.setLfoRate (20.0);
		d.setLfoShape (0.0);
		d.setLfoCutoffDepth (1.0);
		d.setVcfEnvAmount (0.0);
		d.setKeyTrack (0.0);
		d.setLfo2Rate (0.5);       // one cycle in 2 s: first second the rising half
		d.setLfo2Level (0.8);
		d.reset ();
		d.noteOn (60, 1.f);
		float l[1], r[1];
		int upFirst = 0, upSecond = 0;
		for (int i = 0; i < 96000; ++i)
		{
			d.render (l, r, 1);
			const bool up = d.lastCutoffHz () > 1200.0;
			(i < 48000 ? upFirst : upSecond) += up ? 1 : 0;
		}
		const double dutyFirst = upFirst / 48000.0, dutySecond = upSecond / 48000.0;
		std::printf ("  square duty over LFO 2's two halves: %.3f, %.3f\n", dutyFirst, dutySecond);
		// Mean of 0.5 (1 + 0.8 sin) over a half cycle: 0.5 +/- 0.8/pi.
		checkClose (dutyFirst, 0.5 + 0.8 / 3.14159265358979323846, 0.02, "the square is wider while LFO 2 is up");
		checkClose (dutySecond, 0.5 - 0.8 / 3.14159265358979323846, 0.02, "and narrower while it is down");
	}

	// THE LEVEL IS SMOOTHED: a jump from 0 to 100 % mid-note glides the
	// sweep in rather than jumping the Shape.
	{
		R2SynthDsp d;
		d.setSampleRate (48000.0);
		applyDefaultPatch (d);
		d.setLfo2Rate (20.0);
		d.reset ();
		float l[1], r[1];
		for (int i = 0; i < 600; ++i) d.render (l, r, 1);    // a quarter-cycle in: the sine near its top
		double prev = d.lastLfoShape (), worst = 0.0;
		d.setLfo2Level (1.0);
		for (int i = 0; i < 4800; ++i)
		{
			d.render (l, r, 1);
			worst = std::max (worst, std::fabs (d.lastLfoShape () - prev));
			prev = d.lastLfoShape ();
		}
		check (worst < 0.01, "a Level jump glides in (largest Shape step " + num (worst, "%.4f") + ")");
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
// 6b. The two MS-20 filters (VCF Type, 7 Oct 2026)
//
// MS-20 I is the early Korg35 filter (diode pair in the FORWARD path,
// damping 7/3); MS-20 II the later OTA filter (strings of three diodes in
// the FEEDBACK path, damping 2) - after Stinchcombe's study of the two.
//------------------------------------------------------------------------
namespace {

/** One MS-20 for the tests: which filter, its feedback scaling and
    damping, and its diode knee. */
struct Ms20Kind
{
	const char* name;
	bool   early;
	double damping;
	double knee;

	double feedback (double res) const { return early ? korg35Feedback (res) : ms20Feedback (res); }
};

const Ms20Kind kMs20Kinds[] = {
	{ "MS-20 I",  true,  kKorg35Damping, kMs20DiodeKnee },
	{ "MS-20 II", false, kOtaDamping,    kMs20LateDiodeKnee },
};

/** A fresh filter of the kind, run as a function of (input, g, k). */
struct AnyMs20
{
	bool early;
	Ms20Filter i;
	Ms20LateFilter ii;
	double tick (double x, double g, double k) { return early ? i.tick (x, g, k) : ii.tick (x, g, k); }
};

/** Steady-state amplitude and THD (harmonics 2..9) of the filter's
    response to a sine, over a whole second after a second's settling. */
void sineThrough (const Ms20Kind& kind, double amp, double hz, double res, double cutoff,
                  double& gainDb, double& thdPercent)
{
	const double kTwoPi = 2.0 * 3.14159265358979323846;
	const double fs = 48000.0;
	AnyMs20 f { kind.early, {}, {} };
	const double g = ms20Prewarp (cutoff, fs);
	const double k = kind.feedback (res);
	const int n = 48000;
	std::vector<double> y;
	y.reserve (n);
	for (int i = 0; i < 2 * n; ++i)
	{
		const double v = f.tick (amp * std::sin (kTwoPi * hz * i / fs), g, k);
		if (i >= n) y.push_back (v);
	}
	double fund = 0.0, harm = 0.0;
	for (int h = 1; h <= 9 && h * hz < 0.5 * fs; ++h)
	{
		double re = 0.0, im = 0.0;
		for (int i = 0; i < n; ++i)
		{
			re += y[i] * std::cos (kTwoPi * h * hz * (i + n) / fs);
			im += y[i] * std::sin (kTwoPi * h * hz * (i + n) / fs);
		}
		(h == 1 ? fund : harm) += re * re + im * im;
	}
	gainDb = 20.0 * std::log10 (2.0 * std::sqrt (fund) / n / amp);
	thdPercent = 100.0 * std::sqrt (harm / fund);
}

/** Self-oscillation at full resonance, 1 kHz: peak and THD. */
void selfOscillation (const Ms20Kind& kind, double& peak, double& thdPercent)
{
	const double kTwoPi = 2.0 * 3.14159265358979323846;
	const double fs = 48000.0;
	AnyMs20 f { kind.early, {}, {} };
	const double g = ms20Prewarp (1000.0, fs);
	const double k = kind.feedback (kMaxResonanceK);
	f.tick (1e-3, g, k);
	std::vector<double> y;
	for (int i = 0; i < 480000; ++i)
	{
		const double v = f.tick (0.0, g, k);
		if (i >= 432000) y.push_back (v);
	}
	int first = -1, last = -1, cycles = 0;
	for (size_t i = 1; i < y.size (); ++i)
		if (y[i - 1] < 0.0 && y[i] >= 0.0)
		{
			if (first < 0) first = static_cast<int> (i); else ++cycles;
			last = static_cast<int> (i);
		}
	peak = 0.0;
	thdPercent = 100.0;
	if (cycles < 1)
		return;
	const double f0 = cycles * fs / (last - first);
	double fund = 0.0, harm = 0.0;
	for (int h = 1; h <= 9; ++h)
	{
		double re = 0.0, im = 0.0;
		for (int i = first; i < last; ++i)
		{
			re += y[i] * std::cos (kTwoPi * h * f0 * (i - first) / fs);
			im += y[i] * std::sin (kTwoPi * h * f0 * (i - first) / fs);
			if (h == 1) peak = std::max (peak, std::fabs (y[i]));
		}
		(h == 1 ? fund : harm) += re * re + im * im;
	}
	thdPercent = 100.0 * std::sqrt (harm / fund);
}

} // anonymous namespace

static void testMs20 ()
{
	std::printf ("MS-20 filters\n");
	const double kTwoPi = 2.0 * 3.14159265358979323846;
	const double fs = 48000.0;

	for (const Ms20Kind& kind : kMs20Kinds)
	{
		const std::string nm = kind.name;

		// THE SCREEN'S CURVE AGAINST THE MEASURED FILTER: a quiet sine
		// (the diodes stay off), amplitude by correlation.
		for (double cutoff : { 300.0, 1200.0, 6000.0 })
			for (double res : { 0.0, 0.5, 0.9 })
				for (double ratio : { 0.25, 1.0, 3.0 })
				{
					const double hz = cutoff * ratio;
					double gainDb = 0.0, thd = 0.0;
					sineThrough (kind, 0.001, hz, res, cutoff, gainDb, thd);
					const double predictedDb = 20.0 * std::log10 (ms20ResponseGain (
						kTwoPi * hz / fs, ms20Prewarp (cutoff, fs), kind.feedback (res), kind.damping));
					char what[160];
					std::snprintf (what, sizeof (what),
					               "%s response at %.0f Hz, cutoff %.0f Hz, res %.1f: measured %.2f dB, predicted %.2f dB",
					               kind.name, hz, cutoff, res, gainDb, predictedDb);
					checkClose (gainDb, predictedDb, 0.25, what);
				}

		// NO BASS LOSS: unity at DC whatever the resonance. (MS-20 I's
		// diodes, though off, take 1.5e-4 of it.)
		for (double res : { 0.0, 0.5, 0.9 })
		{
			AnyMs20 f { kind.early, {}, {} };
			double y = 0.0;
			for (int i = 0; i < 48000; ++i)
				y = f.tick (0.001, ms20Prewarp (1000.0, fs), kind.feedback (res));
			checkClose (y / 0.001, 1.0, 1e-3, nm + " DC gain is unity at resonance " + num (res));
		}

		// THE THRESHOLD, ON THE SHARED SCALE: loop gain 0.97 dies, 1.03
		// sustains - at every cutoff - and it sings AT THE CUTOFF.
		for (double hz : { 100.0, 1000.0, 8000.0 })
		{
			const double g = ms20Prewarp (hz, fs);
			auto ring = [&] (double res, double& early, double& late, std::vector<double>* tail) {
				AnyMs20 f { kind.early, {}, {} };
				f.tick (1e-3, g, kind.feedback (res));
				early = late = 0.0;
				for (int i = 0; i < 192000; ++i)
				{
					const double y = f.tick (0.0, g, kind.feedback (res));
					// Short early window: at 8 kHz a 0.97 ring is gone in a
					// tenth of a second.
					if (i > 48 && i < 960) early = std::max (early, std::fabs (y));
					if (i > 182000) late = std::max (late, std::fabs (y));
					if (tail && i >= 144000) tail->push_back (y);
				}
			};
			double e = 0.0, l = 0.0;
			ring (0.97, e, l, nullptr);
			check (l < 0.01 * e, nm + " loop gain 0.97 dies away at " + num (hz) + " Hz");
			std::vector<double> tail;
			ring (1.03, e, l, &tail);
			check (l >= 0.99 * e && l > 0.01, nm + " loop gain 1.03 sustains at " + num (hz) + " Hz (" + num (l, "%.3f") + ")");
			const double f0 = measureFrequency (tail, fs);
			check (std::fabs (f0 / hz - 1.0) < 0.03, nm + " sings at its cutoff: " + num (f0, "%.1f") + " Hz for " + num (hz));
		}

		// SELF-OSCILLATION IS BOUNDED by the diodes.
		double peak = 0.0, thd = 0.0;
		selfOscillation (kind, peak, thd);
		std::printf ("  %s self-oscillation at full resonance, 1 kHz: peak %.3f, %.2f %% THD (before the trim)\n",
		             kind.name, peak, thd);
		check (peak > 0.1 && peak < 2.0 * kind.knee, nm + " self-oscillation settles at a bounded amplitude");

		// And a huge input cannot blow it up.
		{
			AnyMs20 loud { kind.early, {}, {} };
			bool finite = true;
			double worst = 0.0;
			for (int i = 0; i < 48000; ++i)
			{
				const double y = loud.tick ((i % 50 < 25) ? 100.0 : -100.0, ms20Prewarp (19000.0, fs),
				                            kind.feedback (kMaxResonanceK));
				finite = finite && std::isfinite (y);
				worst = std::max (worst, std::fabs (y));
			}
			check (finite && worst < 20.0, nm + " stays finite and bounded under a +40 dB square (" + num (worst, "%.2f") + ")");
		}
	}

	// THE CORNER: prewarped, so exactly on the knob at every rate. With
	// no resonance the OTA filter's coincident poles are -6 dB there; the
	// Korg35's extra damping (7/3) makes it 20 log (3/7) = -7.36 dB.
	for (double rate : kRates)
		for (double hz : { 100.0, 1000.0, 10000.0 })
		{
			const double w = kTwoPi * hz / rate, g = ms20Prewarp (hz, rate);
			checkClose (20.0 * std::log10 (ms20ResponseGain (w, g, 0.0, kOtaDamping)), -6.0206, 1e-6,
			            "MS-20 II is -6 dB at its cutoff, " + num (hz) + " Hz at " + num (rate));
			checkClose (20.0 * std::log10 (ms20ResponseGain (w, g, 0.0, kKorg35Damping)), 20.0 * std::log10 (3.0 / 7.0), 1e-6,
			            "MS-20 I is -7.36 dB at its cutoff, " + num (hz) + " Hz at " + num (rate));
		}

	// 12 dB AN OCTAVE, not the ladder's 24.
	{
		const double g = ms20Prewarp (500.0, 96000.0);
		const double a = ms20ResponseGain (kTwoPi * 4000.0 / 96000.0, g, 0.0);
		const double b = ms20ResponseGain (kTwoPi * 8000.0 / 96000.0, g, 0.0);
		const double slope = 20.0 * std::log10 (b / a);
		check (slope < -11.0 && slope > -13.5, "MS-20 falls 12 dB an octave (" + num (slope, "%.2f") + " dB)");
	}

	// WHERE THE DIODES ARE - Stinchcombe's listening test, measured. With
	// no resonance, MS-20 I (forward-path diodes) gets RICHER AS THE INPUT
	// GETS LOUDER; MS-20 II (feedback-path diodes) STAYS LARGELY THE SAME.
	{
		double gq = 0.0, tq = 0.0, gl = 0.0, tl = 0.0, g2 = 0.0, t2 = 0.0;
		sineThrough (kMs20Kinds[0], 0.1, 250.0, 0.0, 1000.0, gq, tq);
		sineThrough (kMs20Kinds[0], 1.5, 250.0, 0.0, 1000.0, gl, tl);
		sineThrough (kMs20Kinds[1], 1.5, 250.0, 0.0, 1000.0, g2, t2);
		std::printf ("  250 Hz sine, no resonance: MS-20 I %.2f %% THD quiet, %.2f %% loud; MS-20 II %.2f %% loud\n",
		             tq, tl, t2);
		check (tq < 0.1, "MS-20 I is clean for a quiet signal");
		check (tl > 5.0 && gl < gq - 0.5, "MS-20 I distorts and compresses a loud one, with no resonance");
		check (t2 < 1.0, "MS-20 II passes the same loud signal nearly clean");
	}

	// MS-20 II's diodes TAME ITS PEAK: a loud sine at the resonant peak
	// gets far less than the linear gain.
	{
		const Ms20Kind& ii = kMs20Kinds[1];
		double gainDb = 0.0, thd = 0.0;
		sineThrough (ii, 2.0, 1000.0, 0.95, 1000.0, gainDb, thd);
		const double linearDb = 20.0 * std::log10 (ms20ResponseGain (kTwoPi * 1000.0 / fs, ms20Prewarp (1000.0, fs),
		                                                              ii.feedback (0.95), ii.damping));
		std::printf ("  MS-20 II, loud sine at the resonant peak (res 0.95): %.1f dB, linear %.1f dB\n", gainDb, linearDb);
		check (gainDb < linearDb - 6.0, "MS-20 II's feedback diodes tame a loud resonant peak");
	}

	// THE DIODE LAW: off below the knee, a sharp knee, then a slow
	// LOGARITHMIC climb with no ceiling - the shape a tanh does not have.
	for (double knee : { kMs20DiodeKnee, kMs20LateDiodeKnee })
	{
		const std::string at = " (knee " + num (knee) + ")";
		checkClose (ms20DiodeClip (1e-3 * knee, knee) / (1e-3 * knee), 1.0, 2e-4,
		            "the diodes are off for a quiet signal" + at);
		checkClose (ms20DiodeClip (0.5 * knee, knee) / (0.5 * knee), 1.0, 5e-3,
		            "and still nearly off at half the knee (within 0.5 %)" + at);
		// By construction, at the knee the diodes take as much as the
		// resistor: twice the knee in gives the knee out.
		checkClose (ms20DiodeClip (2.0 * knee, knee) / knee, 1.0, 1e-9, "twice the knee in gives the knee out" + at);
		const double at10 = ms20DiodeClip (10.0 * knee, knee);
		const double at100 = ms20DiodeClip (100.0 * knee, knee);
		const double at1000 = ms20DiodeClip (1000.0 * knee, knee);
		check (at100 > at10 && at10 > knee && at100 < 1.5 * knee,
		       "beyond it the voltage still climbs, slowly: 10x " + num (at10 / knee, "%.3f") +
		       ", 100x " + num (at100 / knee, "%.3f") + " knees" + at);
		check (std::fabs ((at100 - at10) - (at1000 - at100)) < 0.02 * knee,
		       "each further decade of drive adds the same voltage (the exponential diode law)" + at);
		check (ms20DiodeClip (-3.7, knee) == -ms20DiodeClip (3.7, knee) && ms20DiodeClip (0.0, knee) == 0.0,
		       "the diodes are symmetric" + at);

		// THE LOOP SOLVE: the residual is nil wherever a filter can ask.
		const double vd  = knee / kMs20DiodeSharpness;
		const double eps = knee / std::sinh (kMs20DiodeSharpness);
		double worstResidual = 0.0;
		for (double a : { -1e6, -50.0, -3.1, -0.2, 1e-9, 0.7, 2.9, 3.3, 40.0, 1e6 })
			for (double b : { 0.0, 0.1, 0.3, 0.55 })
			{
				const double v = ms20DiodeSolve (a, b, knee);
				const double residual = std::fabs ((1.0 - b) * v + eps * std::sinh (v / vd) - a) / (1.0 + std::fabs (a));
				worstResidual = std::max (worstResidual, std::isfinite (v) ? residual : 1.0);
			}
		check (worstResidual < 1e-9, "ms20DiodeSolve solves the loop equation (worst residual " + num (worstResidual) + ")" + at);
		check (ms20DiodeSolve (std::numeric_limits<double>::quiet_NaN (), 0.2, knee) == 0.0, "and survives a NaN" + at);
	}

	// THE INPUT OTA (MS-20 II): slope 1 at rest, odd, bounded.
	checkClose (otaSaturate (1e-6) / 1e-6, 1.0, 1e-9, "otaSaturate has slope 1 at zero");
	check (otaSaturate (-2.0) == -otaSaturate (2.0) && std::fabs (otaSaturate (1e9)) <= 1.0 / kOtaDrive + 1e-12,
	       "otaSaturate is odd and bounded");

	// THE TRACES: the same clamp as process(), flat passband with
	// resonance (no bass loss), the peak at the cutoff - and MS-20 I's
	// extra damping shows at the cutoff with no resonance.
	{
		const int n = 64;
		for (FilterType type : { FilterType::Ms20, FilterType::Ms20Late })
		{
			float db[n];
			traceFilterResponse (type, 1200.0, 0.9, fs, 20.0, 20000.0, db, n);
			const std::string nm = (type == FilterType::Ms20) ? "MS-20 I" : "MS-20 II";
			check (std::fabs (db[0]) < 0.1, nm + " trace is 0 dB at 20 Hz even at resonance 0.9");
			int top = 0;
			for (int i = 1; i < n; ++i)
				if (db[i] > db[top])
					top = i;
			const double topHz = 20.0 * std::pow (1000.0, static_cast<double> (top) / (n - 1));
			check (topHz > 900.0 && topHz < 1600.0, nm + " trace peaks at the cutoff (" + num (topHz, "%.0f") + " Hz)");
			check (db[top] > 6.f, "and stands 6 dB proud");
		}

		float one, two;
		traceFilterResponse (FilterType::Ms20, 1200.0, 0.0, fs, 1200.0, 1200.0, &one, 1);
		traceFilterResponse (FilterType::Ms20Late, 1200.0, 0.0, fs, 1200.0, 1200.0, &two, 1);
		checkClose (two - one, 7.3595 - 6.0206, 0.01, "with no resonance MS-20 I is 1.34 dB more damped at the cutoff");

		float moog[n], viaType[n];
		traceFilterResponse (1200.0, 0.5, fs, 20.0, 20000.0, moog, n);
		traceFilterResponse (FilterType::Moog, 1200.0, 0.5, fs, 20.0, 20000.0, viaType, n);
		check (std::equal (moog, moog + n, viaType), "traceFilterResponse(Moog, ...) IS the ladder's trace");

		float nyq[4];
		traceFilterResponse (FilterType::Ms20, 1200.0, 0.0, 22050.0, 1000.0, 20000.0, nyq, 4);
		check (nyq[3] == -120.f, "MS-20 points above Nyquist read -120 dB");
	}
}

//------------------------------------------------------------------------
// 6c. The VCF Type switch in the voice
//------------------------------------------------------------------------
static void testFilterSwitch ()
{
	std::printf ("VCF Type switch\n");

	auto render = [] (R2SynthDsp& d, int samples) {
		std::vector<float> l (static_cast<size_t> (samples)), r (l.size ());
		d.render (l.data (), r.data (), samples);
		return l;
	};

	// MOOG IS THE DEFAULT and a Moog voice is the voice as it was: one
	// that never heard of the switch and one switched away and back
	// (and settled, and reset) render the same samples.
	{
		R2SynthDsp a, b;
		a.setSampleRate (48000.0);
		b.setSampleRate (48000.0);
		applyDefaultPatch (a);
		applyDefaultPatch (b);
		check (a.filterType () == FilterType::Moog && a.filterWeight (FilterType::Moog) == 1.0 &&
		       a.filterWeight (FilterType::Ms20) == 0.0 && a.filterWeight (FilterType::Ms20Late) == 0.0,
		       "a new voice is on the Moog ladder");
		b.setFilterType (FilterType::Ms20);
		render (b, 4800);
		b.setFilterType (FilterType::Ms20Late);
		render (b, 4800);
		b.setFilterType (FilterType::Moog);
		render (b, 4800);
		render (a, 14400);   // the same number of samples: the noise floor's LCG runs on
		a.reset ();
		b.reset ();
		a.noteOn (48, 1.f);
		b.noteOn (48, 1.f);
		check (render (a, 24000) == render (b, 24000), "Moog after a round trip through both MS-20s is bit-identical");
	}

	// THE CROSS-FADE: monotonic, kFilterCrossfadeSeconds long, landing
	// EXACTLY; a reset lands at once.
	for (double rate : kRates)
	{
		R2SynthDsp d;
		d.setSampleRate (rate);
		applyDefaultPatch (d);
		d.reset ();
		d.noteOn (60, 1.f);
		d.setFilterType (FilterType::Ms20);
		const int expect = static_cast<int> (std::lround (kFilterCrossfadeSeconds * rate));
		int steps = 0;
		bool monotonic = true;
		double last = d.filterWeight (FilterType::Ms20);
		std::vector<float> l (1), r (1);
		while (d.filterWeight (FilterType::Ms20) < 1.0 && steps < 10 * expect)
		{
			d.render (l.data (), r.data (), 1);
			monotonic = monotonic && d.filterWeight (FilterType::Ms20) >= last;
			last = d.filterWeight (FilterType::Ms20);
			++steps;
		}
		check (monotonic && d.filterWeight (FilterType::Ms20) == 1.0 && d.filterWeight (FilterType::Moog) == 0.0 &&
		       steps == expect,
		       "the switch fades in " + std::to_string (steps) + " samples (want " + std::to_string (expect) + ") at " + num (rate));

		// A SWITCH MID-FADE to the third filter: the weights still sum to
		// 1 on every sample, and all land exactly.
		d.setFilterType (FilterType::Moog);
		for (int i = 0; i < expect / 3; ++i)
			d.render (l.data (), r.data (), 1);
		d.setFilterType (FilterType::Ms20Late);
		bool sumsToOne = true;
		for (int i = 0; i < expect + 2; ++i)
		{
			const double sum = d.filterWeight (FilterType::Moog) + d.filterWeight (FilterType::Ms20) +
			                   d.filterWeight (FilterType::Ms20Late);
			sumsToOne = sumsToOne && std::fabs (sum - 1.0) < 1e-12;
			d.render (l.data (), r.data (), 1);
		}
		check (sumsToOne && d.filterWeight (FilterType::Ms20Late) == 1.0 &&
		       d.filterWeight (FilterType::Moog) == 0.0 && d.filterWeight (FilterType::Ms20) == 0.0,
		       "a switch mid-fade keeps the three weights summing to 1 and lands exactly at " + num (rate));

		d.setFilterType (FilterType::Moog);
		d.render (l.data (), r.data (), 1);
		d.setFilterType (FilterType::Ms20);   // turned round mid-fade
		d.reset ();
		check (d.filterWeight (FilterType::Ms20) == 1.0 && d.filterWeight (FilterType::Moog) == 0.0,
		       "reset() lands on the chosen filter at once");

		check (filterTypeFromInternal (0.0) == FilterType::Moog && filterTypeFromInternal (1.0) == FilterType::Ms20 &&
		       filterTypeFromInternal (2.0) == FilterType::Ms20Late && filterTypeFromInternal (7.0) == FilterType::Ms20Late &&
		       filterTypeFromInternal (-1.0) == FilterType::Moog, "filterTypeFromInternal rounds and clamps");
	}

	// NO CLICK: switching mid-note, the biggest sample-to-sample step
	// in the fade is no bigger than the sawtooth's own steps either side.
	{
		R2SynthDsp d;
		d.setSampleRate (48000.0);
		applyDefaultPatch (d);
		d.setVcfEnvAmount (0.0);
		d.reset ();
		d.noteOn (45, 1.f);
		const auto before = render (d, 24000);
		d.setFilterType (FilterType::Ms20);
		const auto during = render (d, 960);
		const auto after = render (d, 24000);
		auto maxStep = [] (const std::vector<float>& v, size_t from) {
			double m = 0.0;
			for (size_t i = from + 1; i < v.size (); ++i)
				m = std::max (m, static_cast<double> (std::fabs (v[i] - v[i - 1])));
			return m;
		};
		const double around = std::max (maxStep (before, 12000), maxStep (after, 12000));
		const double fade = std::max (maxStep (during, 0),
		                              static_cast<double> (std::fabs (during[0] - before.back ())));
		check (fade <= 1.1 * around, "switching mid-note does not click (step " + num (fade, "%.4f") +
		       " vs " + num (around, "%.4f") + ")");
	}

	// THE MS-20 VOICE SOUNDS DIFFERENT - brighter for the same knobs.
	{
		R2SynthDsp m, s;
		for (R2SynthDsp* d : { &m, &s })
		{
			d->setSampleRate (48000.0);
			applyDefaultPatch (*d);
			d->setVcfEnvAmount (0.0);
		}
		s.setFilterType (FilterType::Ms20);
		m.reset ();
		s.reset ();
		m.noteOn (48, 1.f);
		s.noteOn (48, 1.f);
		const auto a = render (m, 24000), b = render (s, 24000);
		check (a != b, "NEGATIVE CONTROL: the MS-20 voice is not the Moog voice");
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

	R2SynthDsp dsp;
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
// Portamento: every note slides from the last, overlapping or not
//------------------------------------------------------------------------
static void testPortamento ()
{
	std::printf ("portamento\n");

	const double fs = 48000.0;
	R2SynthDsp dsp;
	dsp.setSampleRate (fs);
	applyDefaultPatch (dsp);
	dsp.reset ();

	std::vector<float> l (96000), r (96000);
	auto run = [&] (int n) { dsp.render (l.data (), r.data (), n); };

	// ---- the predicate and the range --------------------------------
	check (!portamentoActive (kMinPortamentoSeconds), "the bottom of the knob is off");
	check (portamentoActive (0.002), "2 ms glides");

	// ---- OFF: an exact jump, as before portamento existed ------------
	dsp.noteOn (48, 1.f);
	run (100);
	dsp.noteOn (60, 1.f);
	run (1);
	check (dsp.currentPitch () == 60.0 && !dsp.gliding (), "off: the new note sounds at once");
	dsp.allNotesOff ();

	// ---- ON, the first note after a reset does NOT slide in ----------
	const double t = 0.100;   // 100 ms
	dsp.setPortamento (t);
	dsp.reset ();
	dsp.noteOn (48, 1.f);
	run (1);
	check (dsp.currentPitch () == 48.0 && !dsp.gliding (), "the first note after a reset starts in tune");

	// ---- OVERLAPPING (legato): 48 held, 60 struck --------------------
	const int n = static_cast<int> (t * fs);   // 4800 samples
	dsp.noteOn (60, 1.f);
	run (n / 2);
	checkClose (dsp.currentPitch (), 54.0, 0.01, "legato: half way through the time, half way in pitch (linear)");
	run (n / 2 - 1);
	check (dsp.gliding (), "still sliding one sample before the time is up");
	run (1);
	check (dsp.currentPitch () == 60.0 && !dsp.gliding (), "legato: arrives EXACTLY on the note, in the knob's time");

	// ---- DETACHED: every key released before the next is struck ------
	dsp.noteOff (48);                  // the held one first, so 60 never falls back
	dsp.noteOff (60);
	check (!dsp.gateOpen (), "no key down");
	run (2400);
	check (dsp.currentPitch () == 60.0, "the pitch is held after key-up");
	dsp.noteOn (72, 1.f);
	run (n / 4);
	checkClose (dsp.currentPitch (), 63.0, 0.01, "detached: a note struck after the last was RELEASED still slides");
	run (n);
	check (dsp.currentPitch () == 72.0, "and arrives");

	// ---- THE TIME IS THE TIME, whatever the interval -----------------
	dsp.noteOn (73, 1.f);
	run (n / 2);
	checkClose (dsp.currentPitch (), 72.5, 0.001, "a semitone takes the same time as an octave");
	run (n);
	check (dsp.currentPitch () == 73.0, "the semitone arrives");

	// ---- A NEW NOTE MID-SLIDE starts from where the pitch IS ----------
	dsp.noteOn (61, 1.f);              // 73 -> 61
	run (n / 2);                       // at 67
	const double mid = dsp.currentPitch ();
	checkClose (mid, 67.0, 0.01, "mid-slide at 67");
	dsp.noteOn (79, 1.f);
	run (1);
	check (std::fabs (dsp.currentPitch () - mid) < 0.01, "a note struck mid-slide does not jump");
	run (n / 2 - 1);
	checkClose (dsp.currentPitch (), (mid + 79.0) / 2.0, 0.01, "and slides from there in a full time");
	run (n);

	// ---- RELEASING the sounding key slides back to the one held ------
	dsp.allNotesOff ();
	dsp.noteOn (50, 1.f);
	run (n + 1);
	dsp.noteOn (62, 1.f);
	run (n + 1);
	dsp.noteOff (62);
	check (dsp.currentNote () == 50, "back to the held key");
	run (n / 2);
	checkClose (dsp.currentPitch (), 56.0, 0.01, "and the return slides too");
	run (n);
	check (dsp.currentPitch () == 50.0, "and arrives");

	// ---- SAMPLE-RATE INDEPENDENT --------------------------------------
	for (double rate : { 44100.0, 96000.0 })
	{
		R2SynthDsp d;
		d.setSampleRate (rate);
		applyDefaultPatch (d);
		d.setPortamento (t);
		d.reset ();
		d.noteOn (40, 1.f);
		std::vector<float> a (20000), b (20000);
		d.render (a.data (), b.data (), 1);
		d.noteOn (52, 1.f);
		const int half = static_cast<int> (std::lround (t * rate / 2.0));
		d.render (a.data (), b.data (), half);
		checkClose (d.currentPitch (), 46.0, 0.02, "half the time is half the slide at " + std::to_string (static_cast<int> (rate)));
	}

	// ---- THE VCOs REALLY SLIDE: the sound's period follows the pitch --
	{
		R2SynthDsp d;
		d.setSampleRate (fs);
		applyDefaultPatch (d);
		d.setVcoLevel (1, 0.0);                 // VCO 1 alone, a sawtooth
		d.setCutoffHz (20000.0);
		d.setResonance (0.0);
		d.setVcfEnvAmount (0.0);
		d.setPortamento (1.0);                  // a slow one: 1 s
		d.reset ();
		d.noteOn (45, 1.f);                     // A2, 110 Hz
		std::vector<float> a (48000), b (48000);
		d.render (a.data (), b.data (), 24000);
		d.noteOn (57, 1.f);                     // A3, 220 Hz
		d.render (a.data (), b.data (), 48000);
		// Rising zero crossings (the saw's reset is a falling edge, so
		// count downward crossings) in 50 ms windows at the start, the
		// middle and the end of the slide.
		auto hzAt = [&] (int start) {
			int crossings = 0;
			for (int i = start + 1; i < start + 2400; ++i)
				if (a[i - 1] > 0.f && a[i] <= 0.f) ++crossings;
			return crossings / 0.05;
		};
		const double h0 = hzAt (0), h1 = hzAt (22800), h2 = hzAt (45600);
		check (h0 < 130.0 && h1 > 140.0 && h1 < 175.0 && h2 > 200.0,
		       "the audio glides 110 -> ~155 -> 220 Hz (" + std::to_string (h0) + ", " +
		       std::to_string (h1) + ", " + std::to_string (h2) + ")");
	}
}

//------------------------------------------------------------------------
// 9. Modulation reaches the cutoff, and the VCA
//------------------------------------------------------------------------
static void testModulation ()
{
	std::printf ("modulation routing\n");

	// The VCF envelope: +3 octaves at the top of the attack.
	{
		R2SynthDsp dsp;
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
		R2SynthDsp dsp;
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
		R2SynthDsp dsp;
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
		R2SynthDsp dsp;
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

		R2SynthDsp plain;
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
		R2SynthDsp dsp;
		dsp.setSampleRate (48000.0);
		applyDefaultPatch (dsp);
		dsp.setVcoLevel (1, 0.0);
		dsp.setVcoWave (0, Vco::Wave::Square);
		dsp.setCutoffHz (20000.0);
		dsp.setResonance (0.0);
		dsp.setVcfEnvAmount (0.0);
		dsp.setLfoPitchDepth (1.0);
		dsp.setLfoOutput (LfoOutput::Square); // two pitches, easy to measure
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

	float shape[200], square[200], sine[200];
	traceLfo (LfoOutput::Shape, 0.0, shape, 200, 2.0);
	traceLfo (LfoOutput::Square, 0.0, square, 200, 2.0);
	traceLfo (LfoOutput::Sine, 0.0, sine, 200, 2.0);
	checkClose (sine[0], -1.0, 1e-6, "the sine trace starts at the bottom, like the triangle it is made from");
	checkClose (sine[50], 1.0, 0.03, "and peaks where the triangle does");
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

	R2SynthDsp dsp;
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
		R2SynthDsp d;
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
			R2SynthDsp dsp;
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

	// THE MS-20s AT THE SAME PATCH: under full scale like the ladder, and
	// within 1.5 dB RMS of it - kMs20OutputGain's and kMs20LateOutputGain's
	// measurement.
	{
		double peak[3] = { -1e9, -1e9, -1e9 }, rms[3] = { 0.0, 0.0, 0.0 };
		for (int t = 0; t < 3; ++t)
			for (int note : { 36, 48, 60, 72, 84 })
			{
				R2SynthDsp dsp;
				dsp.setSampleRate (48000.0);
				applyDefaultPatch (dsp);
				dsp.setFilterType (static_cast<FilterType> (t));
				dsp.reset ();
				dsp.noteOn (note, 1.f);
				std::vector<float> l (48000), r (48000);
				dsp.render (l.data (), r.data (), 48000);
				double sum = 0.0;
				for (float x : l) sum += static_cast<double> (x) * x;
				rms[t] += 10.0 * std::log10 (sum / l.size ()) / 5.0;
				peak[t] = std::max (peak[t], peakDbFS (l));
			}
		for (int t = 1; t < 3; ++t)
		{
			const std::string name = (t == 1) ? "MS-20 I" : "MS-20 II";
			std::printf ("  %s, default patch: peak %.2f dBFS, RMS %.2f dB (ladder %.2f dB)\n",
			             name.c_str (), peak[t], rms[t], rms[0]);
			check (peak[t] < -3.0 && peak[t] > -12.0, "the " + name + " default patch peaks between -12 and -3 dBFS");
			check (std::fabs (rms[t] - rms[0]) < 1.5, "and within 1.5 dB RMS of the ladder");
		}
	}

	// The worst case a user can dial in: three full squares in unison,
	// full resonance, +6 dB of Output Level.
	{
		R2SynthDsp dsp;
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
		for (FilterType type : { FilterType::Moog, FilterType::Ms20, FilterType::Ms20Late })
			for (int note : { 24, 48, 72 })
			{
				dsp.setFilterType (type);
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
		R2SynthDsp dsp;
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

		dsp.setFilterType (FilterType::Ms20);
		dsp.reset ();
		dsp.noteOn (60, 1.f);
		dsp.render (l.data (), r.data (), 96000);
		std::printf ("  MS-20 self-oscillation alone, VCOs off: %.2f dBFS\n", peakDbFS (l, 48000));
		check (peakDbFS (l, 48000) > -20.0, "the MS-20 at full resonance sings with every VCO off");

		dsp.setFilterType (FilterType::Ms20Late);
		dsp.reset ();
		dsp.noteOn (60, 1.f);
		dsp.render (l.data (), r.data (), 96000);
		std::printf ("  MS-20 II self-oscillation alone, VCOs off: %.2f dBFS\n", peakDbFS (l, 48000));
		check (peakDbFS (l, 48000) > -20.0, "the MS-20 II at full resonance sings with every VCO off");
		dsp.setFilterType (FilterType::Moog);

		dsp.setResonance (0.0);
		dsp.reset ();
		dsp.noteOn (60, 1.f);
		dsp.render (l.data (), r.data (), 96000);
		check (peakDbFS (l, 48000) < -90.0, "NEGATIVE CONTROL: without resonance, VCOs off is silence");
	}

	// AND AT THE HARD CORNER: a 30 Hz cutoff on a 192 k engine, where
	// SpaceDub's 1e-15 state floor zeroed the noise that starts it. See
	// kStateFloor in R2SynthDsp.cpp.
	{
		R2SynthDsp dsp;
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
// 5c. LFO 1 -> resonance
//------------------------------------------------------------------------
static void testLfoResonance ()
{
	std::printf ("LFO 1 -> resonance\n");

	// THE ARITHMETIC the panel's line and the DSP share.
	for (double knob : { 0.0, 0.22, 0.9, kMaxResonanceK })
		for (double lfo : { -1.0, -0.4, 0.0, 0.8, 1.0 })
			check (lfoResonance (knob, 0.0, lfo) == knob,
			       "depth 0 leaves the resonance EXACTLY on the knob (" + num (knob) + ", lfo " + num (lfo) + ")");
	checkClose (lfoResonance (0.5, 0.2, 1.0), 0.7, 1e-15, "bipolar: up by the depth on the LFO's peak");
	checkClose (lfoResonance (0.5, 0.2, -1.0), 0.3, 1e-15, "and down by it on the trough");
	check (lfoResonance (0.9, kMaxLfoResonanceK, 1.0) == kMaxResonanceK, "clamped at the knob's top");
	check (lfoResonance (0.1, kMaxLfoResonanceK, -1.0) == 0.0, "and at zero - never negative feedback");
	{
		double lo = 0.0, hi = 0.0;
		lfoResonanceRange (0.22, 0.5, lo, hi);
		checkClose (lo, 0.0, 1e-15, "knob 0.22 at depth 0.5 reaches down to 0");
		checkClose (hi, 0.72, 1e-12, "and up to 0.72");
	}

	auto setup = [] (R2SynthDsp& d, double depth) {
		d.setSampleRate (48000.0);
		applyDefaultPatch (d);
		d.setResonance (0.5);
		d.setLfoRate (5.0);
		d.setLfoOutput (LfoOutput::Sine);
		d.setLfoResonanceDepth (depth);
		d.reset ();
		d.noteOn (48, 1.f);
	};

	// AT DEPTH 0 THE FILTER GETS THE KNOB on every sample, and the audio
	// is bit-identical to a patch that never heard of the control.
	{
		R2SynthDsp a, b;
		setup (a, 0.0);
		setup (b, 0.0);
		b.setLfoResonanceDepth (0.0);   // set twice, on purpose: no drift from the smoother
		std::vector<float> la (48000), ra (la.size ()), lb (la.size ()), rb (la.size ());
		bool onKnob = true;
		for (size_t i = 0; i < la.size (); i += 480)
		{
			a.render (&la[i], &ra[i], 480);
			b.render (&lb[i], &rb[i], 480);
			onKnob = onKnob && (a.lastResonance () == static_cast<double> (static_cast<float> (0.5)));
		}
		check (onKnob, "at depth 0 the filter is handed the Resonance knob exactly");
		check (la == lb, "at depth 0 the output is deterministic and unchanged");
	}

	// AT DEPTH THE RESONANCE SWINGS round the knob, at the LFO's rate,
	// and the swing reaches the expected ends.
	{
		R2SynthDsp d;
		setup (d, 0.3);
		float l[1], r[1];
		for (int i = 0; i < 4800; ++i) d.render (l, r, 1);   // the smoothers settle
		std::vector<double> k (48000);
		double lo = 9.0, hi = -9.0;
		for (auto& v : k)
		{
			d.render (l, r, 1);
			v = d.lastResonance ();
			lo = std::min (lo, v); hi = std::max (hi, v);
		}
		checkClose (lo, 0.2, 0.01, "depth 0.3 round 0.5 swings down to 0.2 (" + num (lo, "%.3f") + ")");
		checkClose (hi, 0.8, 0.01, "and up to 0.8 (" + num (hi, "%.3f") + ")");
		for (auto& v : k) v -= 0.5;
		checkClose (measureFrequency (k, 48000.0), 5.0, 0.05, "the resonance moves at LFO 1's rate");
	}

	// FULL DEPTH FROM A KNOB AT ZERO sings on the peaks: the swing crosses
	// self-oscillation, and nothing blows up.
	{
		R2SynthDsp d;
		setup (d, kMaxLfoResonanceK);
		d.setResonance (0.0);
		d.setLfoRate (1.0);
		d.reset ();
		d.noteOn (48, 1.f);
		std::vector<float> l (48000 * 4), r (l.size ());
		d.render (l.data (), r.data (), static_cast<int> (l.size ()));
		bool finite = true;
		for (float v : l) finite = finite && std::isfinite (v);
		check (finite, "full depth from Resonance 0 stays finite");
		check (peakDbFS (l, 0) < 6.0, "and bounded (" + num (peakDbFS (l, 0), "%.2f") + " dBFS)");
	}
}

//------------------------------------------------------------------------
//------------------------------------------------------------------------
// The filter display's curve: ladderResponseGain() against a MEASURED
// ladder, and traceFilterResponse() against process()'s set-up.
//------------------------------------------------------------------------
static void testLadderResponse ()
{
	std::printf ("ladder response (filter display)\n");

	const double fs = 48000.0;
	const double cutoffs[] = { 300.0, 1200.0, 6000.0 };
	const double resonances[] = { 0.0, 0.5, 0.9 };   // loop gain

	for (double cutoff : cutoffs)
		for (double k : resonances)
		{
			double coeff = 0.0, unity = 0.0;
			ladderTuning ().lookup (cutoff / fs, coeff, unity);
			const double fb = k * unity;

			for (double ratio : { 0.25, 1.0, 3.0 })
			{
				const double hz = cutoff * ratio;
				const double w = 2.0 * 3.14159265358979323846 * hz / fs;

				// A quiet sine, so limitFeedback() stays linear; settle,
				// then take the amplitude at w by correlation - a peak
				// reading misses the crest of a sine near Nyquist.
				MoogLadder ladder;
				ladder.reset ();
				const double amp = 0.01;
				const int settle = static_cast<int> (fs);
				const int measure = static_cast<int> (fs / 2);
				double re = 0.0, im = 0.0;
				for (int n = 0; n < settle + measure; ++n)
				{
					const double y = ladder.tick (amp * std::sin (w * n), coeff, fb);
					if (n >= settle)
					{
						re += y * std::cos (w * n);
						im += y * std::sin (w * n);
					}
				}
				const double got = 2.0 * std::sqrt (re * re + im * im) / measure;
				const double measuredDb  = 20.0 * std::log10 (got / amp);
				const double predictedDb = 20.0 * std::log10 (ladderResponseGain (w, coeff, fb));

				char what[160];
				std::snprintf (what, sizeof (what),
				               "response at %.0f Hz, cutoff %.0f Hz, k %.1f: measured %.2f dB, predicted %.2f dB",
				               hz, cutoff, k, measuredDb, predictedDb);
				checkClose (measuredDb, predictedDb, 0.25, what);
			}
		}

	// The trace: process()'s clamp and scaling, and the shape a
	// low-pass display has to have.
	const int n = 64;
	float db[n];
	traceFilterResponse (1200.0, 0.0, fs, 20.0, 20000.0, db, n);
	check (std::fabs (db[0]) < 0.5, "the trace is flat (0 dB) at 20 Hz with no resonance");
	check (db[n - 1] < -40.f, "the trace is at least 40 dB down at 20 kHz from a 1.2 kHz cutoff");
	bool falling = true;
	for (int i = 1; i < n; ++i)
		if (db[i] > db[i - 1] + 1e-4f)
			falling = false;
	check (falling, "with no resonance the trace never rises");

	float peaky[n];
	traceFilterResponse (1200.0, 0.9, fs, 20.0, 20000.0, peaky, n);
	int top = 0;
	for (int i = 1; i < n; ++i)
		if (peaky[i] > peaky[top])
			top = i;
	const double topHz = 20.0 * std::pow (1000.0, static_cast<double> (top) / (n - 1));
	check (topHz > 900.0 && topHz < 1600.0, "with resonance the peak is at the cutoff (got " +
	       std::to_string (topHz) + " Hz)");
	check (peaky[top] > 6.f, "and stands at least 6 dB above the floor of the passband");

	float nyq[4];
	traceFilterResponse (1200.0, 0.0, 22050.0, 1000.0, 20000.0, nyq, 4);
	check (nyq[3] == -120.f, "points above Nyquist read -120 dB");
}

int main ()
{
	std::printf ("R2Synth DSP tests\n");
	std::printf ("---------------------\n");

	testConversions ();
	testSmootherStep ();
	testOutputStage ();
	testVcos ();
	testLfo ();
	testLfo2 ();
	testLfoResonance ();
	testLadder ();
	testLadderResponse ();
	testMs20 ();
	testFilterSwitch ();
	testAdsr ();
	testNotes ();
	testPortamento ();
	testModulation ();
	testTraces ();
	testDegenerate ();
	measureDefaultLevel ();

	std::printf ("---------------------\n");
	std::printf ("%d checks, %d failures\n", gChecks, gFailures);

	return (gFailures == 0) ? 0 : 1;
}
