// cyberline — automated module-level integration tests.
// Tests CyberlineLungs, CyberlineBroca, and their poly-bus normalling
// into CyberlineOralChamber, CyberlineLarynx, and CyberlineVoice.
//
// Build & run:
//   RACK=/tmp/opencode/rack241/Rack2Free
//   D=../JayBird/src/Rack/dep
//   g++ -O2 -std=c++11 -march=nehalem -DARCH_X64 -DARCH_LIN \
//     -I$D/nanovg/src -I$D/nanosvg/src -I$D/glfw/include -I$D/oui-blendish -I$D/pffft \
//     -I../JayBird/src/Rack/include -I$D/include \
//     -o /tmp/test_modules tests/test_modules.cpp \
//     src/CyberlineLungs.cpp src/CyberlineBroca.cpp src/CyberlineOralChamber.cpp \
//     src/CyberlineLarynx.cpp src/CyberlineVoice.cpp \
//     -L$RACK -lRack -Wl,-rpath,$RACK
//   /tmp/test_modules

#include "../src/CyberlineLungs.hpp"
#include "../src/CyberlineBroca.hpp"
#include "../src/CyberlineBrocaSq.hpp"
#include "../src/CyberlineOralChamber.hpp"
#include "../src/CyberlineLarynx.hpp"
#include "../src/CyberlineVoice.hpp"
#include "../src/CyberlineQuality.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <string>

using namespace cyberline;

static int g_passCount = 0;
static int g_failCount = 0;

#define CHECK(cond, msg) do { \
	if (cond) { \
		g_passCount++; \
	} else { \
		g_failCount++; \
		std::fprintf(stderr, "FAIL: %s (%s:%d)\n", msg, __FILE__, __LINE__); \
	} \
} while (0)

static void test_lungs_singer_cycle() {
	std::printf("--- Testing CyberlineLungs: singer breathing pattern ---\n");
	CyberlineLungs lungs;
	rack::engine::Module::ProcessArgs args;
	args.sampleRate = 48000.f;
	args.sampleTime = 1.f / 48000.f;

	// Nominal singer breathing: 12 bpm (5.0s period), 0.4s quick inhale, 0.65 effort
	lungs.params[CyberlineLungs::RATE_PARAM].setValue(12.f);
	lungs.params[CyberlineLungs::DEPTH_PARAM].setValue(0.7f);
	lungs.params[CyberlineLungs::INHALE_PARAM].setValue(0.4f);
	lungs.params[CyberlineLungs::MAXFLOW_PARAM].setValue(0.4f);
	lungs.params[CyberlineLungs::EFFORT_PARAM].setValue(0.65f);
	lungs.params[CyberlineLungs::MODE_PARAM].setValue((float)CyberlineLungs::CYCLE);
	lungs.params[CyberlineLungs::INHMUTE_PARAM].setValue(1.0f); // Mute on inhale

	// Run for 5.0 seconds (one full cycle)
	int totalSamples = (int)(5.0f * args.sampleRate);
	float peakVolume = 0.f;
	int peakSample = 0;
	float minInhalePressure = 999.f;
	float maxExhalePressure = -999.f;
	float endVolume = 0.f;

	for (int i = 0; i < totalSamples; i++) {
		lungs.process(args);
		float v = lungs.outputs[CyberlineLungs::VOLUME_OUTPUT].getVoltage() * 0.1f;
		float p = lungs.outputs[CyberlineLungs::PRESSURE_OUTPUT].getVoltage();

		if (v > peakVolume) {
			peakVolume = v;
			peakSample = i;
		}
		if (lungs.inhaling) {
			if (p < minInhalePressure) minInhalePressure = p;
		}
		if (lungs.exhaling) {
			if (p > maxExhalePressure) maxExhalePressure = p;
		}
		endVolume = v;
	}

	float inhaleTime = peakSample * args.sampleTime;
	float exhaleTime = 5.0f - inhaleTime;

	std::printf("  Peak volume: %.3f at t = %.3f s, Exhale duration = %.3f s\n",
		peakVolume, inhaleTime, exhaleTime);
	std::printf("  Min inhale pressure: %.3f V (INHMUTE=1), Max exhale pressure: %.3f V\n",
		minInhalePressure, maxExhalePressure);

	CHECK(peakVolume >= 0.65f && peakVolume <= 0.75f, "Peak tidal volume reaches configured depth (~0.7)");
	CHECK(inhaleTime >= 0.35f && inhaleTime <= 0.45f, "Inhale time matches 0.4s configured time");
	CHECK(exhaleTime >= 4.0f, "Singer exhale is much longer than inhale (> 4.0s vs 0.4s)");
	CHECK(minInhalePressure >= 0.0f, "INHMUTE=1 mutes negative suction pressure (>= 0 V)");
	CHECK(maxExhalePressure >= 5.0f, "Singer effort produces strong regulated subglottic pressure");

	// Now test INHMUTE=0 (Suction gasp enabled)
	lungs.reset();
	lungs.params[CyberlineLungs::INHMUTE_PARAM].setValue(0.0f); // Suction
	minInhalePressure = 999.f;
	for (int i = 0; i < (int)(0.4f * args.sampleRate); i++) {
		lungs.process(args);
		float p = lungs.outputs[CyberlineLungs::PRESSURE_OUTPUT].getVoltage();
		if (p < minInhalePressure) minInhalePressure = p;
	}
	std::printf("  Min inhale pressure with INHMUTE=0: %.3f V\n", minInhalePressure);
	CHECK(minInhalePressure < -0.5f, "INHMUTE=0 produces negative suction pressure during inhale");

	// Test proportional inhale ratio at different rate (24 bpm -> 2.5s period -> ~0.20s inhale)
	lungs.reset();
	lungs.params[CyberlineLungs::RATE_PARAM].setValue(24.f);
	lungs.params[CyberlineLungs::DEPTH_PARAM].setValue(0.7f);
	lungs.params[CyberlineLungs::INHALE_PARAM].setValue(0.4f);
	lungs.params[CyberlineLungs::EFFORT_PARAM].setValue(0.65f);
	peakVolume = 0.f;
	peakSample = 0;
	for (int i = 0; i < (int)(2.5f * args.sampleRate); i++) {
		lungs.process(args);
		float v = lungs.outputs[CyberlineLungs::VOLUME_OUTPUT].getVoltage() * 0.1f;
		if (v > peakVolume) {
			peakVolume = v;
			peakSample = i;
		}
	}
	float fastInhaleTime = peakSample * args.sampleTime;
	std::printf("  At 24 bpm (2.5s period), inhale duration: %.3f s\n", fastInhaleTime);
	CHECK(fastInhaleTime >= 0.16f && fastInhaleTime <= 0.24f, "Inhale time scales proportionally with cycle period at higher rates");
}

