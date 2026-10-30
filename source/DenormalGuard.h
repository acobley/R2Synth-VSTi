//------------------------------------------------------------------------
// R2Synth - flush-to-zero for the length of one process() call
//------------------------------------------------------------------------
//
// Taken from SpaceDub. R2Synth's own recursive state already flushes
// itself (scrub() on the filters at 1e-30, the smoothers at 1e-25, the
// half-band decimator at 1e-30, the envelopes snap to idle), so this is
// belt and braces: it covers every other float and double operation on
// the audio thread, and anything added later that forgets to flush.
// Denormal arithmetic is slow on x86 - SpaceDub measured 7-16x.
//
// The FPU mode belongs to the host's thread, so it is saved on entry and
// put back on exit - never left switched on.
//
//   x86_64 : MXCSR FTZ (bit 15) and DAZ (bit 6)
//   arm64  : FPCR FZ (bit 24), which flushes inputs and outputs both
//
// Standard library only, so tests/DspTests.cpp can check it without
// the VST3 SDK.
//------------------------------------------------------------------------

#pragma once

#include <cstdint>

#if defined(__x86_64__) || defined(_M_X64) || defined(__SSE2__)
#include <xmmintrin.h>
#define R2SYNTH_FTZ_X86 1
#elif defined(__aarch64__) && (defined(__GNUC__) || defined(__clang__))
#define R2SYNTH_FTZ_ARM64 1
#endif

namespace R2Synth {

class ScopedFlushDenormals
{
public:
	ScopedFlushDenormals ()
	{
#if defined(R2SYNTH_FTZ_X86)
		mSaved = _mm_getcsr ();
		_mm_setcsr (static_cast<unsigned> (mSaved) | 0x8040u);
#elif defined(R2SYNTH_FTZ_ARM64)
		std::uint64_t fpcr;
		asm volatile ("mrs %0, fpcr" : "=r" (fpcr));
		mSaved = fpcr;
		asm volatile ("msr fpcr, %0" : : "r" (fpcr | (std::uint64_t (1) << 24)));
#endif
	}

	~ScopedFlushDenormals ()
	{
#if defined(R2SYNTH_FTZ_X86)
		_mm_setcsr (static_cast<unsigned> (mSaved));
#elif defined(R2SYNTH_FTZ_ARM64)
		asm volatile ("msr fpcr, %0" : : "r" (mSaved));
#endif
	}

	/** False on an architecture the guard does not know (it then does
	    nothing, and DelayLine's own flushing is all there is). */
	static constexpr bool supported ()
	{
#if defined(R2SYNTH_FTZ_X86) || defined(R2SYNTH_FTZ_ARM64)
		return true;
#else
		return false;
#endif
	}

	ScopedFlushDenormals (const ScopedFlushDenormals&) = delete;
	ScopedFlushDenormals& operator= (const ScopedFlushDenormals&) = delete;

private:
	std::uint64_t mSaved = 0;
};

} // namespace R2Synth
