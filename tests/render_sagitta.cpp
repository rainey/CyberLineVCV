// cyberline — CyberlineSagittaCore harness: validates the CV decode, the
// tract-replica response and the telemetry snapshot headlessly.
//
//   g++ -O2 -std=c++11 -o render_sagitta tests/render_sagitta.cpp
//   ./render_sagitta
//
// Checks:
//   1. BUS-mode decode: Broca-style offset channels reproduce absolute
//      tongue positions (front /i/ vs back /ɑ/ profiles differ at the
//      expected sections).
//   2. DIRECT-mode decode: absolute 0-10 V channels are interpreted as-is.
//   3. Velum channel opens the nasal port in the replica.
//   4. Constriction channel narrows the oral profile near the mapped
//      section.
//   5. Voiced sine drives the replica sound-pressure meters + F0 tracker.
//   6. Lungs outputs map into telemetry (volume/flow/pressure).
//   7. Snapshot stays finite under noisy/garbage input.

#include "../src/CyberlineSagittaCore.hpp"
#include <cstdio>
#include <cmath>

using cyberline::CyberlineSagittaCore;
using cyberline::TRACT_SECTIONS_MAX;

static int failures = 0;
static void check(bool ok, const char* label) {
	std::printf("%s %s\n", ok ? "PASS" : "FAIL", label);
	if (!ok) failures++;
}

// Run the core for `seconds` with fixed bus values.
static void run(CyberlineSagittaCore& core, float seconds,
		const float* tb, int tc, const float* lb, int lc,
		float pV, float vV, float fV, float (*voicedGen)(float, void*), void* ctx,
		bool busMode = true, float sr = 44100.f) {
	core.setSampleRate(sr);
	core.reset();
	core.busMode = busMode;
	int total = (int) (seconds * sr);
	float dt = 1.f / sr;
	for (int i = 0; i < total; i++) {
		float t = i * dt;
		float v = voicedGen ? voicedGen(t, ctx) : 0.f;
		core.process(tb, tc, lb, lc, pV, vV, fV, v, dt);
	}
}

static float sineGen(float t, void* ctx) {
	float f = *(float*) ctx;
	return 0.5f * std::sin(2.f * (float) M_PI * f * t);
}

static float zeroGen(float, void*) { return 0.f; }

// max diameter in a section window
static float maxDiam(const CyberlineSagittaCore& core, int lo, int hi) {
	const auto* s = core.snapshot();
	float m = 0.f;
	for (int i = lo; i < hi && i < s->n; i++) m = std::fmax(m, s->diam[i]);
	return m;
}