static void test_lungs_sync_and_gate() {
	std::printf("--- Testing CyberlineLungs: SYNC reset and GATE hold ---\n");
	CyberlineLungs lungs;
	rack::engine::Module::ProcessArgs args;
	args.sampleRate = 48000.f;
	args.sampleTime = 1.f / 48000.f;

	lungs.params[CyberlineLungs::RATE_PARAM].setValue(10.f); // 6.0s period
	lungs.params[CyberlineLungs::DEPTH_PARAM].setValue(0.8f);
	lungs.params[CyberlineLungs::INHALE_PARAM].setValue(0.5f);
	lungs.params[CyberlineLungs::MODE_PARAM].setValue((float)CyberlineLungs::CYCLE);

	// Run 2.0s into the cycle (well into exhale phase)
	for (int i = 0; i < (int)(2.0f * args.sampleRate); i++) {
		lungs.process(args);
	}
	CHECK(lungs.exhaling == true, "Lungs are exhaling at t=2.0s");

	// Send SYNC pulse
	lungs.inputs[CyberlineLungs::SYNC_INPUT].setVoltage(10.f);
	for (int i = 0; i < 48; i++) lungs.process(args); // 1 ms pulse
	lungs.inputs[CyberlineLungs::SYNC_INPUT].setVoltage(0.f);

	// Process 100 samples to let physics tick
	for (int i = 0; i < 100; i++) lungs.process(args);

	CHECK(lungs.inhaling == true, "SYNC trigger hard-resets lungs to inhale immediately");
	CHECK(lungs.exhaling == false, "SYNC trigger clears exhale state");
	CHECK(lungs.volume < 0.2f, "Volume was reset to near 0 on sync");

	// Let inhale finish (~0.6s at 10 bpm)
	for (int i = 0; i < (int)(0.7f * args.sampleRate); i++) {
		lungs.process(args);
	}
	CHECK(lungs.exhaling == true, "Lungs transitioned to exhale after inhale completed");
	float volBeforeHold = lungs.volume;

	// Hold breath with GATE_INPUT = 10V for 1.0 second
	lungs.inputs[CyberlineLungs::GATE_INPUT].setVoltage(10.f);
	for (int i = 0; i < (int)(1.0f * args.sampleRate); i++) {
		lungs.process(args);
	}
	float volAfterHold = lungs.volume;
	std::printf("  Volume before hold: %.3f, after 1.0s gate hold: %.3f\n", volBeforeHold, volAfterHold);
	CHECK(std::fabs(volAfterHold - volBeforeHold) < 0.02f, "Gate high holds breath (volume remains constant)");

	// Release gate -> exhale should continue
	lungs.inputs[CyberlineLungs::GATE_INPUT].setVoltage(0.f);
	for (int i = 0; i < (int)(2.0f * args.sampleRate); i++) {
		lungs.process(args);
	}
	CHECK(lungs.volume < volAfterHold, "Releasing gate allows exhale drain to resume");
}

static void test_lungs_exhale_first_polarity() {
	std::printf("--- Testing CyberlineLungs: exhale-first cycle polarity ---\n");
	CyberlineLungs lungs;
	rack::engine::Module::ProcessArgs args;
	args.sampleRate = 48000.f;
	args.sampleTime = 1.f / 48000.f;

	lungs.params[CyberlineLungs::RATE_PARAM].setValue(12.f); // 5.0s period
	lungs.params[CyberlineLungs::DEPTH_PARAM].setValue(0.8f);
	lungs.params[CyberlineLungs::INHALE_PARAM].setValue(0.5f);
	lungs.params[CyberlineLungs::MAXFLOW_PARAM].setValue(1.f); // fast vent: drain ~1.7s
	lungs.params[CyberlineLungs::MODE_PARAM].setValue((float)CyberlineLungs::CYCLE);
	lungs.params[CyberlineLungs::POLARITY_PARAM].setValue(1.f); // Exhale first

	// SYNC pulse: the cycle must begin on the exhale — full lungs releasing
	// on the beat, not a silent inhale
	lungs.inputs[CyberlineLungs::SYNC_INPUT].setVoltage(10.f);
	for (int i = 0; i < 48; i++) lungs.process(args); // 1 ms pulse
	lungs.inputs[CyberlineLungs::SYNC_INPUT].setVoltage(0.f);
	for (int i = 0; i < 100; i++) lungs.process(args);

	CHECK(lungs.exhaling == true, "Exhale-first SYNC begins the cycle with the exhale");
	CHECK(lungs.inhaling == false, "Exhale-first SYNC does not inhale first");
	CHECK(lungs.volume > 0.7f, "Exhale-first SYNC tops up the reservoir to full");
	CHECK(lungs.outputs[CyberlineLungs::PHASE_OUTPUT].getVoltage() > 2.5f,
		"PHASE gate is high at the cycle boundary in exhale-first");
	CHECK(lungs.outputs[CyberlineLungs::PRESSURE_OUTPUT].getVoltage() > 1.f,
		"PRESS is active immediately at the cycle boundary in exhale-first");

	// Run past the exhale drain: the reservoir empties and the module
	// recharges by inhaling (scan for it — drain duration depends on knobs)
	bool sawRecharge = false;
	float t = 0.f;
	for (int i = 0; i < (int)(4.5f * args.sampleRate); i++) {
		lungs.process(args);
		t += args.sampleTime;
		if (t > 1.5f && lungs.inhaling) { sawRecharge = true; break; }
	}
	CHECK(sawRecharge, "After the drain, exhale-first recharges by inhaling");

	// After the recharge completes, the module holds at the top (no
	// immediate re-exhale — the exhale waits for the next cycle boundary)
	for (int i = 0; i < (int)(1.0f * args.sampleRate); i++) lungs.process(args);
	CHECK(!lungs.inhaling && !lungs.exhaling && lungs.volume > 0.7f,
		"Recharge completes to a top hold, not an immediate re-exhale");
	CHECK(lungs.outputs[CyberlineLungs::PHASE_OUTPUT].getVoltage() < 0.5f,
		"PHASE gate is low during the top hold (waiting for the boundary)");

	// Crossing the next cycle boundary restarts the exhale from full
	// (scan for it — the hold lasts until the 5 s boundary)
	float volAtHold = 0.f;
	bool boundaryRestart = false;
	for (int i = 0; i < (int)(4.5f * args.sampleRate); i++) {
		lungs.process(args);
		if (lungs.exhaling) { boundaryRestart = true; break; }
	}
	CHECK(boundaryRestart, "Cycle boundary restarts the exhale in exhale-first");
	// Let the drain run a moment, then confirm it is venting from the top-up
	for (int i = 0; i < (int)(0.5f * args.sampleRate); i++) lungs.process(args);
	CHECK(lungs.volume < 0.75f && lungs.exhaling,
		"Exhale drains from the full top-up at the boundary");

	// Switching back to inhale-first must restore classic behavior
	lungs.params[CyberlineLungs::POLARITY_PARAM].setValue(0.f);
	lungs.inputs[CyberlineLungs::SYNC_INPUT].setVoltage(10.f);
	for (int i = 0; i < 48; i++) lungs.process(args);
	lungs.inputs[CyberlineLungs::SYNC_INPUT].setVoltage(0.f);
	for (int i = 0; i < 100; i++) lungs.process(args);
	CHECK(lungs.inhaling == true && lungs.volume < 0.2f,
		"Inhale-first SYNC still resets to a silent fill");
}

