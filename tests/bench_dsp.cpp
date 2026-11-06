// DSP micro-benchmark: ns/sample for Glottis, Tract (per VTL / sample rate),
// and Heart core, reported as % of the real-time budget at each sample rate.
// Build: g++ -O3 -funsafe-math-optimizations -std=c++11 -march=nehalem \
//          -o /tmp/bench_dsp tests/bench_dsp.cpp && /tmp/bench_dsp
#include "../src/CyberlineGlottis.hpp"
#include "../src/CyberlineTract.hpp"
#include "../src/CyberlineHeartCore.hpp"
#include "../src/CyberlineDspGuard.hpp"
#include <chrono>
#include <cstdio>
#include <initializer_list>

using namespace cyberline;
using Clock = std::chrono::steady_clock;

static volatile float sink = 0.f;

static double nsPerSample(long n, Clock::time_point t0, Clock::time_point t1) {
	return std::chrono::duration<double, std::nano>(t1 - t0).count() / (double) n;
}

static void report(const char* name, double ns, float sr) {
	double budget = 1e9 / sr;
	std::printf("%-34s %8.1f ns/sample  = %5.2f%% of one core @ %.0f Hz\n",
		name, ns, 100.0 * ns / budget, sr);
}

int main() {
	const long N = 2000000;

	for (float sr : {48000.f, 96000.f, 192000.f}) {
		CyberlineGlottis g;
		g.setSampleRate(sr);
		g.setTargets(140.f, 0.6f, 0.8f, 14.f);
		auto t0 = Clock::now();
		float acc = 0.f;
		for (long i = 0; i < N; i++) acc += g.process(0.3f, 0.f, 0.3f, 6.f);
		sink = acc;
		report("Glottis::process", nsPerSample(N, t0, Clock::now()), sr);
	}

	for (float vtl : {17.5f, 30.f, 60.f}) {
		for (float sr : {48000.f, 96000.f, 192000.f}) {
			CyberlineTract t;
			t.setGeometry(vtl, sr);
			t.setTongue(t.bladeStart + 0.5f * (t.tipStart - t.bladeStart), 2.5f);
			t.applyConstriction();
			float dt = 1.f / sr;
			auto t0 = Clock::now();
			float acc = 0.f;
			for (long i = 0; i < N / 4; i++) {
				float s = (i & 1) ? 0.2f : -0.2f;
				acc += t.process(dt, s, 0.f, 0.f, 0.1f);
			}
			sink = acc;
			char label[64];
			std::snprintf(label, sizeof(label), "Tract n=%d (VTL %.1f cm)", t.n, vtl);
			report(label, nsPerSample(N / 4, t0, Clock::now()), sr);
		}
	}

	for (float sr : {48000.f, 96000.f}) {
		CyberlineHeartCore h;
		h.setSampleRate(sr);
		CyberlineHeartCore::Params p;
		auto t0 = Clock::now();
		float acc = 0.f;
		for (long i = 0; i < N; i++) acc += h.process(p).sound;
		sink = acc;
		report("HeartCore::process", nsPerSample(N, t0, Clock::now()), sr);
	}

	// ---- denormal stress: ring-out to silence with vs without FTZ/DAZ ----
	// The damped delay lines decay exponentially; below ~1.17e-38 the x86
	// FPU leaves the fast path for a microcode assist on every junction op.
	{
		const float sr = 48000.f;
		const long ringN = 48000 * 4;   // 4 s of decay after a burst

		// with FTZ/DAZ (what the modules do via DenormalGuard)
		{
			DenormalGuard guard;
			CyberlineTract t;
			t.setGeometry(17.5f, sr);
			float dt = 1.f / sr;
			// excite, then let it ring out
			for (long i = 0; i < 1000; i++)
				t.process(dt, 0.5f, 0.f, 0.f, 0.f);
			auto t0 = Clock::now();
			float acc = 0.f;
			for (long i = 0; i < ringN; i++) acc += t.process(dt, 0.f, 0.f, 0.f, 0.f);
			sink = acc;
			report("Tract ring-out 4s (FTZ on)", nsPerSample(ringN, t0, Clock::now()), sr);
		}
		// without FTZ/DAZ (the pre-fix behavior)
		{
			CyberlineTract t;
			t.setGeometry(17.5f, sr);
			float dt = 1.f / sr;
			for (long i = 0; i < 1000; i++)
				t.process(dt, 0.5f, 0.f, 0.f, 0.f);
			auto t0 = Clock::now();
			float acc = 0.f;
			for (long i = 0; i < ringN; i++) acc += t.process(dt, 0.f, 0.f, 0.f, 0.f);
			sink = acc;
			report("Tract ring-out 4s (FTZ off)", nsPerSample(ringN, t0, Clock::now()), sr);
		}
	}

	// ---- articulation benchmark: moving shape, HIGH vs LOW quality ----
	// Tongue re-target every 16 samples (simulating CV motion), which
	// keeps shapeMoved true and forces coefficient refreshes.
	for (int low = 0; low < 2; low++) {
		const float sr = 48000.f;
		CyberlineTract t;
		t.setGeometry(17.5f, sr);
		t.lowQuality = low != 0;
		float dt = 1.f / sr;
		auto t0 = Clock::now();
		float acc = 0.f;
		for (long i = 0; i < N / 4; i++) {
			if ((i & 15) == 0) {
				float tx = (float) ((i >> 4) % 100) / 100.f;
				t.setTongue(t.bladeStart + tx * (t.tipStart - t.bladeStart), 2.5f);
				t.applyConstriction();
			}
			float s = (i & 1) ? 0.2f : -0.2f;
			acc += t.process(dt, s, 0.f, 0.f, 0.1f);
		}
		sink = acc;
		char label[64];
		std::snprintf(label, sizeof(label), "Tract moving-shape (%s quality)",
			low ? "LOW " : "HIGH");
		report(label, nsPerSample(N / 4, t0, Clock::now()), sr);
	}

	// ---- static (settled) shape: the common idle case. The lerps
	// converge and the reshape early-out skips its O(n) loops.
	{
		const float sr = 48000.f;
		CyberlineTract t;
		t.setGeometry(17.5f, sr);
		t.setTongue(t.bladeStart + 0.5f * (t.tipStart - t.bladeStart), 2.5f);
		t.applyConstriction();
		float dt = 1.f / sr;
		for (long i = 0; i < sr; i++)   // settle
			t.process(dt, (i & 1) ? 0.2f : -0.2f, 0.f, 0.f, 0.f);
		auto t0 = Clock::now();
		float acc = 0.f;
		for (long i = 0; i < N / 4; i++)
			acc += t.process(dt, (i & 1) ? 0.2f : -0.2f, 0.f, 0.f, 0.1f);
		sink = acc;
		report("Tract static settled (HIGH)", nsPerSample(N / 4, t0, Clock::now()), sr);
	}

	// ---- glottis: HIGH vs LOW quality waveform ----
	for (int low = 0; low < 2; low++) {
		const float sr = 48000.f;
		CyberlineGlottis g;
		g.setSampleRate(sr);
		g.lowQuality = low != 0;
		g.setTargets(140.f, 0.6f, 0.8f, 14.f);
		auto t0 = Clock::now();
		float acc = 0.f;
		for (long i = 0; i < N; i++) acc += g.process(0.3f, 0.f, 0.3f, 6.f);
		sink = acc;
		char label[64];
		std::snprintf(label, sizeof(label), "Glottis::process (%s quality)",
			low ? "LOW " : "HIGH");
		report(label, nsPerSample(N, t0, Clock::now()), sr);
	}
	return 0;
}
