// CyberlinePtBreakout — "PT Breakout": adapter between cyberline's
// Pink-Trombone-mode poly bus and Vega Deftwing's PinkTromboneVCV module.
//
// Feed it a TRACT POLY cable from Broca/BrocaSq with "Pink Trombone poly
// mode" enabled (10-channel re-encoded bus, see PtBusEncode) and it emits
// the individual mono CVs Pink Trombone expects, with the voltage
// conventions PT uses:
//   VOCT      1 V/oct relative to C4 (PT multiplies C4·knob by 2^V)
//   TONGUE X/Y ±10 V offsets onto PT's 0..1 tongue axes (attenver. 1.0)
//   THROAT X  ±10 V onto PT's 0.06..1 constriction axis
//   FRIC FC   0-5 V  (PT FRICFCI: 2000 Hz/V -> 0-10 kHz)
//   FRIC LVL  0-10 V (0-1 fricative level)
//   TENSE     0-10 V (0-1 tenseness)
//   VEL       monitor only (PT has no velum CV)
// Creative fusions of channels PT cannot sink:
//   TENSE out  = TENSE knob * (VOICING/10)  — voiceless consonants drop
//               PT into breathy air (voicing gates the fold tension)
//   FRIC LVL   = FRIC + PRESS boost         — pressure shades fricative
//               energy (PT has no pressure CV)
// Pitch: FOLD mm knob via the suite's allometric law referenced to C4
// (1 V/oct), or the V/OCT input passed through; the SRC switch selects
// LAW+in / VOCT pass-through.