static void test_lungs_phasor_ratio() {
	std::printf("--- Testing CyberlineLungs: PHASOR I:E ratio follows INHALE knob ---\n");
	rack::engine::Module::ProcessArgs args;
	args.sampleRate = 48000.f;
	args.sampleTime = 1.f / 48000.f;

	// Feed one full 0->10 V saw cycle over 5 s and measure what fraction
	// of the cycle spends inhaling. The split must follow the INHALE knob
	// (INHALE/5 s, the same 12-bpm reference as CYCLE mode), not the old
	// fixed 50/50 mapping.
	struct Case { float inhale; float polarity; float expect; const char* name; };
	Case cases[] = {
		{0.4f, 0.f, 0.08f, "inhale-first, INHALE 0.4s -> 8%"},
		{2.0f, 0.f, 0.40f, "inhale-first, INHALE 2.0s -> 40%"},
		{0.4f, 1.f, 0.08f, "exhale-first, INHALE 0.4s -> 8%"},
		{3.5f, 0.f, 0.70f, "inhale-first, INHALE 3.5s -> 70% (no 45% cap in PHASOR)"},
	};
	for (auto& c : cases) {
		CyberlineLungs lungs;
		lungs.params[CyberlineLungs::DEPTH_PARAM].setValue(0.8f);
		lungs.params[CyberlineLungs::INHALE_PARAM].setValue(c.inhale);
		lungs.params[CyberlineLungs::MODE_PARAM].setValue((float)CyberlineLungs::PHASOR);
		lungs.params[CyberlineLungs::POLARITY_PARAM].setValue(c.polarity);

		int totalSamples = (int)(5.0f * args.sampleRate);
		int inhaleTicks = 0, totalTicks = 0;
		for (int i = 0; i < totalSamples; i++) {
			// 0 -> 10 V linear saw (the cycle under test)
			lungs.inputs[CyberlineLungs::GATE_INPUT].setVoltage(
				10.f * (float)i / (float)totalSamples);
			lungs.process(args);
			if (i % 32 == 0) { // control-rate tick census
				totalTicks++;
				if (lungs.inhaling) inhaleTicks++;
			}
		}
		float frac = (float)inhaleTicks / (float)totalTicks;
		std::printf("  %s: measured %.3f\n", c.name, frac);
		// Endpoints of the ramp sit at bucket edges; 0.03 tolerance is generous
		CHECK(std::fabs(frac - c.expect) < 0.03f, c.name);
	}

	// Exhale-first mirror check: with a saw, the drain must begin at
	// phase 0 — just after the boundary the lungs are still near full
	// and actively draining (a held constant phase is a no-flow hold,
	// so probe while the saw is moving)
	CyberlineLungs lungs;
	lungs.params[CyberlineLungs::DEPTH_PARAM].setValue(0.8f);
	lungs.params[CyberlineLungs::INHALE_PARAM].setValue(0.4f);
	lungs.params[CyberlineLungs::MODE_PARAM].setValue((float)CyberlineLungs::PHASOR);
	lungs.params[CyberlineLungs::POLARITY_PARAM].setValue(1.f);
	int totalSamples = (int)(5.0f * args.sampleRate);
	float volAfterBoundary = 0.f;
	for (int i = 0; i <= (int)(0.1f * args.sampleRate); i++) {
		lungs.inputs[CyberlineLungs::GATE_INPUT].setVoltage(
			10.f * (float)i / (float)totalSamples);
		lungs.process(args);
	}
	volAfterBoundary = lungs.volume;
	CHECK(lungs.exhaling, "Exhale-first PHASOR drains right after the phase-0 boundary");
	CHECK(volAfterBoundary > 0.7f && volAfterBoundary < 0.795f,
		"Exhale-first PHASOR starts the drain from full at the boundary");
	CHECK(lungs.outputs[CyberlineLungs::PHASE_OUTPUT].getVoltage() > 2.5f,
		"Exhale-first PHASOR raises PHASE at the phase-0 boundary");
}

