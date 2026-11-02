// CyberlineLungs — breath-pressure reservoir / physically-shaped envelope
// generator. Spec: docs/spec.md §5b.
//
// Model: tidal volume V (0..1) filled by inhale, drained by exhale. Output
// pressure P = elastic recoil + regulated effort. Three modes: CYCLE
// (free breathing at RATE), TRIG (one-shot on trigger), CLOSED (air retained
// across breaths — durations shrink until refilled, whale-style).
//
// Physics block runs at control rate (32-sample cadence); outputs are
// interpolated per sample. Signed-CV convention applies to FLOW.

#pragma once
#include <rack.hpp>

namespace cyberline {

struct CyberlineLungs : rack::engine::Module {
	enum ParamIds {
		RATE_PARAM,
		DEPTH_PARAM,
		INHALE_PARAM,
		MAXFLOW_PARAM,
		EFFORT_PARAM,
		MODE_PARAM,     // 3-position switch: CYCLE / TRIG / CLOSED
		NUM_PARAMS
	};
	enum InputIds {
		TRIGGER_INPUT,
		GATE_INPUT,
		DEPTHCV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		PRESSURE_OUTPUT,
		VOLUME_OUTPUT,
		FLOW_OUTPUT,    // signed: + inhale, - exhale
		PHASE_OUTPUT,   // gate, high during exhale
		NUM_OUTPUTS
	};
	enum LightIds {
		PHASE_LIGHT,    // exhale
		NUM_LIGHTS
	};

	enum Mode { CYCLE = 0, TRIG = 1, CLOSED = 2 };

	// State (integrated at control rate)
	float volume = 0.f;      // 0..1 reservoir fill
	float pressure = 0.f;    // 0..1 output value (pre 10V scale)
	float flow = 0.f;        // -1..1 signed flow
	float phase = 0.f;       // 1 during exhale, 0 inhale
	bool exhaling = false;
	bool gateWasHigh = false;
	bool trigWasHigh = false;
	float cycleTimer = 0.f;  // s since current breath started (CYCLE period floor)
	int inhaleHold = 0;      // control ticks remaining for inhale phase

	// Output interpolation (per sample)
	float outPressure = 0.f, outVolume = 0.f, outFlow = 0.f, outPhase = 0.f;
	float tgtPressure = 0.f, tgtVolume = 0.f, tgtFlow = 0.f, tgtPhase = 0.f;

	static constexpr int PHYS_INTERVAL = 32;
	int physCounter = 0;

	CyberlineLungs() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(RATE_PARAM, 4.f, 30.f, 12.f, "Breath rate", " /min");
		configParam(DEPTH_PARAM, 0.f, 1.f, 0.7f, "Depth (tidal volume)");
		configParam(INHALE_PARAM, 0.05f, 3.f, 0.4f, "Inhale time", " s");
		configParam(MAXFLOW_PARAM, 0.f, 1.f, 0.5f, "Max flow (gasp-calm)");
		configParam(EFFORT_PARAM, 0.f, 1.f, 0.5f, "Effort (recoil-plateau)");
		configParam(MODE_PARAM, 0.f, 2.f, 0.f, "Mode");
		configInput(TRIGGER_INPUT, "Trigger (one-shot)");
		configInput(GATE_INPUT, "Gate (inhale while high)");
		configInput(DEPTHCV_INPUT, "Depth CV (0-10 V)");
		configOutput(PRESSURE_OUTPUT, "Pressure (0-10 V)");
		configOutput(VOLUME_OUTPUT, "Volume (CV)");
		configOutput(FLOW_OUTPUT, "Flow (signed CV: + inhale, - exhale)");
		configOutput(PHASE_OUTPUT, "Phase (gate: exhale)");
	}

	void process(const ProcessArgs& args) override;

	void reset() {
		volume = 0.f; pressure = 0.f; flow = 0.f; phase = 0.f;
		exhaling = false; inhaleHold = 0; cycleTimer = 0.f;
		outPressure = outVolume = outFlow = outPhase = 0.f;
		tgtPressure = tgtVolume = tgtFlow = tgtPhase = 0.f;
	}

	void onReset() override { reset(); }
};

} // namespace cyberline
