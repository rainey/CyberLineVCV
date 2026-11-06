// CyberlineOralChamber — KL waveguide tract module.
// Spec: docs/spec.md §4.

#pragma once
#include "CyberlineTract.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineOralChamber : rack::engine::Module {
	enum ParamIds {
		VTL_PARAM,
		TONGUEX_PARAM,
		TONGUEY_PARAM,
		VELUM_PARAM,
		CONSTRPOS_PARAM,
		CONSTRW_PARAM,
		FRIC_PARAM,
		INJPOS_PARAM,
		SPEED_PARAM,
		WALL_PARAM,
		QUALITY_PARAM,  // 0 = HIGH pinned, 1 = LOW pinned, 2 = FOLLOW GLOBAL (default); context-menu toggled
		NUM_PARAMS
	};
	enum InputIds {
		SOURCE_INPUT,
		INJECT_INPUT,
		TONGUEX_INPUT,
		TONGUEY_INPUT,
		VELUM_INPUT,
		CONSTRPOS_INPUT,
		CONSTRW_INPUT,
		FRIC_INPUT,
		INJPOS_INPUT,
		VTL_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		LIPS_OUTPUT,
		NOSE_OUTPUT,
		SHAPE_OUTPUT,
		PROBE_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	CyberlineTract tract;
	float vtlCm = 17.5f;
	float lastSampleRate = 0.f;
	float vtlSlew = 17.5f;      // slewed VTL (5 ms one-pole) — smooths CV
	                            // modulation so geometry rebuilds never jitter

	// SHAPE scan: one section amplitude per audio sample, cycling
	int scanIndex = 0;
	int blockCounter = 0;

	CyberlineOralChamber() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(VTL_PARAM, 6.f, 60.f, 17.5f, "Tract length", " cm");
		configParam(TONGUEX_PARAM, 0.f, 10.f, 5.f, "Tongue position");
		configParam(TONGUEY_PARAM, 0.f, 10.f, 5.f, "Tongue constriction");
		configParam(VELUM_PARAM, 0.f, 1.f, 0.01f, "Velum");
		configParam(CONSTRPOS_PARAM, 0.f, 10.f, 0.f, "Constriction position");
		configParam(CONSTRW_PARAM, 0.f, 10.f, 5.f, "Constriction width");
		configParam(FRIC_PARAM, 0.f, 1.f, 0.f, "Fricative");
		configParam(INJPOS_PARAM, 0.f, 10.f, 0.f, "Injection position");
		configParam(SPEED_PARAM, 0.f, 1.f, 0.5f, "Muscle speed");
		configParam(WALL_PARAM, 0.98f, 1.f, 0.999f, "Wall damping");
		configInput(SOURCE_INPUT, "Source (audio)");
		configInput(INJECT_INPUT, "Inject (audio)");
		configInput(TONGUEX_INPUT, "Tongue X (0-10 V) / 6-ch Tract Poly Bus (TNGX, TNGY, VEL, CPOS, CWID, FRIC)");
		configInput(TONGUEY_INPUT, "Tongue Y (0-10 V)");
		configInput(VELUM_INPUT, "Velum (0-10 V)");
		configInput(CONSTRPOS_INPUT, "Constriction position (0-10 V)");
		configInput(CONSTRW_INPUT, "Constriction width (0-10 V)");
		configInput(FRIC_INPUT, "Fricative (0-10 V)");
		configInput(INJPOS_INPUT, "Injection position CV");
		configInput(VTL_INPUT, "Tract length CV (5.4 cm/V, ±10 V)");
		configOutput(LIPS_OUTPUT, "Lips");
		configOutput(NOSE_OUTPUT, "Nose");
		configOutput(SHAPE_OUTPUT, "Shape scan (CV)");
		configOutput(PROBE_OUTPUT, "Probe (CV)");
		configParam(QUALITY_PARAM, 0.f, 2.f, 2.f, "Quality override (context menu)");
	}

	void process(const ProcessArgs& args) override;
	void onReset() override {
		tract.reset();
		vtlCm = 17.5f;
		vtlSlew = 17.5f;
		lastSampleRate = 0.f;
		scanIndex = 0;
		blockCounter = 0;
	}
};

} // namespace cyberline
