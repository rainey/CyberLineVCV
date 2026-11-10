// CyberlineLarynx — LF-glottis voice source module.
// Spec: docs/spec.md §3.

#pragma once
#include "CyberlineGlottis.hpp"
#include "CyberlineModels.hpp"
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
		TENSIONCV_PARAM,
		QUALITY_PARAM,  // 0 = HIGH pinned, 1 = LOW pinned, 2 = FOLLOW GLOBAL (default); context-menu toggled
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
	CyberlineCpuMeter cpuMeter;   // plugin-side CPU % ring (see CyberlineModels.hpp)
	float cachedAllo = 235.f;

	CyberlineLarynx() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(TENSION_PARAM, 0.f, 1.f, 0.6f, "Tension");
		configParam(PRESSURE_PARAM, 0.f, 1.f, 0.8f, "Pressure");
		configParam(FOLDLEN_PARAM, 2.f, 45.f, 14.f, "Fold length", " mm");
		configParam<OctaveParamQuantity>(OCT_PARAM, -2.f, 2.f, 0.f, "Octave");
		configParam(DRIFT_PARAM, 0.f, 1.f, 0.3f, "Drift");
		configParam(VIBRATE_PARAM, 0.f, 12.f, 6.f, "Vibrato rate", " Hz");
		configParam(FBGAIN_PARAM, 0.f, 1.f, 0.f, "Feedback gain");
		configParam(PCOUPLE_PARAM, 0.f, 1.f, 0.3f, "Pressure-pitch coupling");
		configParam(TENSIONCV_PARAM, 0.f, 1.f, 1.f, "Tension CV level", "%", 0.f, 100.f);
		configParam(QUALITY_PARAM, 0.f, 2.f, 2.f, "Quality override (context menu)");
		configInput(PITCH_INPUT, "Pitch (1 V/oct)");
		configInput(TENSION_INPUT, "Tension (0-10 V) / 2-3 ch Larynx Poly Bus (Ch 1: TENS, Ch 2: PRESS, Ch 3: VOICING)");
		configInput(PRESSURE_INPUT, "Pressure (0-10 V)");
		configInput(FEEDBACK_INPUT, "Tract feedback (audio)");
		configOutput(VOICED_OUTPUT, "Voiced");
		configOutput(NOISEMOD_OUTPUT, "Noise modulator (CV)");
	}

	void process(const ProcessArgs& args) override;
	// Persist the size-band choice (Rack restores params first, then calls
	// dataFromJson, so re-applying the range here lands after the param load).
	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "foldBand", json_integer(deriveBandIndex(this, FOLDLEN_PARAM, 0)));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		json_t* bJ = json_object_get(rootJ, "foldBand");
		if (bJ) {
			int band = (int) json_integer_value(bJ);
			if (band >= 0 && band < NUM_SIZE_BANDS) {
				applyParamRange(paramQuantities[FOLDLEN_PARAM], SIZE_BANDS[band].foldLo, SIZE_BANDS[band].foldHi);
			}
		}
	}

	void onReset() override {
		glottis.reset();
		lastFoldLen = -1.f;
	}
};

} // namespace cyberline