static void test_broca_channels_and_vowels() {
	std::printf("--- Testing CyberlineBroca: buses and vowel morphing ---\n");
	CyberlineBroca broca;
	rack::engine::Module::ProcessArgs args;
	args.sampleRate = 48000.f;
	args.sampleTime = 1.f / 48000.f;

	// In Rack's Port model, an output must have a connected cable before setChannels() takes effect
	broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].channels = 1;
	broca.outputs[CyberlineBroca::LARYNX_POLY_OUTPUT].channels = 1;

	// Process one frame to initialize
	broca.process(args);

	CHECK(broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].getChannels() == 9,
		"TRACT_POLY_OUTPUT produces exactly 9 polyphonic channels");
	CHECK(broca.outputs[CyberlineBroca::LARYNX_POLY_OUTPUT].getChannels() == 3,
		"LARYNX_POLY_OUTPUT produces exactly 3 polyphonic channels");

	// Vowel A: EE (/i/, front high: tongueX=8.5, tongueY=2.1)
	// Vowel B: OO (/u/, back high: tongueX=3.0, tongueY=2.1)
	broca.params[CyberlineBroca::VOWELA_PARAM].setValue(0.f); // EE
	broca.params[CyberlineBroca::VOWELB_PARAM].setValue(4.f); // OO
	broca.params[CyberlineBroca::VMORPH_PARAM].setValue(0.f); // Pure EE

	// Run 0.1s to allow 14ms slew filter to fully settle
	for (int i = 0; i < (int)(0.1f * args.sampleRate); i++) broca.process(args);
	float eeTongueX = broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].getVoltage(0);

	broca.params[CyberlineBroca::VMORPH_PARAM].setValue(1.f); // Pure OO
	for (int i = 0; i < (int)(0.1f * args.sampleRate); i++) broca.process(args);
	float ooTongueX = broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].getVoltage(0);

	std::printf("  Vowel EE TongueX offset: %.3f V, Vowel OO TongueX offset: %.3f V\n",
		eeTongueX, ooTongueX);
	CHECK(eeTongueX > ooTongueX + 4.0f, "Front vowel /i/ has significantly higher TongueX than back vowel /u/");

	// Test voicing channel on steady-state vowels
	float steadyVoicing = broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].getVoltage(8);
	float steadyLarynxVoicing = broca.outputs[CyberlineBroca::LARYNX_POLY_OUTPUT].getVoltage(2);
	CHECK(steadyVoicing >= 9.9f, "Vowels have full 10V voicing on TRACT POLY Ch 9");
	CHECK(steadyLarynxVoicing >= 9.9f, "Vowels have full 10V voicing on LARYNX POLY Ch 3");

	// Test Vowel B CV modulation
	broca.inputs[CyberlineBroca::VOWELB_INPUT].channels = 1;
	broca.params[CyberlineBroca::VOWELB_PARAM].setValue(0.f);
	broca.inputs[CyberlineBroca::VOWELB_INPUT].setVoltage(5.0f);
	for (int i = 0; i < 200; i++) broca.process(args);
	CHECK(broca.activeVowelB > 0, "VOWEL B CV modulates active Vowel B target index");
	broca.inputs[CyberlineBroca::VOWELB_INPUT].setVoltage(0.f);
}

static void test_broca_consonants_and_sync() {
	std::printf("--- Testing CyberlineBroca: consonants and manners ---\n");
	CyberlineBroca broca;
	rack::engine::Module::ProcessArgs args;
	args.sampleRate = 48000.f;
	args.sampleTime = 1.f / 48000.f;

	broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].channels = 1;
	broca.outputs[CyberlineBroca::LARYNX_POLY_OUTPUT].channels = 1;

	// Settle
	for (int i = 0; i < 100; i++) broca.process(args);

	// Consonant /t/ (voiceless alveolar stop)
	int tIndex = -1;
	for (int c = 0; c < CyberlineBroca::NUM_CONSONANTS; c++) {
		if (std::string(CyberlineBroca::CONSONANTS[c].code) == "T") {
			tIndex = c;
			break;
		}
	}
	CHECK(tIndex >= 0, "Found consonant /T/ in table");
	broca.params[CyberlineBroca::CONSONANT_PARAM].setValue((float)tIndex);
	broca.params[CyberlineBroca::ONSET_PARAM].setValue(0.08f); // 80 ms onset

	// Trigger consonant onset
	broca.inputs[CyberlineBroca::TRIG_INPUT].setVoltage(10.f);
	broca.process(args);
	broca.inputs[CyberlineBroca::TRIG_INPUT].setVoltage(0.f);

	float minVoicing = 10.f;
	float minConstrWidth = 10.f;
	float maxBurst = 0.f;

	for (int i = 0; i < (int)(0.12f * args.sampleRate); i++) {
		broca.process(args);
		float v = broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].getVoltage(8);
		float cw = broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].getVoltage(4);
		float burst = broca.outputs[CyberlineBroca::BURST_OUTPUT].getVoltage();
		if (v < minVoicing) minVoicing = v;
		if (cw < minConstrWidth) minConstrWidth = cw;
		if (burst > maxBurst) maxBurst = burst;
	}

	std::printf("  /T/ Stop: min voicing = %.2f V, min constriction width offset = %.2f V, max burst = %.2f V\n",
		minVoicing, minConstrWidth, maxBurst);
	CHECK(minVoicing < 1.0f, "Voiceless stop /T/ cuts voicing down to ~0 V");
	CHECK(minConstrWidth <= -4.0f, "Stop closure narrows constriction width (-5V offset)");
	CHECK(maxBurst >= 5.0f, "Stop release triggers plosive release BURST output (> 5V)");

	// Test Nasal /M/ (voiced bilabial nasal)
	int mIndex = -1;
	for (int c = 0; c < CyberlineBroca::NUM_CONSONANTS; c++) {
		if (std::string(CyberlineBroca::CONSONANTS[c].code) == "M") {
			mIndex = c;
			break;
		}
	}
	CHECK(mIndex >= 0, "Found consonant /M/ in table");
	broca.params[CyberlineBroca::CONSONANT_PARAM].setValue((float)mIndex);

	broca.inputs[CyberlineBroca::TRIG_INPUT].setVoltage(10.f);
	broca.process(args);
	broca.inputs[CyberlineBroca::TRIG_INPUT].setVoltage(0.f);

	float maxVelum = 0.f;
	minVoicing = 10.f;
	for (int i = 0; i < (int)(0.1f * args.sampleRate); i++) {
		broca.process(args);
		float vel = broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].getVoltage(2);
		float v = broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].getVoltage(8);
		if (vel > maxVelum) maxVelum = vel;
		if (v < minVoicing) minVoicing = v;
	}
	std::printf("  /M/ Nasal: max velum = %.2f V, min voicing = %.2f V\n", maxVelum, minVoicing);
	CHECK(maxVelum >= 8.0f, "Nasal /M/ opens velum port (> 8V on Ch 3)");
	CHECK(minVoicing >= 8.0f, "Voiced nasal /M/ maintains vocal fold vibration (voicing >= 8V)");

	// Test SYNC input
	broca.scanStep = 7;
	broca.inputs[CyberlineBroca::SYNC_INPUT].setVoltage(10.f);
	broca.process(args);
	broca.inputs[CyberlineBroca::SYNC_INPUT].setVoltage(0.f);
	broca.process(args);
	CHECK(broca.scanStep == 0, "SYNC input resets Broca scanStep to 0");
}

