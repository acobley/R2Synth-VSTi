//------------------------------------------------------------------------
// SimpleSynth - the audio engine, implementation
//
// SDK-FREE. If a #include of anything under pluginterfaces/ or
// public.sdk/ ever appears in this file, the standalone test build stops
// compiling and the only executable verification on this project goes
// with it. See the banner in SimpleSynthDsp.h.
//------------------------------------------------------------------------

#include "SimpleSynthDsp.h"

#include <algorithm>

namespace SimpleSynth {

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
    band-limited one, over one sample either side of the discontinuity. */
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
// SimpleSynthDsp
//------------------------------------------------------------------------
SimpleSynthDsp::SimpleSynthDsp ()
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

void SimpleSynthDsp::setSampleRate (double sampleRate)
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
	mLfo2Level.setSampleRate (mSampleRate, kTrimSmoothingSeconds);
	mEnvAmount.setSampleRate (mSampleRate, kTrimSmoothingSeconds);
	mOutput.setSampleRate (mSampleRate, kTrimSmoothingSeconds);

	mLfo.setSampleRate (mSampleRate);
	mLfo2.setSampleRate (mSampleRate);
	mVcfEnv.setSampleRate (mSampleRate);
	mVcaEnv.setSampleRate (mSampleRate);

	reset ();
}

void SimpleSynthDsp::setMaxBlockSize (int maxSamples)
{
	// Nothing in the voice needs scratch - it renders a sample at a
	// time - so this only records the promise for the tests.
	mMaxBlockSize = (maxSamples > 0) ? maxSamples : 0;
}

void SimpleSynthDsp::reset ()
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
	mVcfEnv.reset ();
	mVcaEnv.reset ();

	mHeldCount = 0;
	mCurrentNote = -1;
}

//------------------------------------------------------------------------
void SimpleSynthDsp::setOutputLevelDb (double db)
{
	mOutputDb = db;
	mOutput.setTarget (static_cast<float> (dbToGain (db)));
}

void SimpleSynthDsp::setVcoWave (int vco, Vco::Wave wave)
{
	if (vco >= 0 && vco < kNumVcos)
		mVco[vco].setWave (wave);
}

void SimpleSynthDsp::setVcoTune (int vco, double semitones)
{
	if (vco >= 0 && vco < kNumVcos)
		mVcoTune[vco].setTarget (static_cast<float> (std::clamp (semitones, -12.0, 12.0)));
}

void SimpleSynthDsp::setVcoLevel (int vco, double level)
{
	if (vco >= 0 && vco < kNumVcos)
		mVcoLevel[vco].setTarget (static_cast<float> (std::clamp (level, 0.0, 1.0)));
}

void SimpleSynthDsp::setLfoRate (double hz)          { mLfo.setRate (std::max (0.0, hz)); }
void SimpleSynthDsp::setLfoShape (double shape)
{
	// The KNOB. What LFO 1 runs at is this swept by LFO 2, per sample, in
	// renderSample(); setting it on the LFO too keeps it right for a
	// reset() that comes before the next sample.
	mLfoShapeKnob = std::clamp (shape, -1.0, 1.0);
	mLfo.setShape (mLfoShapeKnob);
}
void SimpleSynthDsp::setLfoOutput (LfoOutput o)     { mLfo.setOutput (o); }

void SimpleSynthDsp::setLfoPitchDepth (double semitones)
{
	mLfoPitch.setTarget (static_cast<float> (std::clamp (semitones, 0.0, kMaxLfoPitchSemis)));
}

void SimpleSynthDsp::setLfoCutoffDepth (double octaves)
{
	mLfoCutoff.setTarget (static_cast<float> (std::clamp (octaves, 0.0, kMaxLfoCutoffOctaves)));
}

void SimpleSynthDsp::setLfo2Rate (double hz)         { mLfo2.setRate (std::max (0.0, hz)); }

void SimpleSynthDsp::setLfo2Level (double level)
{
	mLfo2Level.setTarget (static_cast<float> (std::clamp (level, 0.0, 1.0)));
}

void SimpleSynthDsp::setLfoVcaDepth (double depth)
{
	mLfoVca.setTarget (static_cast<float> (std::clamp (depth, 0.0, 1.0)));
}

void SimpleSynthDsp::setCutoffHz (double hz)
{
	hz = std::max (kMinCutoffHz, hz);
	mCutoffOct.setTarget (static_cast<float> (std::log2 (hz)));
}

void SimpleSynthDsp::setResonance (double loopGain)
{
	mResonance.setTarget (static_cast<float> (std::clamp (loopGain, 0.0, kMaxResonanceK)));
}

void SimpleSynthDsp::setKeyTrack (double amount)     { mKeyTrack = std::clamp (amount, 0.0, 1.0); }

void SimpleSynthDsp::setVcfEnvAmount (double octaves)
{
	mEnvAmount.setTarget (static_cast<float> (std::clamp (octaves, -kMaxEnvOctaves, kMaxEnvOctaves)));
}

void SimpleSynthDsp::setVcfEnvelope (const AdsrSpec& spec) { mVcfEnv.setSpec (spec); }
void SimpleSynthDsp::setVcaEnvelope (const AdsrSpec& spec) { mVcaEnv.setSpec (spec); }

//------------------------------------------------------------------------
void SimpleSynthDsp::noteOn (int pitch, float /*velocity*/)
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

	// EVERY NEW NOTE RETRIGGERS, from wherever the envelopes are.
	mVcfEnv.gateOn ();
	mVcaEnv.gateOn ();
}

void SimpleSynthDsp::noteOff (int pitch)
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
		return;
	}

	mVcfEnv.gateOff ();
	mVcaEnv.gateOff ();
}

void SimpleSynthDsp::allNotesOff ()
{
	mHeldCount = 0;
	mVcfEnv.gateOff ();
	mVcaEnv.gateOff ();
}

double SimpleSynthDsp::tailSeconds () const
{
	return releaseTailSeconds (mVcaEnv.spec ().release);
}

//------------------------------------------------------------------------
float SimpleSynthDsp::renderSample ()
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

	// Before the first note the VCOs still run - an analogue VCO does -
	// at middle C, behind a closed VCA.
	const double note = (mCurrentNote >= 0) ? static_cast<double> (mCurrentNote) : 60.0;

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

	double coeff = 0.0, unityFeedback = 0.0;
	ladderTuning ().lookup (cutoff / mSampleRate, coeff, unityFeedback);

	// The noise floor - see kFilterNoiseLevel. A 32-bit LCG: cheap,
	// deterministic, so the tests are repeatable.
	mNoiseState = mNoiseState * 1664525u + 1013904223u;
	const double noise = (static_cast<double> (mNoiseState) / 4294967296.0 * 2.0 - 1.0) * kFilterNoiseLevel;

	const double filtered = mFilter.tick (mix + noise, coeff, mResonance.next () * unityFeedback);

	// ---- VCA -----------------------------------------------------------
	// The LFO's VCA depth is TREMOLO DOWNWARDS from the envelope: at
	// 100 % the gain swings between the envelope and silence, and at no
	// depth can it lift the level above what the envelope asks for.
	const double tremolo = 1.0 - mLfoVca.next () * (0.5 - 0.5 * lfo);

	return static_cast<float> (filtered * vcaEnv * tremolo * kVoiceGain);
}

void SimpleSynthDsp::render (float* left, float* right, int numSamples)
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

void SimpleSynthDsp::applyOutputLevel (float* left, float* right, int numSamples)
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
} // namespace SimpleSynth
