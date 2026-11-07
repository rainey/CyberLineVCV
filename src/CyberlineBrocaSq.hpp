// CyberlineBrocaSq — "Broca²" (Broca-Squared) dual phoneme Turing machine.
// Two rotating phoneme registers — a vowel register (Broca's 12 vowels) and a
// consonant register (Broca's 21 consonants, index 0 = NONE/rest) — with
// independent lengths (1–16 steps), per-step write-current-slot mutation at
// independent probabilities, and three clocking modes: FREE (internal RATE
// clock), CLOCK (external trigger), and PHASOR (independent 0–10 V phase
// inputs per register; stateless position = floor(phase·len), so forward and
// backward phasors both work and fast jumps snap to a new step).
//
// The syllable gesture reuses Broca's consonant→vowel onset engine and emits
// the identical poly bus conventions (TRACT POLY 9-ch, LARYNX POLY 3-ch), so
// Broca² is a drop-in replacement for Broca in the Larynx/OralChamber/Voice
// chain.

#pragma once
#include "CyberlineModels.hpp"
#include "CyberlineBroca.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineBrocaSq : rack::engine::Module {
	enum ParamIds {
		MODE_PARAM,     // clocking mode: FREE / CLOCK / PHASOR
		RATE_PARAM,     // internal clock rate (steps/sec) in FREE mode
		VLEN_PARAM,     // vowel register length, 1..16
		CLEN_PARAM,     // consonant register length, 1..16
		MUTV_PARAM,     // vowel register mutation probability 0..1
		MUTC_PARAM,     // consonant register mutation probability 0..1
		MORPH_PARAM,    // articulator slew between phonemes 0..1
		ONSET_PARAM,    // consonant onset/glide time (same span as Broca)
		STEP_PARAM,     // manual step button (trigger)
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		VPH_INPUT,      // vowel register phase CV (0-10 V ramp)
		CPH_INPUT,      // consonant register phase CV (0-10 V ramp)
		PV_INPUT,       // vowel mutation probability CV (0-10 V)
		PC_INPUT,       // consonant mutation probability CV (0-10 V)
		MORPH_INPUT,    // morph slew CV (0-10 V)
		LEN_INPUT,      // shared length CV (scales both lengths)
		NUM_INPUTS
	};
	enum OutputIds {
		TRACT_POLY_OUTPUT,   // identical channel map to Broca (9-ch)
		LARYNX_POLY_OUTPUT,  // identical channel map to Broca (3-ch)
		CVENV_OUTPUT,        // consonant onset envelope 0-10 V
		BURST_OUTPUT,        // fricative/plosive burst gate 0-10 V
		GATE_OUTPUT,         // syllable gesture gate 0-10 V
		VOWCV_OUTPUT,        // current vowel index as CV (0-10 V over 12)
		CONSCV_OUTPUT,       // current consonant index as CV (0-10 V over 21)
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	static constexpr int REG_LEN = 16;   // register capacity
	static constexpr int NUM_VOWELS = CyberlineBroca::NUM_VOWELS;
	static constexpr int NUM_CONSONANTS = CyberlineBroca::NUM_CONSONANTS;

	// Clocking modes (MODE switch)
	enum Mode {
		MODE_FREE = 0,
		MODE_CLOCK,
		MODE_PHASOR
	};

	// ---- Register state ----
	uint8_t vowelReg[REG_LEN] = {0, 2, 4, 7, 2, 4, 9, 0, 4, 7, 11, 2, 7, 4, 2, 0};
	uint8_t consReg[REG_LEN]  = {0, 11, 0, 13, 0, 2, 0, 15, 11, 0, 8, 0, 6, 0, 19, 0};

	// ---- Clocking state ----
	int mode = MODE_FREE;
	int step = 0;            // shared step counter (FREE/CLOCK modes)
	int vPos = 0;            // vowel register read position 0..15
	int cPos = 0;            // consonant register read position 0..15
	bool clockWasHigh = false;
	bool resetWasHigh = false;
	bool stepWasPressed = false;
	float freePhase = 0.f;   // internal clock/phasor accumulator 0..1

	// ---- Syllable gesture state (mirrors Broca) ----
	bool onsetActive = false;
	float onsetPhase = 1.f;
	float gateLevel = 0.f;

	// ---- Smoothed anatomical outputs (same parameter space as Broca) ----
	float curTongueX = 5.0f;
	float curTongueY = 5.0f;
	float curVelum = 0.01f;
	float curConstrPos = 0.0f;
	float curConstrWidth = 5.0f;
	float curFric = 0.0f;
	float curVoicing = 1.0f;
	float curPressure = 0.8f;

	// ---- Telemetry for the 4x4 grid HUD ----
	int telVLen = 8;
	int telCLen = 8;
	int telVPos = 0;
	int telCPos = 0;
	int telMode = MODE_FREE;
	int activeVowel = 0;
	int activeConsonant = 0;
	float activeCVMix = 0.f;
	float activeBurst = 0.f;

	CyberlineCpuMeter cpuMeter;

	CyberlineBrocaSq() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configSwitch(MODE_PARAM, 0.f, 2.f, 0.f, "Clocking mode", {
			"FREE (internal RATE clock)",
			"CLOCK (external trigger)",
			"PHASOR (dual phase CVs)"
		});
		configParam(RATE_PARAM, 0.f, 1.f, 0.35f, "Internal step rate", " steps/s", 0.f, 19.5f, 0.5f);
		configParam(VLEN_PARAM, 1.f, 16.f, 8.f, "Vowel register length", " steps");
		configParam(CLEN_PARAM, 1.f, 16.f, 8.f, "Consonant register length", " steps");
		configParam(MUTV_PARAM, 0.f, 1.f, 0.f, "Vowel mutation probability", "%", 0.f, 100.f);
		configParam(MUTC_PARAM, 0.f, 1.f, 0.f, "Consonant mutation probability", "%", 0.f, 100.f);
		configParam(MORPH_PARAM, 0.f, 1.f, 0.f, "Morph (articulator slew)", "%", 0.f, 100.f);
		configParam(ONSET_PARAM, 0.02f, 0.35f, 0.11f, "Consonant onset / glide time", " s");
		configButton(STEP_PARAM, "Manual step");

		configInput(CLOCK_INPUT, "Clock / step trigger (CLOCK mode)");
		configInput(RESET_INPUT, "Reset (trigger: step counter & heads to 0; registers untouched)");
		configInput(VPH_INPUT, "Vowel register phase CV (0-10 V ramp, PHASOR mode)");
		configInput(CPH_INPUT, "Consonant register phase CV (0-10 V ramp, PHASOR mode)");
		configInput(PV_INPUT, "Vowel mutation probability CV (0-10 V)");
		configInput(PC_INPUT, "Consonant mutation probability CV (0-10 V)");
		configInput(MORPH_INPUT, "Morph slew CV (0-10 V)");
		configInput(LEN_INPUT, "Length CV (scales both register lengths)");

		configOutput(TRACT_POLY_OUTPUT, "Tract Poly Bus (Ch 1-6: TNGX, TNGY, VEL, CPOS, CWID, FRIC; Ch 7-9: TENS, PRESS, VOICING)");
		configOutput(LARYNX_POLY_OUTPUT, "Larynx Poly Bus (Ch 1: TENS, Ch 2: PRESS, Ch 3: VOICING)");
		configOutput(CVENV_OUTPUT, "Consonant onset envelope (0-10 V)");
		configOutput(BURST_OUTPUT, "Fricative / plosive burst gate (0-10 V)");
		configOutput(GATE_OUTPUT, "Syllable gesture gate (0-10 V)");
		configOutput(VOWCV_OUTPUT, "Current vowel index CV (0-10 V over 12)");
		configOutput(CONSCV_OUTPUT, "Current consonant index CV (0-10 V over 21)");
	}

	void process(const ProcessArgs& args) override;

	// Read the effective lengths (knobs + shared LEN CV scaling)
	void getLengths(int* vLen, int* cLen) {
		float lenScale = inputs[LEN_INPUT].isConnected()
			? (1.f + inputs[LEN_INPUT].getVoltage() * 0.08f)   // ±0.4 length scale at ±5 V
			: 1.f;
		*vLen = (int) rack::math::clamp(std::round(params[VLEN_PARAM].getValue() * lenScale), 1.f, (float) REG_LEN);
		*cLen = (int) rack::math::clamp(std::round(params[CLEN_PARAM].getValue() * lenScale), 1.f, (float) REG_LEN);
	}

	void initRegisters() {
		const uint8_t v[REG_LEN] = {0, 2, 4, 7, 2, 4, 9, 0, 4, 7, 11, 2, 7, 4, 2, 0};
		const uint8_t c[REG_LEN] = {0, 11, 0, 13, 0, 2, 0, 15, 11, 0, 8, 0, 6, 0, 19, 0};
		for (int i = 0; i < REG_LEN; i++) {
			vowelReg[i] = v[i];
			consReg[i] = c[i];
		}
	}

	void randomizeRegisters() {
		for (int i = 0; i < REG_LEN; i++) {
			vowelReg[i] = (uint8_t) (rack::random::u32() % NUM_VOWELS);
			// NONE allowed: consonant rolls may land on rests (index 0)
			consReg[i] = (uint8_t) (rack::random::u32() % NUM_CONSONANTS);
		}
	}

	void onReset() override {
		step = 0;
		vPos = 0;
		cPos = 0;
		freePhase = 0.f;
		onsetActive = false;
		onsetPhase = 1.f;
		gateLevel = 0.f;
		clockWasHigh = resetWasHigh = stepWasPressed = false;
		curTongueX = 5.0f;
		curTongueY = 5.0f;
		curVelum = 0.01f;
		curConstrPos = 0.0f;
		curConstrWidth = 5.0f;
		curFric = 0.0f;
		curVoicing = 1.0f;
		curPressure = 0.8f;
		activeBurst = 0.f;
		initRegisters();
	}

	// JSON: persist registers, mode, and positions
	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "mode", json_integer(mode));
		json_t* vJ = json_array();
		json_t* cJ = json_array();
		for (int i = 0; i < REG_LEN; i++) {
			json_array_append_new(vJ, json_integer(vowelReg[i]));
			json_array_append_new(cJ, json_integer(consReg[i]));
		}
		json_object_set_new(rootJ, "vowelReg", vJ);
		json_object_set_new(rootJ, "consReg", cJ);
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* mJ = json_object_get(rootJ, "mode");
		if (mJ) mode = (int) json_integer_value(mJ);
		json_t* vJ = json_object_get(rootJ, "vowelReg");
		json_t* cJ = json_object_get(rootJ, "consReg");
		for (int i = 0; i < REG_LEN; i++) {
			if (vJ) {
				json_t* e = json_array_get(vJ, i);
				if (e) vowelReg[i] = (uint8_t) rack::math::clamp((int) json_integer_value(e), 0, NUM_VOWELS - 1);
			}
			if (cJ) {
				json_t* e = json_array_get(cJ, i);
				if (e) consReg[i] = (uint8_t) rack::math::clamp((int) json_integer_value(e), 0, NUM_CONSONANTS - 1);
			}
		}
	}
};

} // namespace cyberline
