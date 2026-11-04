// CyberlineLarynx — LF-glottis voice source module.
// Spec: docs/spec.md §3.

#pragma once
#include "CyberlineGlottis.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineLarynx : rack::engine::Module {
	enum ParamIds {
		TENSION_PARAM,
		PRESSURE_PARAM,
		FOLDLEN_PARAM,
		OCT_PARAM,
		DRIFT_PARAM,
		VIBRATE_PARAM,
		FBGAIN_PARAM,
		PCOUPLE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_INPUT,
		TENSION_INPUT,
		PRESSURE_INPUT,
		FEEDBACK_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		VOICED_OUTPUT,
		NOISEMOD_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	CyberlineGlottis glottis;
	float lastFoldLen = -1.f;
	float cachedAllo = 235.f;

	CyberlineLarynx() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(TENSION_PARAM, 0.f, 1.f, 0.6f, "Tension");
		configParam(PRESSURE_PARAM, 0.f, 1.f, 0.8f, "Pressure");
		configParam(FOLDLEN_PARAM, 2.f, 45.f, 14.f, "Fold length", " mm");
		configParam(OCT_PARAM, -2.f, 2.f, 0.f, "Octave");
		configParam(DRIFT_PARAM, 0.f, 1.f, 0.3f, "Drift");
		configParam(VIBRATE_PARAM, 0.f, 12.f, 6.f, "Vibrato rate", " Hz");
		configParam(FBGAIN_PARAM, 0.f, 1.f, 0.f, "Feedback gain");
		configParam(PCOUPLE_PARAM, 0.f, 1.f, 0.3f, "Pressure-pitch coupling");
		configInput(PITCH_INPUT, "Pitch (1 V/oct)");
		configInput(TENSION_INPUT, "Tension (0-10 V) / 2-3 ch Larynx Poly Bus (Ch 1: TENS, Ch 2: PRESS, Ch 3: VOICING)");
		configInput(PRESSURE_INPUT, "Pressure (0-10 V)");
		configInput(FEEDBACK_INPUT, "Tract feedback (audio)");
		configOutput(VOICED_OUTPUT, "Voiced");
		configOutput(NOISEMOD_OUTPUT, "Noise modulator (CV)");
	}

	void process(const ProcessArgs& args) override;
	void onReset() override {
		glottis.reset();
		lastFoldLen = -1.f;
	}
};

} // namespace cyberline