static void test_poly_normalling_oral_chamber() {
	std::printf("--- Testing Poly Normalling: Broca -> Oral Chamber ---\n");
	CyberlineBroca broca;
	CyberlineOralChamber chamber;
	rack::engine::Module::ProcessArgs args;
	args.sampleRate = 48000.f;
	args.sampleTime = 1.f / 48000.f;

	broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].channels = 1;
	broca.outputs[CyberlineBroca::LARYNX_POLY_OUTPUT].channels = 1;

	// Initial step
	broca.process(args);

	// Patch Broca TRACT POLY to Oral Chamber TONGUEX_INPUT (TNGX/POLY)
	auto patchTractBus = [&]() {
		int n = broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].getChannels();
		chamber.inputs[CyberlineOralChamber::TONGUEX_INPUT].channels = n;
		for (int c = 0; c < n; c++) {
			chamber.inputs[CyberlineOralChamber::TONGUEX_INPUT].setVoltage(
				broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].getVoltage(c), c);
		}
	};

	// Test VTL CV modulation on Oral Chamber
	chamber.params[CyberlineOralChamber::VTL_PARAM].setValue(17.5f);
	chamber.inputs[CyberlineOralChamber::VTL_INPUT].channels = 1;
	chamber.inputs[CyberlineOralChamber::VTL_INPUT].setVoltage(0.f);
	for (int i = 0; i < 32; i++) chamber.process(args);
	CHECK(std::fabs(chamber.vtlCm - 17.5f) < 0.1f, "VTL default is 17.5 cm");

	// Add 1.0V CV to VTL input (scaled +5.4 cm/V -> 22.9 cm).
	// The CV is slewed with a 5 ms one-pole (pop prevention); run long
	// enough for it to settle before checking the converged length.
	chamber.inputs[CyberlineOralChamber::VTL_INPUT].setVoltage(1.0f);
	for (int i = 0; i < 44100 / 20; i++) chamber.process(args);
	std::printf("  Oral Chamber VTL at 1V CV (settled): %.2f cm\n", chamber.vtlCm);
	CHECK(std::fabs(chamber.vtlCm - 22.9f) < 0.2f, "VTL CV modulates effective tract length to 22.9 cm");
	chamber.inputs[CyberlineOralChamber::VTL_INPUT].setVoltage(0.f);
	for (int i = 0; i < 44100 / 20; i++) chamber.process(args);

	// LOW quality mode: finite output with no discontinuities while the
	// tongue articulates and the VTL sweeps (exercises the block-rate
	// reflection fade, the state resample and the subnormal flush).
	chamber.params[CyberlineOralChamber::QUALITY_PARAM].setValue(1.f);
	chamber.inputs[CyberlineOralChamber::SOURCE_INPUT].channels = 1;
	bool lowFinite = true;
	float lowMaxJump = 0.f, lowPrev = 0.f;
	int lowN = (int) (0.4f * args.sampleRate);
	for (int i = 0; i < lowN; i++) {
		if ((i & 15) == 0) {
			float ph = (float) (i % (lowN / 2)) / (float) (lowN / 2);
			chamber.params[CyberlineOralChamber::TONGUEX_PARAM].setValue(
				5.f + 4.f * std::sin(2.f * (float) M_PI * ph));
			chamber.params[CyberlineOralChamber::TONGUEY_PARAM].setValue(
				4.f + 3.f * std::sin(2.f * (float) M_PI * 0.7f * ph));
			chamber.params[CyberlineOralChamber::VTL_PARAM].setValue(
				14.f + 4.f * std::sin(2.f * (float) M_PI * 0.3f * ph));
		}
		chamber.inputs[CyberlineOralChamber::SOURCE_INPUT].setVoltage(
			2.5f * std::sin(2.f * (float) M_PI * 110.f * i * args.sampleTime));
		chamber.process(args);
		float lip = chamber.outputs[CyberlineOralChamber::LIPS_OUTPUT].getVoltage();
		if (!std::isfinite(lip)) lowFinite = false;
		float j = std::fabs(lip - lowPrev);
		if (i > 100 && j > lowMaxJump) lowMaxJump = j;
		lowPrev = lip;
	}
	std::printf("  LOW quality: maxJump %.3f V finite %s\n", lowMaxJump,
		lowFinite ? "yes" : "NO");
	CHECK(lowFinite && lowMaxJump < 6.f, "LOW quality mode stays finite without pops");
	chamber.params[CyberlineOralChamber::QUALITY_PARAM].setValue(2.f);

	// Set Broca to nasal /M/ and trigger
	int mIndex = -1;
	for (int c = 0; c < CyberlineBroca::NUM_CONSONANTS; c++) {
		if (std::string(CyberlineBroca::CONSONANTS[c].code) == "M") {
			mIndex = c;
			break;
		}
	}
	broca.params[CyberlineBroca::CONSONANT_PARAM].setValue((float)mIndex);
	broca.inputs[CyberlineBroca::TRIG_INPUT].setVoltage(10.f);
	broca.process(args);
	broca.inputs[CyberlineBroca::TRIG_INPUT].setVoltage(0.f);

	float maxNoseOut = 0.f;
	// Feed a test excitation into Chamber's SOURCE_INPUT
	chamber.inputs[CyberlineOralChamber::SOURCE_INPUT].channels = 1;
	for (int i = 0; i < (int)(0.08f * args.sampleRate); i++) {
		broca.process(args);
		patchTractBus();

		// Glottal impulse
		float src = (i % 200 == 0) ? 2.0f : 0.0f;
		chamber.inputs[CyberlineOralChamber::SOURCE_INPUT].setVoltage(src);
		chamber.process(args);

		float nose = std::fabs(chamber.outputs[CyberlineOralChamber::NOSE_OUTPUT].getVoltage());
		if (nose > maxNoseOut) maxNoseOut = nose;
	}
	std::printf("  Oral Chamber nose radiation during Broca /M/: %.4f V\n", maxNoseOut);
	CHECK(maxNoseOut > 0.05f, "Broca TRACT POLY Ch 2 (Velum) correctly normalled into Chamber and opened nasal tract");
}

