// CyberlineCantor — Musical singer LF-glottis voice source module.
// Tracks 1V/oct accurately via inverse-allometric effective fold length,
// decouples tension from pitch (timbre-only Rd control), provides a MASS
// vocal-weight control, exponential pressure-pitch bend, and external
// oscillator CARRIER blending + optional glottal hard-sync.

#pragma once
#include "CyberlineGlottis.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineCantor : rack::engine::Module {
	enum ParamIds {
		TENSION_PARAM,
		PRESSURE_PARAM,
		TUNE_PARAM,      // -7..+7 semitones fine/interval tune
		MASS_PARAM,      // 0.25..4.0x vocal fold mass multiplier (1.0 = neutral)
		OCT_PARAM,       // -2..+2 octaves
		DRIFT_PARAM,
		VIBRATE_PARAM,
		CARRMIX_PARAM,   // 0..1 blend between internal LF glottal pulse and CARRIER input
		PCOUPLE_PARAM,   // 0..1 exponential breath pitch bend (up to +2 semitones)
		TENSIONCV_PARAM, // 0..1 Tension CV attenuator
		QUALITY_PARAM,   // 0 = HIGH pinned, 1 = LOW pinned, 2 = FOLLOW GLOBAL (default)
		NUM_PARAMS
	};
	enum InputIds {
		PITCH_INPUT,
		TENSION_INPUT,
		PRESSURE_INPUT,
		CARRIER_INPUT,
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

	enum RefTuning {
		REF_C4 = 0, // 261.6256 Hz (VCV standard)
		REF_C3 = 1, // 130.8128 Hz (Vocal baritone/alto center)
		REF_A3 = 2, // 220.0000 Hz
		REF_C2 = 3  // 65.4064 Hz  (Bass / drone)
	};

	CyberlineGlottis glottis;
	CyberlineCpuMeter cpuMeter;
	int refTuning = REF_C4;
	bool carrierHardSync = false;
	float lastCarrierSample = 0.f;
	float telemetryF0 = 261.6256f;
	float telemetryEffFoldMm = 12.82f;
	bool firstFrame = true;

	CyberlineCantor() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(TENSION_PARAM, 0.f, 1.f, 0.6f, "Tension (timbre only)");
		configParam(PRESSURE_PARAM, 0.f, 1.f, 0.8f, "Pressure");
		configParam(TUNE_PARAM, -7.f, 7.f, 0.f, "Fine tune", " st");
		configParam(MASS_PARAM, -1.f, 1.f, 0.f, "Vocal mass (light <-> heavy)");
		configParam<OctaveParamQuantity>(OCT_PARAM, -2.f, 2.f, 0.f, "Octave");
		configParam(DRIFT_PARAM, 0.f, 1.f, 0.15f, "Drift");
		configParam(VIBRATE_PARAM, 0.f, 12.f, 6.f, "Vibrato rate", " Hz");
		configParam(CARRMIX_PARAM, 0.f, 1.f, 0.f, "External carrier mix", "%", 0.f, 100.f);
		configParam(PCOUPLE_PARAM, 0.f, 1.f, 0.1f, "Pressure-pitch bend", " st", 0.f, 2.f);
		configParam(TENSIONCV_PARAM, 0.f, 1.f, 1.f, "Tension CV level", "%", 0.f, 100.f);
		configParam(QUALITY_PARAM, 0.f, 2.f, 2.f, "Quality override (context menu)");

		configInput(PITCH_INPUT, "Pitch (1 V/oct)");
		configInput(TENSION_INPUT, "Tension (0-10 V) / 2-3 ch Larynx Poly Bus (Ch 1: TENS, Ch 2: PRESS, Ch 3: VOICING)");
		configInput(PRESSURE_INPUT, "Pressure (0-10 V)");
		configInput(CARRIER_INPUT, "External oscillator carrier (audio)");

		configOutput(VOICED_OUTPUT, "Voiced");
		configOutput(NOISEMOD_OUTPUT, "Noise modulator (CV)");
	}

	float getRefFreq() const {
		switch (refTuning) {
			case REF_C3: return 130.81278f;
			case REF_A3: return 220.0f;
			case REF_C2: return 65.40639f;
			case REF_C4:
			default:     return 261.62557f;
		}
	}

	void process(const ProcessArgs& args) override;

	json_t* dataToJson() override {
		json_t* rootJ = json_object();
		json_object_set_new(rootJ, "refTuning", json_integer(refTuning));
		json_object_set_new(rootJ, "carrierHardSync", json_boolean(carrierHardSync));
		return rootJ;
	}

	void dataFromJson(json_t* rootJ) override {
		if (json_t* rJ = json_object_get(rootJ, "refTuning"))
			refTuning = rack::math::clamp((int) json_integer_value(rJ), 0, 3);
		if (json_t* sJ = json_object_get(rootJ, "carrierHardSync"))
			carrierHardSync = json_boolean_value(sJ);
	}

	void onReset() override {
		glottis.reset();
		refTuning = REF_C4;
		carrierHardSync = false;
		lastCarrierSample = 0.f;
		firstFrame = true;
		telemetryF0 = 261.6256f;
		telemetryEffFoldMm = 12.82f;
	}
};

} // namespace cyberline
