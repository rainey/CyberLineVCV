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
		FAVOR_PARAM,    // consonant character: 0 = nasal/glide/fricative, 1 = stop
		EDIT_POS_PARAM, // register editor: cell index 0..15 (wrapping encoder)
		EDIT_V_PARAM,   // register editor: vowel index 0..11 at editPos (wrapping)
		EDIT_C_PARAM,   // register editor: consonant index 0..20 at editPos (wrapping)
		EDIT_BUT_PARAM, // register editor: arm/disarm button (momentary)
		LKV_BUT_PARAM,  // toggle vowel mutation lock at editPos (momentary)
		LKC_BUT_PARAM,  // toggle consonant mutation lock at editPos (momentary)
		COPYL_BUT_PARAM,// copy the edited pair to the left neighbor cell
		FILL_BUT_PARAM, // fill the whole register with the edited pair
		COPYR_BUT_PARAM,// copy the edited pair to the right neighbor cell
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
		LKV_LIGHT,      // green: vowel cell at editPos is mutation-locked
		LKC_LIGHT,      // green: consonant cell at editPos is mutation-locked
		EDIT_LIGHT,     // yellow: register editor armed
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

	// ---- Per-cell mutation locks (bit i = cell i frozen) ----
	// Persisted; toggled via the LK-V / LK-C buttons at the editor position.
	uint32_t vowelLockMask = 0;
	uint32_t consLockMask = 0;

	// ---- Register editor state ----
	// editActive is runtime-only (auto-armed by dragging the V/C edit
	// encoders, or toggled by the EDIT button). While armed, the edited
	// cell pair is written from the V/C encoder params and locked from
	// mutation.
	int editPos = 0;
	bool editActive = false;
	bool editButWas = false;
	bool lkvWas = false;
	bool lkcWas = false;
	bool copylWas = false;
	bool copyrWas = false;
	bool fillWas = false;
	int copyHoldDir = 0;   // 0 = none; -1/+1 while a copy button is held

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
	int telEditPos = 0;
	bool telEditActive = false;
	uint32_t telVLocks = 0;
	uint32_t telCLocks = 0;
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
		configParam<SnapParamQuantity>(VLEN_PARAM, 1.f, 16.f, 8.f, "Vowel register length", " steps");
		configParam<SnapParamQuantity>(CLEN_PARAM, 1.f, 16.f, 8.f, "Consonant register length", " steps");
		configParam(MUTV_PARAM, 0.f, 1.f, 0.f, "Vowel mutation probability", "%", 0.f, 100.f);
		configParam(MUTC_PARAM, 0.f, 1.f, 0.f, "Consonant mutation probability", "%", 0.f, 100.f);
		configParam(FAVOR_PARAM, 0.f, 1.f, 0.5f, "Consonant character (nasal/glide \xE2\x86\x90\xE2\x86\x92 fricative/stop)", "%", 0.f, 100.f);
		configParam<SnapParamQuantity>(EDIT_POS_PARAM, 0.f, 15.f, 0.f, "Editor cell");
		configParam<SnapParamQuantity>(EDIT_V_PARAM, 0.f, 11.f, 0.f, "Editor vowel");
		configParam<SnapParamQuantity>(EDIT_C_PARAM, 0.f, 20.f, 0.f, "Editor consonant");
		configButton(EDIT_BUT_PARAM, "Edit registers (arm/disarm)");
		configButton(LKV_BUT_PARAM, "Lock/unlock vowel cell from mutation");
		configButton(LKC_BUT_PARAM, "Lock/unlock consonant cell from mutation");
		configButton(COPYL_BUT_PARAM, "Copy cell to left neighbor");
		configButton(FILL_BUT_PARAM, "Fill register with cell");
		configButton(COPYR_BUT_PARAM, "Copy cell to right neighbor");
		configParam(MORPH_PARAM, 0.f, 1.f, 0.f, "Morph (articulator slew)", "%", 0.f, 100.f);
		configParam(ONSET_PARAM, 0.02f, 0.35f, 0.11f, "Consonant onset / glide time", " s");
		configButton(STEP_PARAM, "Manual step");

		configInput(CLOCK_INPUT, "Clock / step trigger (CLOCK mode; unpatched = no steps)");
		configInput(RESET_INPUT, "Reset (trigger: step counter & heads to 0; registers untouched)");
		configInput(VPH_INPUT, "Vowel register phase CV (0-10 V ramp, PHASOR mode; unpatched = hold)");
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

	// ---- Register editor utilities ----
	// Copy the edited pair to the neighbor cell (left/right, wrapping);
	// locked target cells are respected (copy does not overwrite them).
	void copyCell(int dir) {
		int dst = (editPos + dir + REG_LEN) % REG_LEN;
		if (!((vowelLockMask >> dst) & 1u))
			vowelReg[dst] = vowelReg[editPos];
		if (!((consLockMask >> dst) & 1u))
			consReg[dst] = consReg[editPos];
	}

	// Fill the whole register with the edited pair (skipping locked
	// cells so user-locked content survives).
	void fillCells() {
		for (int i = 0; i < REG_LEN; i++) {
			if (!((vowelLockMask >> i) & 1u))
				vowelReg[i] = vowelReg[editPos];
			if (!((consLockMask >> i) & 1u))
				consReg[i] = consReg[editPos];
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
		vowelLockMask = consLockMask = 0;
		editActive = false;
		editPos = 0;
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
		json_object_set_new(rootJ, "vowelLocks", json_integer(vowelLockMask));
		json_object_set_new(rootJ, "consLocks", json_integer(consLockMask));
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
		json_t* vlJ = json_object_get(rootJ, "vowelLocks");
		if (vlJ) vowelLockMask = (uint32_t) json_integer_value(vlJ) & 0xffffu;
		json_t* clJ = json_object_get(rootJ, "consLocks");
		if (clJ) consLockMask = (uint32_t) json_integer_value(clJ) & 0xffffu;
	}
};

} // namespace cyberline
