// CyberlineVoice — fused Larynx + Oral Chamber. Internally pre-patched;
// FEEDBACK GAIN closes the semi-coupled loop without an external cable.
// Shares DSP classes with the atoms (no duplicate implementations).
// Spec: docs/spec.md §5.

#pragma once
#include "CyberlineGlottis.hpp"
#include "CyberlineTract.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineVoice : rack::engine::Module {
	enum ParamIds {
		// larynx side
		V_TENSION_PARAM,
		V_PRESSURE_PARAM,
		V_FOLDLEN_PARAM,
		V_PITCHOCT_PARAM,
		V_DRIFT_PARAM,
		V_FB_PARAM,
		V_PCOUPLE_PARAM,
		// chamber side
		V_VTL_PARAM,
		V_TONGUEX_PARAM,
		V_TONGUEY_PARAM,
		V_VELUM_PARAM,
		V_SPEED_PARAM,
		V_QUALITY_PARAM,   // 0 = HIGH pinned, 1 = LOW pinned, 2 = FOLLOW GLOBAL (default); context-menu toggled
		NUM_PARAMS
	};
	enum InputIds {
		V_PITCH_INPUT,
		V_TENSION_INPUT,
		V_PRESSURE_INPUT,
		V_TONGUEX_INPUT,
		V_TONGUEY_INPUT,
		V_VELUM_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		V_LIPS_OUTPUT,
		V_NOSE_OUTPUT,
		V_VOICED_OUTPUT,   // pre-tract source (for feeding external filters)
		NUM_OUTPUTS
	};
	enum LightIds {
		V_PHASE_LIGHT,
		NUM_LIGHTS
	};

	CyberlineGlottis glottis;
	CyberlineTract tract;
	float vtlCm = 17.5f;
	float vtlSlew = 17.5f;      // slewed VTL (5 ms one-pole)
	float lastSampleRate = 0.f;
	float storedTension = 0.6f;
	float storedPressure = 0.8f;
	float lastFoldLen = -1.f;
	float cachedAllo = 235.f;
	int scanIndex = 0;

	CyberlineVoice() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(V_TENSION_PARAM, 0.f, 1.f, 0.6f, "Tension");
		configParam(V_PRESSURE_PARAM, 0.f, 1.f, 0.8f, "Pressure");
		configParam(V_FOLDLEN_PARAM, 2.f, 45.f, 14.f, "Fold length", " mm");
		configParam(V_PITCHOCT_PARAM, -2.f, 2.f, 0.f, "Octave");
		configParam(V_DRIFT_PARAM, 0.f, 1.f, 0.3f, "Drift");
		configParam(V_FB_PARAM, 0.f, 1.f, 0.f, "Tract feedback");
		configParam(V_PCOUPLE_PARAM, 0.f, 1.f, 0.3f, "Pressure-pitch coupling");
		configParam(V_VTL_PARAM, 6.f, 60.f, 17.5f, "Tract length", " cm");
		configParam(V_TONGUEX_PARAM, 0.f, 10.f, 5.f, "Tongue position");
		configParam(V_TONGUEY_PARAM, 0.f, 10.f, 5.f, "Tongue constriction");
		configParam(V_VELUM_PARAM, 0.f, 1.f, 0.01f, "Velum");
		configParam(V_SPEED_PARAM, 0.f, 1.f, 0.5f, "Muscle speed");
		configParam(V_QUALITY_PARAM, 0.f, 2.f, 2.f, "Quality override (context menu)");
		configInput(V_PITCH_INPUT, "Pitch (1 V/oct)");
		configInput(V_TENSION_INPUT, "Tension (0-10 V) / 2-3 ch Larynx Poly Bus (Ch 1: TENS, Ch 2: PRESS, Ch 3: VOICING)");
		configInput(V_PRESSURE_INPUT, "Pressure (0-10 V)");
		configInput(V_TONGUEX_INPUT, "Tongue X (0-10 V) / 8-9 ch Tract+Voice Poly Bus (Ch 1-6: Tract, Ch 7-8: Tension/Pressure, Ch 9: Voicing)");
		configInput(V_TONGUEY_INPUT, "Tongue Y (0-10 V)");
		configInput(V_VELUM_INPUT, "Velum (0-10 V)");
		configOutput(V_LIPS_OUTPUT, "Lips");
		configOutput(V_NOSE_OUTPUT, "Nose");
		configOutput(V_VOICED_OUTPUT, "Source (pre-tract)");
	}

	void process(const ProcessArgs& args) override;
	void onReset() override {
		glottis.reset();
		tract.reset();
		vtlCm = 17.5f;
		vtlSlew = 17.5f;
		lastSampleRate = 0.f;
		lastFoldLen = -1.f;
		scanIndex = 0;
	}
};

} // namespace cyberline