// Global DSP quality: default LOW, per-module override resolution.
static void test_quality_global_and_override() {
	std::printf("--- Testing DSP quality: global default + per-module override ---\n");
	CyberlineOralChamber chamber;
	rack::engine::Module::ProcessArgs args;
	args.sampleRate = 48000.f;
	args.sampleTime = 1.f / 48000.f;

	bool globalWas = CyberlineQuality::getGlobal();
	CyberlineQuality::setGlobal(true);   // LOW (also the factory default)

	// The modules re-read the override param on their 16-sample block
	// cadence, so run a block of samples after each change.
	auto runBlock = [&]() {
		for (int i = 0; i < 32; i++) chamber.process(args);
	};

	// FOLLOW (default param 2) resolves to the global
	chamber.params[CyberlineOralChamber::QUALITY_PARAM].setValue(2.f);
	runBlock();
	CHECK(chamber.tract.lowQuality == true,
		"FOLLOW mode resolves to the global quality (default LOW)");

	// pinned HIGH wins over a LOW global
	chamber.params[CyberlineOralChamber::QUALITY_PARAM].setValue(0.f);
	runBlock();
	CHECK(chamber.tract.lowQuality == false,
		"pinned HIGH overrides the global LOW");

	// pinned LOW
	chamber.params[CyberlineOralChamber::QUALITY_PARAM].setValue(1.f);
	runBlock();
	CHECK(chamber.tract.lowQuality == true, "pinned LOW stays LOW");

	// a HIGH global reaches FOLLOW-mode modules...
	CyberlineQuality::setGlobal(false);
	chamber.params[CyberlineOralChamber::QUALITY_PARAM].setValue(2.f);
	runBlock();
	CHECK(chamber.tract.lowQuality == false,
		"HIGH global reaches FOLLOW-mode modules");

	// ...and a LOW global switches them back
	CyberlineQuality::setGlobal(true);
	runBlock();
	CHECK(chamber.tract.lowQuality == true,
		"LOW global reaches FOLLOW-mode modules");

	// Voice resolves both cores through the same override param
	CyberlineVoice voice;
	voice.params[CyberlineVoice::V_QUALITY_PARAM].setValue(0.f);
	for (int i = 0; i < 32; i++) voice.process(args);
	CHECK(!voice.tract.lowQuality && !voice.glottis.lowQuality,
		"Voice pinned HIGH applies to tract and glottis");
	voice.params[CyberlineVoice::V_QUALITY_PARAM].setValue(2.f);
	for (int i = 0; i < 32; i++) voice.process(args);
	CHECK(voice.tract.lowQuality && voice.glottis.lowQuality,
		"Voice FOLLOW mode applies the global (LOW) to tract and glottis");

	// restore the pre-test global so later tests see the default state
	CyberlineQuality::setGlobal(globalWas);
}

// Settled-shape reshape early-out: bit-exact against the always-run path.
static void test_tract_settled_early_out_bitexact() {
	std::printf("--- Testing Tract settled-shape early-out (bit-exact) ---\n");
	const float sr = 44100.f;
	const int settleN = (int) (1.0f * sr);    // let the lerps converge
	const int runN = (int) (1.0f * sr);       // then compare A/B

	CyberlineTract a, b;
	for (int pass = 0; pass < 2; pass++) {
		CyberlineTract& t = pass ? b : a;
		t.setGeometry(17.5f, sr);
		t.setTongue(t.bladeStart + 0.4f * (t.tipStart - t.bladeStart), 3.f);
		t.applyConstriction();
	}
	float dt = 1.f / sr;
	// settle both identically
	for (int i = 0; i < settleN; i++) {
		float s = 0.4f * std::sin(2.f * (float) M_PI * 110.f * i / sr);
		a.process(dt, s, 0.f, 0.f, 0.f);
		b.process(dt, s, 0.f, 0.f, 0.f);
	}
	// A: force the full reshape path every sample (reference behavior)
	// B: normal (early-out engages once settled)
	bool identical = true;
	int earlyOuts = 0;
	for (int i = 0; i < runN; i++) {
		if ((i & 15) == 0) {
			// small block-rate target wiggle (like a slow CV), then stillness
			if ((i & 31) < 16) {
				float tx = 0.45f + 0.02f * (float) ((i >> 4) & 1);
				a.setTongue(a.bladeStart + tx * (a.tipStart - a.bladeStart), 3.f);
				b.setTongue(b.bladeStart + tx * (b.tipStart - b.bladeStart), 3.f);
				a.applyConstriction();
				b.applyConstriction();
			}
		}
		a.targetsDirty = true;   // disable the early-out for A only
		float s = 0.4f * std::sin(2.f * (float) M_PI * 110.f * i / sr)
			+ 0.05f * std::sin(2.f * (float) M_PI * 777.f * i / sr);
		float na = std::sin(0.f);   // (noise deterministic: none)
		float la = a.process(dt, s, 0.f, 0.f, na);
		float lb = b.process(dt, s, 0.f, 0.f, na);
		if (b.targetsDirty == false && b.shapeMoved == false)
			earlyOuts++;
		if (std::memcmp(&la, &lb, sizeof(float)) != 0
			|| std::memcmp(&a.lastNoseOutput, &b.lastNoseOutput,
				sizeof(float)) != 0) {
			identical = false;
			std::printf("  first mismatch at sample %d: %.9g vs %.9g\n",
				i, la, lb);
			break;
		}
	}
	std::printf("  %d samples compared, early-out engaged on %d\n",
		runN, earlyOuts);
	CHECK(earlyOuts > 0, "settled early-out actually engages");
	CHECK(identical, "early-out path is bit-exact vs full reshape path");
}

