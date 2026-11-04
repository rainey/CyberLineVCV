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
#include "../src/CyberlineOralChamber.hpp"
#include "../src/CyberlineLarynx.hpp"
#include "../src/CyberlineVoice.hpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
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

	// Let inhale finish (0.5s)
	for (int i = 0; i < (int)(0.5f * args.sampleRate); i++) {
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

static void test_broca_channels_and_vowels() {
	std::printf("--- Testing CyberlineBroca: buses and vowel morphing ---\n");
	CyberlineBroca broca;
	rack::engine::Module::ProcessArgs args;
	args.sampleRate = 48000.f;
	args.sampleTime = 1.f / 48000.f;

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

	// Run 200 samples to settle smoothing filters
	for (int i = 0; i < 200; i++) broca.process(args);
	float eeTongueX = broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].getVoltage(0);

	broca.params[CyberlineBroca::VMORPH_PARAM].setValue(1.f); // Pure OO
	for (int i = 0; i < 200; i++) broca.process(args);
	float ooTongueX = broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].getVoltage(0);

	std::printf("  Vowel EE TongueX offset: %.3f V, Vowel OO TongueX offset: %.3f V\n",
		eeTongueX, ooTongueX);
	CHECK(eeTongueX > ooTongueX + 4.0f, "Front vowel /i/ has significantly higher TongueX than back vowel /u/");

	// Test voicing channel on steady-state vowels
	float steadyVoicing = broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].getVoltage(8);
	float steadyLarynxVoicing = broca.outputs[CyberlineBroca::LARYNX_POLY_OUTPUT].getVoltage(2);
	CHECK(steadyVoicing >= 9.9f, "Vowels have full 10V voicing on TRACT POLY Ch 9");
	CHECK(steadyLarynxVoicing >= 9.9f, "Vowels have full 10V voicing on LARYNX POLY Ch 3");
}

static void test_broca_consonants_and_sync() {
	std::printf("--- Testing CyberlineBroca: consonants and manners ---\n");
	CyberlineBroca broca;
	rack::engine::Module::ProcessArgs args;
	args.sampleRate = 48000.f;
	args.sampleTime = 1.f / 48000.f;

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

	// Initial step
	broca.process(args);

	// Patch Broca TRACT POLY to Oral Chamber TONGUEX_INPUT (TNGX/POLY)
	// (Simulate Rack patch cable copying poly voltages)
	auto patchTractBus = [&]() {
		int n = broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].getChannels();
		chamber.inputs[CyberlineOralChamber::TONGUEX_INPUT].setChannels(n);
		for (int c = 0; c < n; c++) {
			chamber.inputs[CyberlineOralChamber::TONGUEX_INPUT].setVoltage(
				broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].getVoltage(c), c);
		}
	};

	// Test VTL CV modulation on Oral Chamber
	chamber.params[CyberlineOralChamber::VTL_PARAM].setValue(17.5f);
	chamber.inputs[CyberlineOralChamber::VTL_INPUT].setVoltage(0.f);
	chamber.process(args);
	CHECK(std::fabs(chamber.vtlCm - 17.5f) < 0.1f, "VTL default is 17.5 cm");

	// Add 5V CV to VTL input (scaled +1.5 cm/V = +7.5 cm -> 25.0 cm)
	chamber.inputs[CyberlineOralChamber::VTL_INPUT].setVoltage(5.0f);
	chamber.process(args);
	std::printf("  Oral Chamber VTL at 5V CV: %.2f cm\n", chamber.vtlCm);
	CHECK(std::fabs(chamber.vtlCm - 25.0f) < 0.2f, "VTL CV modulates effective tract length to 25.0 cm");
	chamber.inputs[CyberlineOralChamber::VTL_INPUT].setVoltage(0.f);

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

static void test_poly_normalling_larynx_and_voice() {
	std::printf("--- Testing Poly Normalling: Broca -> Larynx & Voice ---\n");
	CyberlineBroca broca;
	CyberlineLarynx larynx;
	CyberlineVoice voice;
	rack::engine::Module::ProcessArgs args;
	args.sampleRate = 48000.f;
	args.sampleTime = 1.f / 48000.f;

	broca.process(args);

	// Patch Broca LARYNX_POLY_OUTPUT to Larynx TENSION_INPUT (TENS/POLY)
	auto patchLarynxBus = [&]() {
		int n = broca.outputs[CyberlineBroca::LARYNX_POLY_OUTPUT].getChannels();
		larynx.inputs[CyberlineLarynx::TENSION_INPUT].setChannels(n);
		for (int c = 0; c < n; c++) {
			larynx.inputs[CyberlineLarynx::TENSION_INPUT].setVoltage(
				broca.outputs[CyberlineBroca::LARYNX_POLY_OUTPUT].getVoltage(c), c);
		}
	};

	// Patch Broca TRACT_POLY_OUTPUT to Voice V_TONGUEX_INPUT (TNGX/POLY)
	auto patchVoiceBus = [&]() {
		int n = broca.outputs[CyberlineBroca::TRACT_POLY_OUTPUT].getChannels();
		voice.inputs[CyberlineVoice::V_TONGUEX_INPUT].setChannels(n);
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
}

int main() {
	std::printf("=========================================\n");
	std::printf("Running Cyberline Module Integration Tests\n");
	std::printf("=========================================\n");

	test_lungs_singer_cycle();
	test_lungs_sync_and_gate();
	test_broca_channels_and_vowels();
	test_broca_consonants_and_sync();
	test_poly_normalling_oral_chamber();
	test_poly_normalling_larynx_and_voice();

	std::printf("=========================================\n");
	std::printf("Results: %d passed, %d failed\n", g_passCount, g_failCount);
	std::printf("=========================================\n");

	return (g_failCount == 0) ? 0 : 1;
}