int main() {
	const float sr = 44100.f;

	// ---- 1. BUS-mode decode: front vs back vowel tongue profiles ----
	{
		CyberlineSagittaCore core;
		// Broca sends (tongueX - 5) V on ch1 and (tongueY - 5) V on ch2.
		// Front vowel /i/: tongueX = 9 -> ch0 = +4 V; tongueY = 2.1 (close).
		float front[9] = {4.f, -2.9f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
		run(core, 0.5f, front, 9, nullptr, 0, 0.f, 0.f, 0.f, zeroGen, nullptr);
		float frontDiam[TRACT_SECTIONS_MAX];
		{
			const auto* s = core.snapshot();
			std::memcpy(frontDiam, s->diam, sizeof(frontDiam));
		}
		float tongueErr = std::fabs(core.tongueX - 9.f) + std::fabs(core.tongueY - 2.1f);
		check(tongueErr < 0.05f && core.snapshot()->alive,
			"BUS decode maps offsets to absolute tongue position");
		// Back vowel /ɑ/: tongueX = 2, tongueY = 9 (open)
		float back[9] = {-3.f, 4.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
		run(core, 0.5f, back, 9, nullptr, 0, 0.f, 0.f, 0.f, zeroGen, nullptr);
		const auto* s2 = core.snapshot();
		// Profiles must differ materially between the two vowels
		float diff = 0.f;
		for (int i = 0; i < s2->n; i++)
			diff = std::fmax(diff, std::fabs(s2->diam[i] - frontDiam[i]));
		check(diff > 0.8f, "front/back vowels produce different tract profiles");
	}

	// ---- 2. DIRECT-mode decode ----
	{
		CyberlineSagittaCore core;
		// absolute tongue X = 9 V, Y = 2 V
		float abs[9] = {9.f, 2.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
		run(core, 0.3f, abs, 9, nullptr, 0, 0.f, 0.f, 0.f, zeroGen, nullptr,
			/*busMode*/ false);
		float err = std::fabs(core.tongueX - 9.f) + std::fabs(core.tongueY - 2.f);
		check(err < 0.05f, "DIRECT decode treats channels as absolute");
	}

	// ---- 3. velum opens nasal port ----
	{
		CyberlineSagittaCore core;
		float closed[9] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
		run(core, 0.3f, closed, 9, nullptr, 0, 0.f, 0.f, 0.f, zeroGen, nullptr);
		const auto* sc = core.snapshot();
		float closedPort = sc->noseDiam[0];
		float openBus[9] = {0.f, 0.f, 10.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f}; // VEL = 10 V
		run(core, 0.5f, openBus, 9, nullptr, 0, 0.f, 0.f, 0.f, zeroGen, nullptr);
		const auto* so = core.snapshot();
		std::printf("  (nasal port closed %.3f -> open %.3f)\n", closedPort, so->noseDiam[0]);
		check(closedPort < 0.1f && so->noseDiam[0] > 0.6f, "velum CV opens the nasal port");
	}

	// ---- 4. constriction narrows the profile ----
	{
		CyberlineSagittaCore core;
		// neutral tongue, constriction at CPOS 7 V (alveolar), width 1 V (near stop)
		float bus[9] = {0.f, 0.f, 0.f, 7.f, -4.f, 0.f, 0.f, 0.f, 0.f};
		run(core, 0.5f, bus, 9, nullptr, 0, 0.f, 0.f, 0.f, zeroGen, nullptr);
		const auto* s = core.snapshot();
		// minimum diameter inside the constriction span (center is blended
		// with the rest profile; the dip bottom sits a couple sections over)
		int center = (int) (s->bladeStart + 0.7f * (s->n - s->bladeStart));
		float atConstr = 5.f;
		for (int i = center - 2; i <= center + 2 && i < s->n; i++)
			atConstr = std::fmin(atConstr, s->diam[i]);
		float away = maxDiam(core, s->bladeStart, s->tipStart - 4);
		std::printf("  (diam min at constr %.3f, widest away %.3f)\n", atConstr, away);
		check(atConstr < 0.3f && away - atConstr > 0.8f, "constriction channel creates a narrow point");
	}

	// ---- 5. voiced sine drives meters + F0 ----
	{
		CyberlineSagittaCore core;
		float freq = 110.f;
		float bus[9] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
		run(core, 1.0f, bus, 9, nullptr, 0, 0.f, 0.f, 0.f, sineGen, &freq);
		const auto* s = core.snapshot();
		float peakAmp = 0.f;
		for (int i = 0; i < s->n; i++) peakAmp = std::fmax(peakAmp, s->amp[i]);
		std::printf("  (F0 %.1f Hz [want ~110], peak amp %.3f)\n", core.f0, peakAmp);
		check(std::fabs(core.f0 - 110.f) < 8.f && peakAmp > 0.05f,
			"voiced sine excites the replica and F0 tracks");
	}

	// ---- 6. lungs outputs map into telemetry ----
	{
		CyberlineSagittaCore core;
		float bus[9] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
		// volume 7 V, pressure 5 V, flow -2.5 V (exhale)
		run(core, 0.3f, bus, 9, nullptr, 0, 5.f, 7.f, -2.5f, zeroGen, nullptr);
		bool ok = std::fabs(core.lungsVol - 0.7f) < 0.01f
			&& std::fabs(core.lungsPress - 0.5f) < 0.01f
			&& std::fabs(core.lungsFlow - (-0.5f)) < 0.01f;
		check(ok, "lungs CVs decode into normalized telemetry");
	}

	// ---- 7. finite under garbage ----
	{
		CyberlineSagittaCore core;
		const float sr2 = 44100.f;
		float dt = 1.f / sr2;
		float garbage[9] = {NAN, INFINITY, -NAN, 1e30f, -1e30f, NAN, 0.f, 0.f, 0.f};
		float lb[3] = {NAN, INFINITY, -NAN};
		for (int i = 0; i < (int) (0.5f * sr2); i++) {
			float t = i * dt;
			core.process(garbage, 9, lb, 3,
				NAN, INFINITY, -INFINITY,
				std::sin(120.f * t) * 1e30f, dt);
		}
		const auto* s = core.snapshot();
		bool finite = s->alive;
		for (int i = 0; i < s->n && finite; i++)
			finite = std::isfinite(s->diam[i]) && std::isfinite(s->amp[i]);
		for (int i = 0; i < s->noseLength && finite; i++)
			finite = std::isfinite(s->noseDiam[i]);
		check(finite, "snapshot stays finite under garbage input");
	}

	std::printf("%s (%d failures)\n", failures ? "FAILURES" : "ALL PASS", failures);
	return failures ? 1 : 0;
}