static void test_poly_normalling_larynx_and_voice() {
	std::printf("--- Testing Poly Normalling: Broca -> Larynx & Voice ---\n");
	CyberlineBroca broca;
	CyberlineLarynx larynx;
	CyberlineVoice voice;
	rack::engine::Module::ProcessArgs args;
	args.sampleRate = 48000.f;
	args.sampleTime = 1.f / 48000.f;

	broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].channels = 1;
	broca.outputs[CyberlineBroca::LARYNX_POLY_OUTPUT].channels = 1;

	broca.process(args);

	// Patch Broca LARYNX_POLY_OUTPUT to Larynx TENSION_INPUT (TENS/POLY)
	auto patchLarynxBus = [&]() {
		int n = broca.outputs[CyberlineBroca::LARYNX_POLY_OUTPUT].getChannels();
		larynx.inputs[CyberlineLarynx::TENSION_INPUT].channels = n;
		for (int c = 0; c < n; c++) {
			larynx.inputs[CyberlineLarynx::TENSION_INPUT].setVoltage(
				broca.outputs[CyberlineBroca::LARYNX_POLY_OUTPUT].getVoltage(c), c);
		}
	};

	// Patch Broca TRACT_POLY_OUTPUT to Voice V_TONGUEX_INPUT (TNGX/POLY)
	auto patchVoiceBus = [&]() {
		int n = broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].getChannels();
		voice.inputs[CyberlineVoice::V_TONGUEX_INPUT].channels = n;
		for (int c = 0; c < n; c++) {
			voice.inputs[CyberlineVoice::V_TONGUEX_INPUT].setVoltage(
				broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].getVoltage(c), c);
		}
	};

	// Consonant /S/ (voiceless alveolar fricative)
	int sIndex = -1;
	for (int c = 0; c < CyberlineBroca::NUM_CONSONANTS; c++) {
		if (std::string(CyberlineBroca::CONSONANTS[c].code) == "S") {
			sIndex = c;
			break;
		}
	}
	CHECK(sIndex >= 0, "Found consonant /S/ in table");
	broca.params[CyberlineBroca::CONSONANT_PARAM].setValue((float)sIndex);
	broca.inputs[CyberlineBroca::TRIG_INPUT].setVoltage(10.f);
	broca.process(args);
	broca.inputs[CyberlineBroca::TRIG_INPUT].setVoltage(0.f);

	float minLarynxTargetVoicing = 10.f;
	float minVoiceTargetVoicing = 10.f;
	bool larynxValid = true;
	bool voiceValid = true;

	for (int i = 0; i < (int)(0.08f * args.sampleRate); i++) {
		broca.process(args);
		patchLarynxBus();
		patchVoiceBus();

		larynx.process(args);
		voice.process(args);

		float lv = larynx.glottis.targetVoicing;
		float vv = voice.glottis.targetVoicing;
		if (lv < minLarynxTargetVoicing) minLarynxTargetVoicing = lv;
		if (vv < minVoiceTargetVoicing) minVoiceTargetVoicing = vv;

		float lo = larynx.outputs[CyberlineLarynx::VOICED_OUTPUT].getVoltage();
		float vo = voice.outputs[CyberlineVoice::V_LIPS_OUTPUT].getVoltage();
		if (std::isnan(lo) || std::isinf(lo)) larynxValid = false;
		if (std::isnan(vo) || std::isinf(vo)) voiceValid = false;
	}

	std::printf("  Larynx glottis targetVoicing during /S/: %.3f\n", minLarynxTargetVoicing);
	std::printf("  Voice glottis targetVoicing during /S/: %.3f\n", minVoiceTargetVoicing);
	CHECK(minLarynxTargetVoicing < 0.1f, "Larynx received voiceless command on LARYNX POLY Ch 3 (targetVoicing ~ 0)");
	CHECK(minVoiceTargetVoicing < 0.1f, "Voice received voiceless command on TRACT POLY Ch 9 (targetVoicing ~ 0)");
	CHECK(larynxValid, "Larynx audio output is finite and stable");
	CHECK(voiceValid, "Voice audio output is finite and stable");

	// Test Larynx Tension CV attenuator knob
	larynx.inputs[CyberlineLarynx::TENSION_INPUT].channels = 1;
	larynx.inputs[CyberlineLarynx::TENSION_INPUT].setVoltage(5.0f, 0); // +0.5 tension
	larynx.params[CyberlineLarynx::TENSION_PARAM].setValue(0.3f);
	larynx.params[CyberlineLarynx::TENSIONCV_PARAM].setValue(1.0f); // 100% CV
	for (int i = 0; i < 40; i++) larynx.process(args);
	float tensFull = larynx.glottis.targetTenseness;

	larynx.params[CyberlineLarynx::TENSIONCV_PARAM].setValue(0.0f); // 0% CV (fully attenuated)
	for (int i = 0; i < 40; i++) larynx.process(args);
	float tensAtten = larynx.glottis.targetTenseness;
	std::printf("  Larynx tenseness full CV: %.3f, attenuated CV: %.3f\n", tensFull, tensAtten);
	CHECK(tensFull > tensAtten + 0.35f, "Larynx Tension CV attenuator trims CV modulation depth");
}

