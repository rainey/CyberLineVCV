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
		MODE_PARAM,     // 4-position switch: CYCLE / TRIG / CLOSED / PHASOR
		INHMUTE_PARAM,  // 2-position switch: 0 = Suction (-P / gasp), 1 = Inhale Mute (0 V)
		NUM_PARAMS
	};
	enum InputIds {
		TRIGGER_INPUT,
		GATE_INPUT,
		DEPTHCV_INPUT,
		SYNC_INPUT,
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

	enum Mode { CYCLE = 0, TRIG = 1, CLOSED = 2, PHASOR = 3 };

	// State (integrated at control rate)
	float volume = 0.f;      // 0..1 reservoir fill
	float pressure = 0.f;    // -0.5..1 output value (pre 10V scale)
	float flow = 0.f;        // -1..1 signed flow
	float phase = 0.f;       // 1 during exhale, 0 inhale/hold
	bool inhaling = false;
	bool exhaling = false;
	float closedBurstTarget = 0.f;
	bool gateWasHigh = false;
	bool trigWasHigh = false;
	bool syncWasHigh = false;
	bool trigPending = false;
	bool syncPending = false;
	bool gateRisePending = false;
	bool gateFallPending = false;
	float cycleTimer = 999.f;  // s since current breath started (starts immediately in CYCLE)

	// Output interpolation (per sample)
	float outPressure = 0.f, outVolume = 0.f, outFlow = 0.f, outPhase = 0.f;
	float tgtPressure = 0.f, tgtVolume = 0.f, tgtFlow = 0.f, tgtPhase = 0.f;
	float lastPhasorVolume = 0.f; // PHASOR mode: previous tick's volume (dV/dt)

	static constexpr int PHYS_INTERVAL = 32;
	int physCounter = 0;

	CyberlineLungs() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(RATE_PARAM, 4.f, 30.f, 12.f, "Breath rate", " /min");
		configParam(DEPTH_PARAM, 0.f, 1.f, 0.7f, "Depth (tidal volume)");
		configParam(INHALE_PARAM, 0.05f, 3.f, 0.4f, "Inhale time", " s");
		configParam(MAXFLOW_PARAM, 0.f, 1.f, 0.4f, "Exhale flow rate (conserve-vent)");
		configParam(EFFORT_PARAM, 0.f, 1.f, 0.65f, "Effort (sigh recoil - singer plateau)");
		configSwitch(MODE_PARAM, 0.f, 3.f, 0.f, "Mode",
			{"Cycle", "Trigger", "Closed", "Phasor"});
		configSwitch(INHMUTE_PARAM, 0.f, 1.f, 1.f, "Inhale pressure",
			{"Suction (-P / gasp)", "Mute (0 V on inhale)"});
		configInput(TRIGGER_INPUT, "Trigger (catch-breath / one-shot / Closed refill)");
		configInput(GATE_INPUT, "Gate (breath hold / Closed release valve / Phasor CV)");
		configInput(DEPTHCV_INPUT, "Depth CV (0-10 V)");
		configInput(SYNC_INPUT, "Sync / Reset (trigger: hard-resets breath cycle from 0)");
		configOutput(PRESSURE_OUTPUT, "Pressure (-5..+10 V or 0..+10 V)");
		configOutput(VOLUME_OUTPUT, "Volume (CV)");
		configOutput(FLOW_OUTPUT, "Flow (signed CV: + inhale, - exhale)");
		configOutput(PHASE_OUTPUT, "Phase (gate: exhale)");
	}

	void process(const ProcessArgs& args) override;

	void reset() {
		volume = 0.f; pressure = 0.f; flow = 0.f; phase = 0.f;
		inhaling = false; exhaling = false;
		closedBurstTarget = 0.f; cycleTimer = 999.f;
		gateWasHigh = trigWasHigh = syncWasHigh = false;
		trigPending = syncPending = gateRisePending = gateFallPending = false;
		lastPhasorVolume = 0.f;
		outPressure = outVolume = outFlow = outPhase = 0.f;
		tgtPressure = tgtVolume = tgtFlow = tgtPhase = 0.f;
		physCounter = 0;
	}

	void onReset() override { reset(); }
};

} // namespace cyberline
