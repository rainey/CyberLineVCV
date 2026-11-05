// CyberlineSagitta — vocal-system fluoroscopy (display-only module).
// Spec: docs/spec.md §5f.
//
// A dedicated visualizer for the Lungs -> Larynx -> Oral Chamber pipeline.
// It taps the same CV contracts the real consumers use — Broca's tract /
// larynx poly buses and the Lungs outputs — and runs its own replica
// CyberlineTract waveguide from the tapped values (see
// CyberlineSagittaCore). The widget draws a side-view anatomical diagram
// from the core's telemetry snapshot.
//
// Pure display: no outputs. Patch Broca's buses (or any CVs) into it in
// parallel with the audio chain.

#pragma once
#include <rack.hpp>
#include "CyberlineSagittaCore.hpp"

namespace cyberline {

struct CyberlineSagitta : rack::engine::Module {
	enum ParamIds {
		GAIN_PARAM,     // voiced audio gain 0..2
		MODE_PARAM,     // 2-position: BUS (Broca offsets) / DIRECT (absolute)
		NUM_PARAMS
	};
	enum InputIds {
		TRACT_INPUT,    // 6-9 ch poly bus (TNGX TNGY VEL CPOS CWID FRIC [TENS PRESS VOIC])
		LARYNX_INPUT,   // 3 ch poly bus (TENS PRESS VOIC)
		LUNGS_P_INPUT,  // Lungs PRESSURE (-5..+10 V)
		LUNGS_V_INPUT,  // Lungs VOLUME (0..10 V)
		LUNGS_F_INPUT,  // Lungs FLOW (signed ±5 V)
		VOICED_INPUT,   // Larynx VOICED (audio; drives the replica source)
		NUM_INPUTS
	};
	enum OutputIds {
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	CyberlineSagittaCore core;

	CyberlineSagitta() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(GAIN_PARAM, 0.f, 2.f, 1.f, "Voiced gain");
		configSwitch(MODE_PARAM, 0.f, 1.f, 0.f, "CV mode",
			{"Bus (Broca)", "Direct"});
		configInput(TRACT_INPUT, "Tract Poly Bus 6-9 ch (TNGX TNGY VEL CPOS CWID FRIC [TENS PRESS VOIC])");
		configInput(LARYNX_INPUT, "Larynx Poly Bus 3 ch (TENS PRESS VOIC)");
		configInput(LUNGS_P_INPUT, "Lungs pressure (-5..+10 V)");
		configInput(LUNGS_V_INPUT, "Lungs volume (0..10 V)");
		configInput(LUNGS_F_INPUT, "Lungs flow (signed ±5 V)");
		configInput(VOICED_INPUT, "Voiced (audio)");
	}

	void process(const ProcessArgs& args) override {
		core.gain = params[GAIN_PARAM].getValue();
		core.busMode = params[MODE_PARAM].getValue() < 0.5f;

		float tb[9];
		int tc = inputs[TRACT_INPUT].getChannels();
		if (tc > 9) tc = 9;
		for (int i = 0; i < tc; i++) tb[i] = inputs[TRACT_INPUT].getVoltage(i);

		float lb[3];
		int lc = inputs[LARYNX_INPUT].getChannels();
		if (lc > 3) lc = 3;
		for (int i = 0; i < lc; i++) lb[i] = inputs[LARYNX_INPUT].getVoltage(i);

		core.process(tb, tc, lb, lc,
			inputs[LUNGS_P_INPUT].getVoltage(),
			inputs[LUNGS_V_INPUT].getVoltage(),
			inputs[LUNGS_F_INPUT].getVoltage(),
			inputs[VOICED_INPUT].getVoltage() * core.gain,
			args.sampleTime);
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		core.setSampleRate(e.sampleRate);
	}

	void onReset() override {
		core.reset();
	}
};

} // namespace cyberline