static void test_brocasq_registers_clocking_and_mutation() {
	std::printf("--- Testing CyberlineBrocaSq: registers, clocking, mutation, phasor ---\n");
	// Headless tests run without Rack's app context, which normally seeds the
	// global PRNG (rack::random::init() is called by Rack/Cardinal at startup).
	// Without it the generator is a zero-state (always returns 0), so seed here.
	rack::random::init();
	CyberlineBrocaSq sq;
	rack::engine::Module::ProcessArgs args;
	args.sampleRate = 48000.f;
	args.sampleTime = 1.f / 48000.f;
	sq.outputs[CyberlineBrocaSq::TRACT_POLY_OUTPUT].channels = 1;
	sq.outputs[CyberlineBrocaSq::LARYNX_POLY_OUTPUT].channels = 1;

	// ---- Register wrap at length boundaries (CLOCK mode, explicit edges) ----
	sq.params[CyberlineBrocaSq::MODE_PARAM].setValue(1.f); // CLOCK
	sq.params[CyberlineBrocaSq::VLEN_PARAM].setValue(4.f);
	sq.params[CyberlineBrocaSq::CLEN_PARAM].setValue(3.f);
	for (int i = 0; i < 16; i++) {
		sq.vowelReg[i] = (uint8_t) i;
		sq.consReg[i] = (uint8_t) i;
	}
	auto clockPulse = [&]() {
		sq.inputs[CyberlineBrocaSq::CLOCK_INPUT].channels = 1;
		sq.inputs[CyberlineBrocaSq::CLOCK_INPUT].setVoltage(5.f, 0);
		sq.process(args);
		sq.inputs[CyberlineBrocaSq::CLOCK_INPUT].setVoltage(0.f, 0);
		sq.process(args);
	};
	clockPulse(); clockPulse(); clockPulse();
	CHECK(sq.vPos == 3 && sq.cPos == 0,
		"3 clock steps with VLEN=4/CLEN=3 -> vPos=3, cPos=0 (consonant wrapped)");

	// ---- Length clamping mid-play (step stays valid) ----
	sq.params[CyberlineBrocaSq::CLEN_PARAM].setValue(16.f);
	sq.process(args); // next step
	CHECK(sq.cPos >= 0 && sq.cPos < 16 && sq.vPos < 4,
		"Length change mid-play keeps both heads in range");

	// ---- Phasor mode: stateless position, forward and backward ----
	sq.params[CyberlineBrocaSq::MODE_PARAM].setValue(2.f); // PHASOR
	sq.params[CyberlineBrocaSq::VLEN_PARAM].setValue(4.f);
	sq.params[CyberlineBrocaSq::CLEN_PARAM].setValue(4.f);
	// Vowel phasor at 75% of a pass (7.5 V) -> position 3 of 4
	sq.inputs[CyberlineBrocaSq::VPH_INPUT].channels = 1;
	sq.inputs[CyberlineBrocaSq::VPH_INPUT].setVoltage(7.5f, 0);
	sq.inputs[CyberlineBrocaSq::CPH_INPUT].channels = 1;
	sq.inputs[CyberlineBrocaSq::CPH_INPUT].setVoltage(0.f, 0);
	for (int i = 0; i < 32; i++) sq.process(args);
	CHECK(sq.vPos == 3, "Vowel phasor at 7.5 V with VLEN=4 -> position 3 (stateless)");
	CHECK(sq.cPos == 0, "Consonant phasor at 0 V -> position 0");

	// Backward phasor: drop to 2.5 V -> position 1 (pure function, no direction state)
	sq.inputs[CyberlineBrocaSq::VPH_INPUT].setVoltage(2.5f, 0);
	for (int i = 0; i < 32; i++) sq.process(args);
	CHECK(sq.vPos == 1, "Vowel phasor stepped backward to 2.5 V -> position 1 (reversal works)");

	// Snap-on-jump: a jump across several thresholds fires one gesture
	int gesturesBefore = sq.onsetActive ? 1 : 0;
	(void) gesturesBefore;
	sq.onsetPhase = 1.f;
	sq.onsetActive = false;
	sq.inputs[CyberlineBrocaSq::VPH_INPUT].setVoltage(9.5f, 0);
	for (int i = 0; i < 32; i++) sq.process(args);
	CHECK(sq.vPos == 3, "Phasor jump 2.5 V -> 9.5 V snaps to the new step (one gesture, no sweep)");

	// Unpatched consonant phasor follows the vowel phasor
	sq.inputs[CyberlineBrocaSq::CPH_INPUT].channels = 0;
	// (simulate unpatched by clearing voltage / channels)
	sq.inputs[CyberlineBrocaSq::CPH_INPUT].voltages[0] = 0.f;
	sq.process(args);
	CHECK(sq.cPos == sq.vPos,
		"Unpatched consonant phasor follows the vowel phasor (fallback rule)");

	// ---- Mutation: at 0% probability registers never change ----
	sq.params[CyberlineBrocaSq::MODE_PARAM].setValue(1.f); // CLOCK
	sq.params[CyberlineBrocaSq::MUTV_PARAM].setValue(0.f);
	sq.params[CyberlineBrocaSq::MUTC_PARAM].setValue(0.f);
	uint8_t v0 = sq.vowelReg[0], c0 = sq.consReg[0];
	for (int i = 0; i < 40; i++) clockPulse();
	CHECK(sq.vowelReg[0] == v0 && sq.consReg[0] == c0,
		"MUT-V / MUT-C at 0% leave the registers untouched after many steps");

	// ---- Mutation: at 100% probability registers keep changing ----
	sq.params[CyberlineBrocaSq::MUTV_PARAM].setValue(1.f);
	sq.params[CyberlineBrocaSq::MUTC_PARAM].setValue(1.f);
	int changed = 0;
	for (int i = 0; i < 20; i++) {
		clockPulse();
		if (sq.vowelReg[sq.vPos] != v0 || sq.consReg[sq.cPos] != c0)
			changed++;
		v0 = sq.vowelReg[sq.vPos];
		c0 = sq.consReg[sq.cPos];
	}
	CHECK(changed > 12, "MUT-V / MUT-C at 100% mutate the slots on most steps");

	// ---- Output conventions match Broca (channel counts & vowel geometry) ----
	CHECK(sq.outputs[CyberlineBrocaSq::TRACT_POLY_OUTPUT].getChannels() == 9,
		"BrocaSq TRACT POLY bus has 9 channels (same convention as Broca)");
	CHECK(sq.outputs[CyberlineBrocaSq::LARYNX_POLY_OUTPUT].getChannels() == 3,
		"BrocaSq LARYNX POLY bus has 3 channels (same convention as Broca)");

	// VOW CV tracks the vowel index (0-10 V over 12)
	sq.params[CyberlineBrocaSq::MUTV_PARAM].setValue(0.f);
	sq.params[CyberlineBrocaSq::MUTC_PARAM].setValue(0.f);
	for (int i = 0; i < 16; i++) sq.vowelReg[i] = 7; // OO everywhere
	for (int i = 0; i < (int) (0.2f * args.sampleRate); i++) sq.process(args);
	float vowCv = sq.outputs[CyberlineBrocaSq::VOWCV_OUTPUT].getVoltage();
	CHECK(std::fabs(vowCv - 7.f * (10.f / 11.f)) < 0.2f,
		"VOW CV reports the current vowel index scaled 0-10 V over 12");

	// RESET zeroes the heads without rewriting registers
	for (int i = 0; i < 16; i++) sq.vowelReg[i] = 7;
	sq.inputs[CyberlineBrocaSq::RESET_INPUT].channels = 1;
	sq.inputs[CyberlineBrocaSq::RESET_INPUT].setVoltage(5.f, 0);
	sq.process(args);
	sq.inputs[CyberlineBrocaSq::RESET_INPUT].setVoltage(0.f, 0);
	sq.process(args);
	CHECK(sq.vPos == 0 && sq.cPos == 0 && sq.vowelReg[0] == 7,
		"RESET zeroes both heads and preserves register contents");

	// GATE pulses on the syllable gesture
	clockPulse();
	float gateMax = 0.f;
	for (int i = 0; i < (int) (0.05f * args.sampleRate); i++) {
		sq.process(args);
		gateMax = std::max(gateMax, sq.outputs[CyberlineBrocaSq::GATE_OUTPUT].getVoltage());
	}
	CHECK(gateMax > 5.f, "GATE output pulses with the syllable gesture");
}

int main() {
	std::printf("=========================================\n");
	std::printf("Running Cyberline Module Integration Tests\n");
	std::printf("=========================================\n");

	test_lungs_singer_cycle();
	test_lungs_sync_and_gate();
	test_lungs_exhale_first_polarity();
	test_lungs_phasor_ratio();
	test_broca_channels_and_vowels();
	test_broca_consonants_and_sync();
	test_poly_normalling_oral_chamber();
	test_quality_global_and_override();
	test_tract_settled_early_out_bitexact();
	test_poly_normalling_larynx_and_voice();
	test_brocasq_registers_clocking_and_mutation();

	std::printf("=========================================\n");
	std::printf("Results: %d passed, %d failed\n", g_passCount, g_failCount);
	std::printf("=========================================\n");

	return (g_failCount == 0) ? 0 : 1;
}
