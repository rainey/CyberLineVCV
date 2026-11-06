// CyberlineDspGuard — RAII x87/SSE denormal protection (rack-free).
//
// Delay lines with damping (wallDamping) and one-pole smoothers decay
// exponentially toward zero; below ~1.17e-38 the x86 FPU leaves the fast
// hardware path and traps to a microcode assist for every affected
// operation. Measured on this suite: 2446 ns/sample vs 41 ns/sample
// during ring-out (~60x) — the mechanism behind the intermittent CPU
// spikes / audio pops after phonemes.
//
// DenormalGuard sets MXCSR FTZ (SSE2) + DAZ (SSE3) on construction and
// restores the previous state on destruction, so each process() call is
// self-contained and polite to the host thread. No-op on non-x86.
//
// Usage: one line at the top of a Module::process():
//     cyberline::DenormalGuard guard;

#pragma once

#if (defined(__x86_64__) || defined(__i386__) || \
     defined(_M_X64) || defined(_M_IX86)) && !defined(CYBERLINE_NO_SSE)
#include <pmmintrin.h>
#define CYBERLINE_HAS_SSE 1
#else
#define CYBERLINE_HAS_SSE 0
#endif

namespace cyberline {

struct DenormalGuard {
#if CYBERLINE_HAS_SSE
	unsigned mxcsr;
	DenormalGuard() {
		mxcsr = _mm_getcsr();
		_MM_SET_FLUSH_ZERO_MODE(_MM_FLUSH_ZERO_ON);
		_MM_SET_DENORMALS_ZERO_MODE(_MM_DENORMALS_ZERO_ON);
	}
	~DenormalGuard() {
		_mm_setcsr(mxcsr);
	}
#else
	// architectures without the x87 subnormal trap (ARM flushes by
	// default in most configurations): nothing to do
#endif
};

} // namespace cyberline