#pragma once
#include "CyberlineModels.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlinePtBreakout : rack::engine::Module {
	enum ParamIds {
		FOLD_PARAM,     // fold length mm (law pitch source)
		TENSE_PARAM,    // PT tenseness base 0..1
		SRC_PARAM,      // pitch source: 0 = LAW + V/OCT in, 1 = V/OCT pass-through
		NUM_PARAMS
	};
	enum InputIds {
		BUS_INPUT,      // 10-ch TRACT POLY from Broca/BrocaSq in PT mode
		VOCT_INPUT,     // 1 V/oct (summed with LAW, or pass-through)
		NUM_INPUTS
	};
	enum OutputIds {
		VOCT_OUTPUT,
		TONGUEX_OUTPUT,
		TONGUEY_OUTPUT,
		THROATX_OUTPUT,
		FRICFC_OUTPUT,
		FRICLVL_OUTPUT,
		TENSE_OUTPUT,
		VEL_OUTPUT,     // monitor (no PT sink)
		NUM_OUTPUTS
	};
	enum LightIds {
		BUSOK_LIGHT,    // green: cyberline phoneme module adjacent LEFT (expander link) and advertising PT mode
		NUM_LIGHTS
	};

	// Auto-detected bus mode from the left expander link. Three states:
	// no neighbor / non-phoneme neighbor => muted (safe default);
	// phoneme neighbor in cyberline mode => muted too (bus carries the
	// wrong map); phoneme neighbor advertising PT mode => fan out.
	bool expanderPtMode = false;
	bool expanderLinked = false;

	static constexpr int BUS_CH = 10;

	CyberlineCpuMeter cpuMeter;

	CyberlinePtBreakout() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(FOLD_PARAM, 2.f, 45.f, 14.f, "Fold length (law pitch)", " mm");
		configParam(TENSE_PARAM, 0.f, 1.f, 0.44f, "PT tenseness", "%", 0.f, 100.f);
		configSwitch(SRC_PARAM, 0.f, 1.f, 0.f, "Pitch source",
			{"LAW + V/OCT in", "V/OCT pass-through"});
		configInput(BUS_INPUT, "TRACT POLY from a Broca/BrocaSq placed directly LEFT (mode auto-follows the neighbor's Pink Trombone setting via the expander link)");
		configInput(VOCT_INPUT, "V/Oct (1 V/oct relative to C4)");
		configOutput(VOCT_OUTPUT, "VOct -> PT V/OCT (1 V/oct rel. C4)");
		configOutput(TONGUEX_OUTPUT, "Tongue X -> PT TONGUE XI (attenver. 1.0)");
		configOutput(TONGUEY_OUTPUT, "Tongue Y -> PT TONGUE YI");
		configOutput(THROATX_OUTPUT, "Throat X -> PT THROAT XI");
		configOutput(FRICFC_OUTPUT, "Fricative Fc -> PT FRICFCI (2 kHz/V)");
		configOutput(FRICLVL_OUTPUT, "Fricative level -> PT FRICL (0-10 V)");
		configOutput(TENSE_OUTPUT, "Tenseness -> PT TENSEI (0-10 V; = knob x voicing)");
		configOutput(VEL_OUTPUT, "Velum (monitor only; PT has no velum CV)");
	}

	void process(const ProcessArgs& args) override {
		bool measure = cpuMeter.frameStart();

		// ---- Auto-detect the bus protocol via the left expander link ----
		// The neighbor module (directly LEFT on the rack row) advertises its
		// phoneme bus mode on its rightExpander message (magic-tagged).
		// No neighbor, a foreign neighbor, or a cyberline-mode neighbor all
		// read as busOk = false: outputs gate to 0 V instead of interpreting
		// the wrong map as articulation data. BUS OK shows the state.
		expanderLinked = false;
		expanderPtMode = false;
		if (leftExpander.module) {
			// The magic proves the neighbor is a cyberline phoneme module
			// (Broca or BrocaSq publish this exact layout; foreign modules
			// have some other buffer or NULL there).
			auto* msg = (CyberlinePhonemeExpanderMsg*) leftExpander.module->rightExpander.producerMessage;
			if (msg && msg->magic == CYBERLINE_PHONEME_MAGIC) {
				expanderLinked = true;
				expanderPtMode = msg->ptMode;
			}
		}
		bool busOk = expanderLinked && expanderPtMode
			&& inputs[BUS_INPUT].isConnected();

		// ---- Bus fan-out (PT-mode channel map) ----
		float bus[BUS_CH];
		for (int c = 0; c < BUS_CH; c++)
			bus[c] = busOk ? inputs[BUS_INPUT].getVoltage(c) : 0.f;

		// ---- Pitch: law source referenced to C4, or pass-through ----
		// Suite law: log10(F0) = -1.43*log10(L_mm) + 4.01, with the sqrt-
		// tension convention at 0.6; the Larynx reference is tension 0.6,
		// so the law output here is the tension-0.6 base F0. VOCT rel C4:
		//   v = log2(F0 / C4)
		float vLaw = 0.f;
		if (params[SRC_PARAM].getValue() < 0.5f) {
			float foldLen = params[FOLD_PARAM].getValue();
			if (foldLen < 2.f) foldLen = 2.f;
			float f0 = std::pow(10.f, 4.01f - 1.43f * std::log10(foldLen));
			vLaw = std::log2(f0 / 261.6255653f);   // dsp::FREQ_C4
		}
		float vIn = inputs[VOCT_INPUT].isConnected() ? inputs[VOCT_INPUT].getVoltage() : 0.f;
		outputs[VOCT_OUTPUT].setVoltage(params[SRC_PARAM].getValue() < 0.5f ? (vLaw + vIn) : vIn);

		// ---- Direct PT-sink channels ----
		outputs[TONGUEX_OUTPUT].setVoltage(bus[0]);
		outputs[TONGUEY_OUTPUT].setVoltage(bus[1]);
		outputs[THROATX_OUTPUT].setVoltage(bus[2]);
		outputs[FRICFC_OUTPUT].setVoltage(bus[3]);

		// ---- Creative fusions ----
		// Tense = knob * voicing (ch8, 0-10 V -> 0-1): voiceless consonants
		// drop PT into breathy air. The TENSE knob is the PT tenseness
		// source (cyberline's bus carries tension as a 0 V sink-owned
		// offset, so the adapter owns this control).
		float voicing = rack::math::clamp(bus[8] * 0.1f, 0.f, 1.f);
		float tense = params[TENSE_PARAM].getValue() * voicing;
		outputs[TENSE_OUTPUT].setVoltage(tense * 10.f);

		// Fricative level = FRIC (ch4) + pressure boost (ch7): PT has no
		// pressure CV, so pressure shades fricative energy instead.
		float press = rack::math::clamp(bus[7] * 0.1f, 0.f, 1.f);
		float fricLvl = rack::math::clamp(bus[4] * 0.1f + press * 0.35f, 0.f, 1.f);
		outputs[FRICLVL_OUTPUT].setVoltage(fricLvl * 10.f);

		// Velum monitor (no PT sink)
		outputs[VEL_OUTPUT].setVoltage(bus[5]);

		// BUS OK light: green when a phoneme neighbor is adjacent LEFT and
		// advertising PT mode (linked but wrong mode = half-bright amber-ish
		// signal via the same light at low brightness)
		lights[BUSOK_LIGHT].setBrightness(busOk ? 1.f : (expanderLinked ? 0.15f : 0.f));

		cpuMeter.frameEnd(measure, args.sampleRate);
	}

	void onReset() override {}

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "fold", json_real(params[FOLD_PARAM].getValue()));
		json_object_set_new(rootJ, "tense", json_real(params[TENSE_PARAM].getValue()));
		return rootJ;
	}
	void dataFromJson(json_t* rootJ) override {
		json_t* fJ = json_object_get(rootJ, "fold");
		if (fJ) params[FOLD_PARAM].setValue((float) json_real_value(fJ));
		json_t* tJ = json_object_get(rootJ, "tense");
		if (tJ) params[TENSE_PARAM].setValue((float) json_real_value(tJ));
	}
};

} // namespace cyberline
