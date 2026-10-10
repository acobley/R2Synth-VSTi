//------------------------------------------------------------------------
// R2Synth - the audio engine, implementation
//
// SDK-FREE. If a #include of anything under pluginterfaces/ or
// public.sdk/ ever appears in this file, the standalone test build stops
// compiling and the only executable verification on this project goes
// with it. See the banner in R2SynthDsp.h.
//------------------------------------------------------------------------

#include "R2SynthDsp.h"

#include <algorithm>

namespace R2Synth {

//------------------------------------------------------------------------
namespace {

constexpr double kPi = 3.14159265358979323846;

/** Kill denormals and bound the recursive filter state - SpaceDub's
    scrub(), with ONE CHANGE: the floor is 1e-30, not SpaceDub's 1e-15.

    SpaceDub's floor is harmless in a delay line and not in a tuned
    synth filter. The ladder's first stage runs at about 0.35 f^3 of the
    input, and at a 20 Hz cutoff on a 192 k engine f is under 0.001 - so
    the -100 dBFS noise that starts self-oscillation (kFilterNoiseLevel)
    arrives at the first pole at around 1e-15 and was being zeroed, and a
    low cutoff at a high rate could not sing. 1e-30 is still far above
    the denormal range (2e-308), which is all the floor is for. The
    ceiling is unchanged: with limitFeedback() in the loop it is never
    reached in normal use, and it is the net under a NaN. */
constexpr double kStateCeiling = 16.0;
constexpr double kStateFloor   = 1.0e-30;

inline double scrub (double v)
{
	if (!std::isfinite (v))
		return 0.0;
	if (v > -kStateFloor && v < kStateFloor)
		return 0.0;
	return std::clamp (v, -kStateCeiling, kStateCeiling);
}

/** The polyBLEP residual: the difference between a naive step and a
    band-limited one, over one sample either side of the discontinuity.

    CREDIT. This is the standard two-sample polynomial BLEP, not original
    to this project. The technique is from V. Valimaki and A. Huovilainen,
    "Antialiasing Oscillators in Subtractive Synthesis", IEEE Signal
    Processing Magazine 24(2), 2007; this form of it is the widely shared
    snippet from the KVR Audio DSP forum ("PolyBLEP oscillators",
    https://www.kvraudio.com/forum/viewtopic.php?p=5269992), used in many
    tutorials and synths since. */
inline double polyBlep (double t, double dt)
{
	if (t < dt)
	{
		t /= dt;
		return t + t - t * t - 1.0;
	}
	if (t > 1.0 - dt)
	{
		t = (t - 1.0) / dt;
		return t * t + t + t + 1.0;
	}
	return 0.0;
}

/** A VCO is never asked for more than this fraction of the sample rate.
    polyBLEP needs dt < 0.5; above 0.45 the "oscillator" is a handful of
    samples a cycle and nothing useful is left to band-limit. */
constexpr double kMaxVcoIncrement = 0.45;

/** One-pole coefficient for a time constant of `tau` seconds. */
inline double onePole (double tau, double sampleRate)
{
	if (!(tau > 0.0) || !(sampleRate > 0.0))
		return 1.0;
	const double c = 1.0 - std::exp (-1.0 / (tau * sampleRate));
	return std::clamp (c, 0.0, 1.0);
}

} // anonymous namespace

//------------------------------------------------------------------------
// Smoother
//------------------------------------------------------------------------
void Smoother::setSampleRate (double sampleRate, double seconds)
{
	if (sampleRate < 1000.0)
		sampleRate = 44100.0;

	if (seconds <= 0.0)
	{
		// No glide asked for: follow the target exactly.
		mCoeff = 1.f;
		return;
	}

	mCoeff = static_cast<float> (onePole (seconds, sampleRate));
}

void Smoother::snap (float target)
{
	mTarget = target;
	mValue  = target;
}

float Smoother::next ()
{
	// The value returned is the one AFTER this sample's step, so a
	// coefficient of 1 gives the new value on the very first sample.
	mValue += (mTarget - mValue) * mCoeff;

	// Denormal flush on the recursive state only.
	if (mValue < kDenormalFloor && mValue > -kDenormalFloor)
		mValue = 0.f;

	return mValue;
}

//------------------------------------------------------------------------
// Vco
//------------------------------------------------------------------------
double Vco::next (double dt)
{
	const double t = mPhase;
	double v;

	if (mWave == Wave::Saw)
	{
		// Rising ramp, -1..+1, with the reset band-limited.
		v = 2.0 * t - 1.0 - polyBlep (t, dt);
	}
	else
	{
		// 50 % square: up at 0, down at 0.5 - two steps, two corrections.
		double t2 = t + 0.5;
		if (t2 >= 1.0)
			t2 -= 1.0;
		v = ((t < 0.5) ? 1.0 : -1.0) + polyBlep (t, dt) - polyBlep (t2, dt);
	}

	mPhase += dt;
	if (mPhase >= 1.0)
		mPhase -= 1.0;

	return v;
}

//------------------------------------------------------------------------
// MsLfo
//------------------------------------------------------------------------
void MsLfo::setSampleRate (double sampleRate)
{
	mSampleRate = (sampleRate >= 1000.0) ? sampleRate : 44100.0;
	mSlewCoeff  = onePole (kLfoSlewSeconds, mSampleRate);
}

void MsLfo::reset ()
{
	mPhase  = 0.0;
	mSlewed = lfoOutputValue (mOutput, 0.0, mShape);
}

double MsLfo::next ()
{
	const double raw = lfoOutputValue (mOutput, mPhase, mShape);

	mSlewed += (raw - mSlewed) * mSlewCoeff;

	mPhase += mRateHz / mSampleRate;
	if (mPhase >= 1.0)
		mPhase -= std::floor (mPhase);

	return mSlewed;
}

//------------------------------------------------------------------------
// SineLfo
//------------------------------------------------------------------------
void SineLfo::setSampleRate (double sampleRate)
{
	mSampleRate = (sampleRate >= 1000.0) ? sampleRate : 44100.0;
}

double SineLfo::next ()
{
	const double v = std::sin (2.0 * kPi * mPhase);

	mPhase += mRateHz / mSampleRate;
	if (mPhase >= 1.0)
		mPhase -= std::floor (mPhase);

	return v;
}

//------------------------------------------------------------------------
// Adsr
//------------------------------------------------------------------------
void Adsr::setSampleRate (double sampleRate)
{
	mSampleRate = (sampleRate >= 1000.0) ? sampleRate : 44100.0;
	recompute ();
}

void Adsr::setSpec (const AdsrSpec& spec)
{
	mSpec = spec;
	mSpec.sustain = std::clamp (mSpec.sustain, 0.0, 1.0);
	recompute ();
}

void Adsr::recompute ()
{
	// ATTACK: an RC charging towards kAttackOvershoot reaches 1.0 after
	// tau * ln (V / (V - 1)), so tau is chosen to make that the knob.
	const double attackTau = mSpec.attack / std::log (kAttackOvershoot / (kAttackOvershoot - 1.0));
	mAttackCoeff = onePole (attackTau, mSampleRate);

	// DECAY and RELEASE: the knob is the time to -60 dB of the distance
	// left to go - tau * ln (1000).
	const double ln60 = -std::log (kReleaseTargetLevel);
	mDecayCoeff   = onePole (mSpec.decay   / ln60, mSampleRate);
	mReleaseCoeff = onePole (mSpec.release / ln60, mSampleRate);
}

void Adsr::gateOn ()
{
	// FROM WHERE IT IS - never back to zero first. See the banner.
	mStage = Stage::Attack;
}

void Adsr::gateOff ()
{
	if (mStage != Stage::Idle)
		mStage = Stage::Release;
}

void Adsr::reset ()
{
	mStage = Stage::Idle;
	mLevel = 0.0;
}

float Adsr::next ()
{
	switch (mStage)
	{
		case Stage::Idle:
			mLevel = 0.0;
			break;

		case Stage::Attack:
			mLevel += (kAttackOvershoot - mLevel) * mAttackCoeff;
			if (mLevel >= 1.0)
			{
				mLevel = 1.0;
				mStage = Stage::Decay;
			}
			break;

		case Stage::Decay:
		case Stage::Sustain:
			// SUSTAIN IS TRACKED, not latched: the same glide that makes
			// the decay keeps following the knob while the key is held.
			mLevel += (mSpec.sustain - mLevel) * mDecayCoeff;
			if (mStage == Stage::Decay && std::fabs (mLevel - mSpec.sustain) < kEnvelopeIdleLevel)
				mStage = Stage::Sustain;
			break;

		case Stage::Release:
			mLevel -= mLevel * mReleaseCoeff;
			if (mLevel < kEnvelopeIdleLevel)
			{
				mLevel = 0.0;
				mStage = Stage::Idle;
			}
			break;
	}

	return static_cast<float> (mLevel);
}

//------------------------------------------------------------------------
// The ladder's tuning
//------------------------------------------------------------------------
namespace {

/** One pole stage of the ladder at w radians/sample: (1 + 0.3 z^-1) /
    (1 - (1 - F) z^-1), with F = coefficient * 1.16. Returns phase and
    magnitude. |1 - F| < 1 for every F used, so each atan2 stays in
    (-pi/2, pi/2) and the sum needs no unwrapping. */
void stageResponse (double w, double F, double& phase, double& magnitude)
{
	const double c = std::cos (w), s = std::sin (w);
	const double nRe = 1.0 + 0.3 * c,       nIm = -0.3 * s;
	const double dRe = 1.0 - (1.0 - F) * c, dIm = (1.0 - F) * s;

	phase = std::atan2 (nIm, nRe) - std::atan2 (dIm, dRe);
	magnitude = std::sqrt ((nRe * nRe + nIm * nIm) / (dRe * dRe + dIm * dIm));
}

} // anonymous namespace

double ladderLoopPhase (double w, double coefficient)
{
	// FOUR stages and the one-sample delay on the feedback (DelayLine
	// subtracts the PREVIOUS out4). The peak is where this is -pi.
	double phase = 0.0, mag = 0.0;
	stageResponse (w, coefficient * 1.16, phase, mag);
	return 4.0 * phase - w;
}

double ladderLoopGain (double w, double coefficient, double resonance)
{
	const double F = coefficient * 1.16;
	double phase = 0.0, mag = 0.0;
	stageResponse (w, F, phase, mag);
	const double g  = 0.35013 * F * F * F * F;
	const double fb = resonance * (1.0 - 0.15 * F * F);
	return g * fb * mag * mag * mag * mag;
}

double ladderResponseGain (double w, double coefficient, double resonance)
{
	const double F = coefficient * 1.16;
	double phase = 0.0, mag = 0.0;
	stageResponse (w, F, phase, mag);
	const double g  = 0.35013 * F * F * F * F;
	const double fb = resonance * (1.0 - 0.15 * F * F);

	// G = g * S^4, as a magnitude and a phase.
	const double gMag   = g * mag * mag * mag * mag;
	const double gPhase = 4.0 * phase;

	// 1 + G fb e^{-jw}
	const double loopMag = gMag * fb;
	const double dRe = 1.0 + loopMag * std::cos (gPhase - w);
	const double dIm = loopMag * std::sin (gPhase - w);
	const double den = std::sqrt (dRe * dRe + dIm * dIm);

	return (den > 1.0e-12) ? gMag / den : 1.0e12;
}

void traceFilterResponse (double cutoffHz, double resonanceK, double sampleRate,
                          double minHz, double maxHz, float* dbOut, int count)
{
	if (dbOut == nullptr || count <= 0)
		return;
	if (sampleRate < 1000.0)
		sampleRate = 44100.0;

	// EXACTLY process()'s set-up: the clamp, the lookup, the scaling.
	const double cutoff = std::clamp (cutoffHz, kMinCutoffHz, cutoffCeilingHz (sampleRate));
	double coeff = 0.0, unityFeedback = 0.0;
	ladderTuning ().lookup (cutoff / sampleRate, coeff, unityFeedback);
	const double feedback = resonanceK * unityFeedback;

	for (int i = 0; i < count; ++i)
	{
		const double t  = (count > 1) ? static_cast<double> (i) / (count - 1) : 0.0;
		const double hz = minHz * std::pow (maxHz / minHz, t);
		if (hz >= 0.5 * sampleRate)
		{
			dbOut[i] = -120.f;
			continue;
		}
		const double gain = ladderResponseGain (2.0 * kPi * hz / sampleRate, coeff, feedback);
		dbOut[i] = static_cast<float> (20.0 * std::log10 (std::max (gain, 1.0e-6)));
	}
}

const LadderTuning& ladderTuning ()
{
	static const LadderTuning table = [] {
		LadderTuning t {};
		const double logMin = std::log (LadderTuning::kMinNorm);
		const double logMax = std::log (LadderTuning::kMaxNorm);

		for (int i = 0; i < LadderTuning::kPoints; ++i)
		{
			const double norm = std::exp (logMin + (logMax - logMin) * i / (LadderTuning::kPoints - 1));
			const double w = 2.0 * kPi * norm;

			// Raising the coefficient moves the poles away from DC and
			// REDUCES the loop's phase lag at any w, so the -pi point
			// moves up - monotonic, and a bisection finds it.
			double lo = 1.0e-7, hi = 1.5;
			for (int it = 0; it < 64; ++it)
			{
				const double mid = 0.5 * (lo + hi);
				if (ladderLoopPhase (w, mid) < -kPi)
					lo = mid;          // too much lag: the peak is above w
				else
					hi = mid;
			}
			const double coeff = 0.5 * (lo + hi);

			t.coefficient[i]   = coeff;
			t.unityFeedback[i] = 1.0 / ladderLoopGain (w, coeff, 1.0);
		}
		return t;
	} ();

	return table;
}

void LadderTuning::lookup (double normFreq, double& coeff, double& feedbackAtUnity) const
{
	if (!(normFreq > kMinNorm)) normFreq = kMinNorm;
	if (normFreq > kMaxNorm)    normFreq = kMaxNorm;

	const double pos = (std::log (normFreq) - std::log (kMinNorm))
	                 / (std::log (kMaxNorm) - std::log (kMinNorm)) * (kPoints - 1);
	int i = static_cast<int> (pos);
	if (i >= kPoints - 1)
		i = kPoints - 2;
	const double frac = pos - i;

	coeff           = coefficient[i]   + (coefficient[i + 1]   - coefficient[i])   * frac;
	feedbackAtUnity = unityFeedback[i] + (unityFeedback[i + 1] - unityFeedback[i]) * frac;
}

//------------------------------------------------------------------------
// The MS-20 filter
//------------------------------------------------------------------------
double ms20ResponseGain (double w, double g, double k, double damping)
{
	// The bilinear map, prewarped: the digital filter at w IS the
	// analogue one at this normalised frequency.
	const double omega = std::tan (0.5 * w) / g;
	const double re = 1.0 - omega * omega;
	const double im = (damping - k) * omega;
	const double den = std::sqrt (re * re + im * im);
	return (den > 1.0e-12) ? 1.0 / den : 1.0e12;
}

double ms20DiodeSolve (double a, double b, double knee)
{
	constexpr int kMaxIterations = 16;
	if (!(knee > 0.0))
		return 0.0;
	const double vd  = knee / kMs20DiodeSharpness;
	const double eps = knee / std::sinh (kMs20DiodeSharpness);

	if (!std::isfinite (a))
		return 0.0;
	const double drive = std::fabs (a);
	const double c = 1.0 - b;
	if (drive == 0.0 || !(c > 0.0))
		return 0.0;

	// Start ABOVE the root: both of these are.
	double v = std::min (drive / c, vd * std::asinh (drive / eps));
	for (int i = 0; i < kMaxIterations; ++i)
	{
		const double x  = v / vd;
		const double f  = c * v + eps * std::sinh (x) - drive;
		const double fp = c + (eps / vd) * std::cosh (x);
		const double step = f / fp;
		v -= step;
		if (std::fabs (step) <= 1.0e-12 * (1.0 + v))
			break;
	}
	return (a < 0.0) ? -v : v;
}

double Ms20Filter::tick (double input, double g, double k)
{
	// Trapezoidal one-pole: lp = G x + (1 - G) s, then s = 2 lp - s.
	const double G = g / (1.0 + g);
	const double a = 1.0 - G;
	constexpr double c = kKorg35Damping - 2.0;   // the Sallen-Key's loading

	// LP1 - the input pole, outside the loop.
	const double y1 = G * input + a * mS1;
	mS1 = scrub (2.0 * y1 - mS1);

	// THE ZERO-DELAY SOLVE, through the forward-path diodes - see the
	// class comment. v is the gain element's output (the filter's
	// output); p, LP2's output, is what drives it.
	const double scale = 1.0 / (1.0 + G * a * c);
	const double A = (G * y1 - G * a * mS3 + a * mS2) * scale;
	const double B = G * a * k * scale;
	const double v = ms20DiodeSolve (A, B, kMs20DiodeKnee);
	const double p = A + B * v;

	// LP2 and the feedback high-pass advance. (u, the summing point, is
	// never needed on its own: p already folds it in.)
	mS2 = scrub (2.0 * p - mS2);
	const double fb  = k * v - c * p;
	const double lp3 = G * fb + a * mS3;
	mS3 = scrub (2.0 * lp3 - mS3);

	return v;
}

double Ms20LateFilter::tick (double input, double g, double k)
{
	const double G = g / (1.0 + g);
	const double a = 1.0 - G;

	// LP1, behind the input OTA.
	const double y1 = G * otaSaturate (input) + a * mS1;
	mS1 = scrub (2.0 * y1 - mS1);

	// THE ZERO-DELAY SOLVE through the feedback diodes - see the class
	// comment. v is the feedback after the diode strings.
	const double A = k * (G * y1 - G * a * mS3 + a * mS2);
	const double B = k * G * a;
	const double v = ms20DiodeSolve (A, B, kMs20LateDiodeKnee);
	const double u = y1 + a * (v - mS3);

	// LP2 - the output, ahead of the diodes.
	const double lp2 = G * u + a * mS2;
	mS2 = scrub (2.0 * lp2 - mS2);

	// The feedback high-pass, driven by the clipped feedback.
	const double lp3 = G * v + a * mS3;
	mS3 = scrub (2.0 * lp3 - mS3);

	return lp2;
}

void traceFilterResponse (FilterType type, double cutoffHz, double resonanceK, double sampleRate,
                          double minHz, double maxHz, float* dbOut, int count)
{
	if (type == FilterType::Moog)
	{
		traceFilterResponse (cutoffHz, resonanceK, sampleRate, minHz, maxHz, dbOut, count);
		return;
	}

	if (dbOut == nullptr || count <= 0)
		return;
	if (sampleRate < 1000.0)
		sampleRate = 44100.0;

	// EXACTLY process()'s set-up: the same clamp, the same prewarp, the
	// same scaling of the resonance.
	const double cutoff = std::clamp (cutoffHz, kMinCutoffHz, cutoffCeilingHz (sampleRate));
	const double g = ms20Prewarp (cutoff, sampleRate);
	const bool early = (type == FilterType::Ms20);
	const double k = early ? korg35Feedback (resonanceK) : ms20Feedback (resonanceK);
	const double damping = early ? kKorg35Damping : kOtaDamping;

	for (int i = 0; i < count; ++i)
	{
		const double t  = (count > 1) ? static_cast<double> (i) / (count - 1) : 0.0;
		const double hz = minHz * std::pow (maxHz / minHz, t);
		if (hz >= 0.5 * sampleRate)
		{
			dbOut[i] = -120.f;
			continue;
		}
		const double gain = ms20ResponseGain (2.0 * kPi * hz / sampleRate, g, k, damping);
		dbOut[i] = static_cast<float> (20.0 * std::log10 (std::max (gain, 1.0e-6)));
	}
}

//------------------------------------------------------------------------
// MoogLadder
//------------------------------------------------------------------------
void MoogLadder::reset ()
{
	mIn1 = mIn2 = mIn3 = mIn4 = 0.0;
	mOut1 = mOut2 = mOut3 = mOut4 = 0.0;
}

double MoogLadder::tick (double input, double coeff, double resonance)
{
	// "Moog VCF, variation 2" (musicdsp.org) - SpaceDub's
	// DelayLine::process, line for line, except that the feedback term
	// goes through limitFeedback(). At resonance 0 that term is exactly
	// zero and this is SpaceDub's arithmetic to the bit.
	const double f  = coeff * 1.16;
	const double fb = resonance * (1.0 - 0.15 * f * f);

	double x = input;
	x -= limitFeedback (mOut4 * fb);
	x *= 0.35013 * (f * f) * (f * f);

	mOut1 = scrub (x + 0.3 * mIn1 + (1.0 - f) * mOut1);      // Pole 1
	mIn1  = x;
	mOut2 = scrub (mOut1 + 0.3 * mIn2 + (1.0 - f) * mOut2);  // Pole 2
	mIn2  = mOut1;
	mOut3 = scrub (mOut2 + 0.3 * mIn3 + (1.0 - f) * mOut3);  // Pole 3
	mIn3  = mOut2;
	mOut4 = scrub (mOut3 + 0.3 * mIn4 + (1.0 - f) * mOut4);  // Pole 4
	mIn4  = mOut3;

	return mOut4;
}

//------------------------------------------------------------------------
// Displays
//------------------------------------------------------------------------
namespace {

/** Points simulated per point drawn, and the bounds on the simulated
    rate - enough that a 0.5 ms attack is several samples even on a
    30-second axis, and no more than a real engine would run. */
constexpr double kTraceOversample = 8.0;
constexpr double kMinTraceRate    = 2000.0;
constexpr double kMaxTraceRate    = 192000.0;

} // anonymous namespace

double traceEnvelopes (const AdsrSpec& vcf, const AdsrSpec& vca,
                       float* vcfOut, float* vcaOut, int count,
                       double* gateOffSeconds)
{
	if (count < 2)
		return 0.0;

	// THE KEY IS HELD for the longer attack-plus-decay and a quarter as
	// long again - enough for both envelopes to be seen settling on
	// their sustain - and released at the same moment for both. Then the
	// axis runs on for the longer release, which is calibrated to -60 dB,
	// so both curves visibly reach the floor.
	const double settle  = std::max (vcf.attack + vcf.decay, vca.attack + vca.decay);
	const double gateOff = std::max (settle * 1.25, 0.010);
	const double span    = gateOff + std::max (vcf.release, vca.release);

	if (gateOffSeconds)
		*gateOffSeconds = gateOff;

	double rate = count * kTraceOversample / span;
	rate = std::min (kMaxTraceRate, std::max (kMinTraceRate, rate));

	long total = static_cast<long> (std::ceil (span * rate));
	if (total < 2)
		total = 2;
	const long offAt = static_cast<long> (gateOff * rate);

	auto run = [&] (const AdsrSpec& spec, float* out)
	{
		if (!out)
			return;

		Adsr e;
		e.setSampleRate (rate);
		e.setSpec (spec);
		e.reset ();
		e.gateOn ();

		// EVERY POINT FROM ONE RUN - re-running per point would be
		// quadratic.
		int written = 0;
		long nextIndex = 0;
		for (long n = 0; n < total && written < count; ++n)
		{
			if (n == offAt)
				e.gateOff ();
			const float v = e.next ();
			if (n < nextIndex)
				continue;
			out[written++] = v;
			nextIndex = static_cast<long> (static_cast<double> (written) * (total - 1) / (count - 1));
		}
		for (; written < count; ++written)
			out[written] = 0.f;
	};

	run (vcf, vcfOut);
	run (vca, vcaOut);

	return span;
}

void traceLfo (LfoOutput output, double shape, float* out, int count, double cycles)
{
	if (count < 2 || out == nullptr)
		return;

	for (int i = 0; i < count; ++i)
	{
		double phase = cycles * static_cast<double> (i) / (count - 1);
		phase -= std::floor (phase);
		// The very last point is the END of the last cycle, not the
		// start of the next one.
		if (i == count - 1)
			phase = 1.0 - 1e-9;

		out[i] = static_cast<float> (lfoOutputValue (output, phase, shape));
	}
}

//------------------------------------------------------------------------
// R2SynthDsp
//------------------------------------------------------------------------
R2SynthDsp::R2SynthDsp ()
{
	// Built HERE, the first time, so no audio thread ever builds it.
	(void) ladderTuning ();

	for (int i = 0; i < kNumVcos; ++i)
	{
		mVcoLevel[i].snap (0.f);
		mVcoTune[i].snap (0.f);
	}
	mCutoffOct.snap (static_cast<float> (std::log2 (1000.0)));

	// The constructor must leave the object usable: a host may call
	// process() before setupProcessing in a rare restart.
	setSampleRate (44100.0);
}

void R2SynthDsp::setSampleRate (double sampleRate)
{
	if (sampleRate < 1000.0)
		sampleRate = 44100.0;

	mSampleRate = sampleRate;

	// EVERY rate-dependent coefficient, here and nowhere else.
	for (int i = 0; i < kNumVcos; ++i)
	{
		mVcoLevel[i].setSampleRate (mSampleRate, kTrimSmoothingSeconds);
		mVcoTune[i].setSampleRate (mSampleRate, kTrimSmoothingSeconds);
	}
	mCutoffOct.setSampleRate (mSampleRate, kTrimSmoothingSeconds);
	mResonance.setSampleRate (mSampleRate, kTrimSmoothingSeconds);
	mLfoPitch.setSampleRate (mSampleRate, kTrimSmoothingSeconds);
	mLfoCutoff.setSampleRate (mSampleRate, kTrimSmoothingSeconds);
	mLfoVca.setSampleRate (mSampleRate, kTrimSmoothingSeconds);
	mLfoRes.setSampleRate (mSampleRate, kTrimSmoothingSeconds);
	mLfo2Level.setSampleRate (mSampleRate, kTrimSmoothingSeconds);
	mEnvAmount.setSampleRate (mSampleRate, kTrimSmoothingSeconds);
	mOutput.setSampleRate (mSampleRate, kTrimSmoothingSeconds);

	mLfo.setSampleRate (mSampleRate);
	mLfo2.setSampleRate (mSampleRate);
	mVcfEnv.setSampleRate (mSampleRate);
	mVcaEnv.setSampleRate (mSampleRate);

	reset ();
}

void R2SynthDsp::setMaxBlockSize (int maxSamples)
{
	// Nothing in the voice needs scratch - it renders a sample at a
	// time - so this only records the promise for the tests.
	mMaxBlockSize = (maxSamples > 0) ? maxSamples : 0;
}

void R2SynthDsp::reset ()
{
	// SNAP every smoother, not glide: a reset that left the output at
	// zero would fade in, and a cutoff that swept from 1 kHz to the
	// loaded value would be a swoop on every project load.
	for (int i = 0; i < kNumVcos; ++i)
	{
		mVcoLevel[i].snap (mVcoLevel[i].target ());
		mVcoTune[i].snap (mVcoTune[i].target ());
		mVco[i].reset (static_cast<double> (i) / kNumVcos);
	}
	mCutoffOct.snap (mCutoffOct.target ());
	mResonance.snap (mResonance.target ());
	mLfoPitch.snap (mLfoPitch.target ());
	mLfoCutoff.snap (mLfoCutoff.target ());
	mLfoVca.snap (mLfoVca.target ());
	mLfoRes.snap (mLfoRes.target ());
	mLfo2Level.snap (mLfo2Level.target ());
	mEnvAmount.snap (mEnvAmount.target ());
	mOutput.snap (static_cast<float> (dbToGain (mOutputDb)));

	// LFO 2 first: at phase 0 its sine is 0, so LFO 1 restarts at the
	// knob's own Shape.
	mLfo2.reset ();
	mLfo.setShape (mLfoShapeKnob);
	mLastLfoShape = mLfoShapeKnob;
	mLfo.reset ();
	mFilter.reset ();
	mMs20.reset ();
	mMs20Late.reset ();
	mFilterFadeSamples = 0;
	settleFilterWeights ();
	mVcfEnv.reset ();
	mVcaEnv.reset ();

	mHeldCount = 0;
	mCurrentNote = -1;

	mPitch = 60.0;
	mGlideStep = 0.0;
	mGlideSamples = 0;
	mHavePitch = false;
}

//------------------------------------------------------------------------
void R2SynthDsp::setOutputLevelDb (double db)
{
	mOutputDb = db;
	mOutput.setTarget (static_cast<float> (dbToGain (db)));
}

void R2SynthDsp::setVcoWave (int vco, Vco::Wave wave)
{
	if (vco >= 0 && vco < kNumVcos)
		mVco[vco].setWave (wave);
}

void R2SynthDsp::setVcoTune (int vco, double semitones)
{
	if (vco >= 0 && vco < kNumVcos)
		mVcoTune[vco].setTarget (static_cast<float> (std::clamp (semitones, -12.0, 12.0)));
}

void R2SynthDsp::setVcoLevel (int vco, double level)
{
	if (vco >= 0 && vco < kNumVcos)
		mVcoLevel[vco].setTarget (static_cast<float> (std::clamp (level, 0.0, 1.0)));
}

void R2SynthDsp::setLfoRate (double hz)          { mLfo.setRate (std::max (0.0, hz)); }
void R2SynthDsp::setLfoShape (double shape)
{
	// The KNOB. What LFO 1 runs at is this swept by LFO 2, per sample, in
	// renderSample(); setting it on the LFO too keeps it right for a
	// reset() that comes before the next sample.
	mLfoShapeKnob = std::clamp (shape, -1.0, 1.0);
	mLfo.setShape (mLfoShapeKnob);
}
void R2SynthDsp::setLfoOutput (LfoOutput o)     { mLfo.setOutput (o); }

void R2SynthDsp::setLfoPitchDepth (double semitones)
{
	mLfoPitch.setTarget (static_cast<float> (std::clamp (semitones, 0.0, kMaxLfoPitchSemis)));
}

void R2SynthDsp::setLfoCutoffDepth (double octaves)
{
	mLfoCutoff.setTarget (static_cast<float> (std::clamp (octaves, 0.0, kMaxLfoCutoffOctaves)));
}

void R2SynthDsp::setLfo2Rate (double hz)         { mLfo2.setRate (std::max (0.0, hz)); }

void R2SynthDsp::setLfo2Level (double level)
{
	mLfo2Level.setTarget (static_cast<float> (std::clamp (level, 0.0, 1.0)));
}

void R2SynthDsp::setLfoVcaDepth (double depth)
{
	mLfoVca.setTarget (static_cast<float> (std::clamp (depth, 0.0, 1.0)));
}

void R2SynthDsp::setLfoResonanceDepth (double loopGain)
{
	mLfoRes.setTarget (static_cast<float> (std::clamp (loopGain, 0.0, kMaxLfoResonanceK)));
}

void R2SynthDsp::setCutoffHz (double hz)
{
	hz = std::max (kMinCutoffHz, hz);
	mCutoffOct.setTarget (static_cast<float> (std::log2 (hz)));
}

void R2SynthDsp::setResonance (double loopGain)
{
	mResonance.setTarget (static_cast<float> (std::clamp (loopGain, 0.0, kMaxResonanceK)));
}

void R2SynthDsp::setKeyTrack (double amount)     { mKeyTrack = std::clamp (amount, 0.0, 1.0); }

void R2SynthDsp::setVcfEnvAmount (double octaves)
{
	mEnvAmount.setTarget (static_cast<float> (std::clamp (octaves, -kMaxEnvOctaves, kMaxEnvOctaves)));
}

void R2SynthDsp::setVcfEnvelope (const AdsrSpec& spec) { mVcfEnv.setSpec (spec); }
void R2SynthDsp::setVcaEnvelope (const AdsrSpec& spec) { mVcaEnv.setSpec (spec); }

void R2SynthDsp::setFilterType (FilterType type)
{
	const int t = static_cast<int> (type);
	if (t < 0 || t >= kNumFilterTypes)
		type = FilterType::Moog;
	if (type == mFilterType)
		return;

	mFilterType = type;

	// FROM WHEREVER THE FADE IS: every weight heads in a straight line
	// for its target in the same number of samples, so the sum stays 1
	// even when a switch turns a fade round half way.
	mFilterFadeSamples = std::max (1, static_cast<int> (std::lround (kFilterCrossfadeSeconds * mSampleRate)));
	for (int i = 0; i < kNumFilterTypes; ++i)
	{
		const double target = (i == static_cast<int> (type)) ? 1.0 : 0.0;
		mFilterWeightStep[i] = (target - mFilterWeight[i]) / mFilterFadeSamples;
	}
}

double R2SynthDsp::filterWeight (FilterType type) const
{
	const int t = static_cast<int> (type);
	return (t >= 0 && t < kNumFilterTypes) ? mFilterWeight[t] : 0.0;
}

void R2SynthDsp::settleFilterWeights ()
{
	// EXACT targets, and the filters now out of the output cleared.
	for (int i = 0; i < kNumFilterTypes; ++i)
	{
		const bool chosen = (i == static_cast<int> (mFilterType));
		mFilterWeight[i] = chosen ? 1.0 : 0.0;
		mFilterWeightStep[i] = 0.0;
	}
	if (mFilterType != FilterType::Moog)     mFilter.reset ();
	if (mFilterType != FilterType::Ms20)     mMs20.reset ();
	if (mFilterType != FilterType::Ms20Late) mMs20Late.reset ();
}

void R2SynthDsp::setPortamento (double seconds)
{
	mPortamentoSeconds = std::clamp (seconds, kMinPortamentoSeconds, kMaxPortamentoSeconds);
}

//------------------------------------------------------------------------
void R2SynthDsp::startPitchTo (int note)
{
	const double target = static_cast<double> (note);

	// A JUMP: portamento off, or nothing to slide from - the first note
	// after a reset has no previous pitch, and sliding up to it from an
	// arbitrary middle C would be a swoop nobody played.
	if (!mHavePitch || !portamentoActive (mPortamentoSeconds))
	{
		mPitch = target;
		mGlideStep = 0.0;
		mGlideSamples = 0;
		mHavePitch = true;
		return;
	}

	// A SLIDE, in a straight line in semitones, from WHERE THE PITCH IS -
	// mid-slide included, so a fast run of notes never jumps - to the
	// new note in the knob's time, whatever the interval. Whether the
	// previous key is still down makes no difference: the pitch is held
	// between notes, so a detached note glides exactly as a legato one.
	const int samples = std::max (1, static_cast<int> (std::lround (mPortamentoSeconds * mSampleRate)));
	mGlideSamples = samples;
	mGlideStep = (target - mPitch) / samples;
}

//------------------------------------------------------------------------

//------------------------------------------------------------------------
void R2SynthDsp::noteOn (int pitch, float /*velocity*/)
{
	// VELOCITY IS IGNORED, like the MS-20's keyboard. The parameter is
	// accepted so that adding a sensitivity control later is a change
	// here and not in the processor's event handling.
	if (pitch < 0 || pitch > 127)
		return;

	// A key already down moves to the top rather than being listed twice.
	int w = 0;
	for (int i = 0; i < mHeldCount; ++i)
		if (mHeld[i] != pitch)
			mHeld[w++] = mHeld[i];
	mHeldCount = w;

	if (mHeldCount == kMaxHeld)
	{
		// Full: the OLDEST key is forgotten. It is the one least likely
		// to be wanted back when the newer ones are released.
		for (int i = 1; i < kMaxHeld; ++i)
			mHeld[i - 1] = mHeld[i];
		--mHeldCount;
	}

	mHeld[mHeldCount++] = pitch;
	mCurrentNote = pitch;
	startPitchTo (pitch);

	// EVERY NEW NOTE RETRIGGERS, from wherever the envelopes are.
	mVcfEnv.gateOn ();
	mVcaEnv.gateOn ();
}

void R2SynthDsp::noteOff (int pitch)
{
	int w = 0;
	bool found = false;
	for (int i = 0; i < mHeldCount; ++i)
	{
		if (mHeld[i] == pitch)
			found = true;
		else
			mHeld[w++] = mHeld[i];
	}
	mHeldCount = w;

	// A note-off for a key we never saw - every host sends a batch on
	// transport stop - changes nothing.
	if (!found || pitch != mCurrentNote)
		return;

	if (mHeldCount > 0)
	{
		// Back to the most recent key still down, at ITS pitch, without
		// retriggering: the key being held has already been struck.
		mCurrentNote = mHeld[mHeldCount - 1];
		startPitchTo (mCurrentNote);    // slides back, like any note change
		return;
	}

	mVcfEnv.gateOff ();
	mVcaEnv.gateOff ();
}

void R2SynthDsp::allNotesOff ()
{
	mHeldCount = 0;
	mVcfEnv.gateOff ();
	mVcaEnv.gateOff ();
}

double R2SynthDsp::tailSeconds () const
{
	return releaseTailSeconds (mVcaEnv.spec ().release);
}

//------------------------------------------------------------------------
float R2SynthDsp::renderSample ()
{
	// LFO 2 turns LFO 1's Shape knob, then LFO 1 runs at the result.
	// The Level is smoothed, so a move of it glides the sweep in rather
	// than jumping the shape.
	const double lfo2 = mLfo2.next ();
	mLastLfoShape = sweptLfoShape (mLfoShapeKnob, mLfo2Level.next (), lfo2);
	mLfo.setShape (mLastLfoShape);

	const double lfo = mLfo.next ();
	const double vcfEnv = mVcfEnv.next ();
	const double vcaEnv = mVcaEnv.next ();

	// THE PITCH, gliding if a slide is under way. Before the first note
	// the VCOs still run - an analogue VCO does - at middle C (mPitch's
	// reset value), behind a closed VCA. The last step lands EXACTLY on
	// the note, so a finished slide is in tune to the bit.
	if (mGlideSamples > 0)
	{
		if (--mGlideSamples == 0)
			mPitch = static_cast<double> (mCurrentNote);
		else
			mPitch += mGlideStep;
	}
	const double note = mPitch;

	// ---- VCOs and mixer ------------------------------------------------
	const double vibrato = lfo * mLfoPitch.next ();
	double mix = 0.0;
	for (int i = 0; i < kNumVcos; ++i)
	{
		const double level = mVcoLevel[i].next ();
		const double hz = noteToHz (note + mVcoTune[i].next () + vibrato);
		const double inc = std::min (hz / mSampleRate, kMaxVcoIncrement);

		// EVERY VCO RUNS whether or not it is heard, so turning one up
		// mid-note brings it in at its own phase rather than restarting.
		mix += level * mVco[i].next (inc);
	}
	mix *= kMixerGain;

	// ---- VCF -----------------------------------------------------------
	// Everything that moves the cutoff is added in OCTAVES, then turned
	// into Hz once. Key tracking is about middle C: at 100 % the cutoff
	// follows the keyboard exactly, at 0 % it ignores it.
	const double octaves = mCutoffOct.next ()
	                     + mEnvAmount.next () * vcfEnv
	                     + mLfoCutoff.next () * lfo
	                     + mKeyTrack * (note - 60.0) / 12.0;

	double cutoff = std::exp2 (octaves);
	cutoff = std::clamp (cutoff, kMinCutoffHz, cutoffCeilingHz (mSampleRate));
	mLastCutoffHz = cutoff;

	// The noise floor - see kFilterNoiseLevel. A 32-bit LCG: cheap,
	// deterministic, so the tests are repeatable.
	mNoiseState = mNoiseState * 1664525u + 1013904223u;
	const double noise = (static_cast<double> (mNoiseState) / 4294967296.0 * 2.0 - 1.0) * kFilterNoiseLevel;

	// Resonance: the knob moved by LFO 1, in loop gain, then scaled to
	// this cutoff's unity feedback exactly as the knob alone is - so a
	// swing through 1.0 crosses self-oscillation at every cutoff.
	mLastResonance = lfoResonance (mResonance.next (), mLfoRes.next (), lfo);

	// THE VCF TYPE. Settled, only the chosen filter runs - and a settled
	// Moog is the ladder exactly as it was before the switch existed.
	// While a change cross-fades, every filter with any weight runs and
	// the output is their weighted sum.
	const double in = mix + noise;
	const double* w = mFilterWeight;
	const int moogI = static_cast<int> (FilterType::Moog);
	const int ms20I = static_cast<int> (FilterType::Ms20);
	const int lateI = static_cast<int> (FilterType::Ms20Late);

	double moog = 0.0, ms20 = 0.0, late = 0.0;
	if (w[moogI] > 0.0)
	{
		double coeff = 0.0, unityFeedback = 0.0;
		ladderTuning ().lookup (cutoff / mSampleRate, coeff, unityFeedback);
		moog = mFilter.tick (in, coeff, mLastResonance * unityFeedback);
	}
	if (w[ms20I] > 0.0 || w[lateI] > 0.0)
	{
		const double g = ms20Prewarp (cutoff, mSampleRate);
		if (w[ms20I] > 0.0)
			ms20 = kMs20OutputGain * mMs20.tick (in, g, korg35Feedback (mLastResonance));
		if (w[lateI] > 0.0)
			late = kMs20LateOutputGain * mMs20Late.tick (in, g, ms20Feedback (mLastResonance));
	}

	const double filtered = (mFilterFadeSamples == 0)
	                      ? ((mFilterType == FilterType::Moog) ? moog
	                         : (mFilterType == FilterType::Ms20) ? ms20 : late)
	                      : w[moogI] * moog + w[ms20I] * ms20 + w[lateI] * late;

	if (mFilterFadeSamples > 0)
	{
		if (--mFilterFadeSamples == 0)
			settleFilterWeights ();   // lands EXACTLY; clears the rest
		else
			for (int i = 0; i < kNumFilterTypes; ++i)
				mFilterWeight[i] += mFilterWeightStep[i];
	}

	// ---- VCA -----------------------------------------------------------
	// The LFO's VCA depth is TREMOLO DOWNWARDS from the envelope: at
	// 100 % the gain swings between the envelope and silence, and at no
	// depth can it lift the level above what the envelope asks for.
	const double tremolo = 1.0 - mLfoVca.next () * (0.5 - 0.5 * lfo);

	return static_cast<float> (filtered * vcaEnv * tremolo * kVoiceGain);
}

void R2SynthDsp::render (float* left, float* right, int numSamples)
{
	// A PARAMETER-ONLY BLOCK is legal and arrives in practice.
	if (numSamples <= 0 || left == nullptr || right == nullptr)
		return;

	for (int i = 0; i < numSamples; ++i)
	{
		const float s = renderSample ();
		left[i]  = s;
		right[i] = s;
	}

	applyOutputLevel (left, right, numSamples);
}

void R2SynthDsp::applyOutputLevel (float* left, float* right, int numSamples)
{
	if (numSamples <= 0 || left == nullptr || right == nullptr)
		return;

	// PER SAMPLE, and both channels off the SAME gain value.
	for (int i = 0; i < numSamples; ++i)
	{
		const float gain = mOutput.next ();
		left[i]  *= gain;
		right[i] *= gain;
	}
}

//------------------------------------------------------------------------
} // namespace R2Synth
