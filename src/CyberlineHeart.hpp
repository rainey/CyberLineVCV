// CyberlineHeart — four-chambered pump modeled as one atrium → ventricle →
// artery chain (time-varying elastance, Suga–Sagawa family). Spec:
// docs/spec.md §5c. The SA node free-runs (the heart is an intrinsic LFO);
// an external pacer overdrives it; the Wiggers cycle, S1/S2 sounds,
// Frank–Starling law and murmurs are emergent. See CyberlineHeartCore.hpp
// for the DSP; this is the Rack wrapper.

#pragma once
#include <rack.hpp>
#include "CyberlineHeartCore.hpp"

namespace cyberline {

struct CyberlineHeart : rack::engine::Module {
	enum ParamIds {
		RATE_PARAM,
		MASS_PARAM,
		AVDELAY_PARAM,
		FILL_PARAM,
		DRIVE_PARAM,
		AFTERLOAD_PARAM,
		CONTRACT_PARAM,
		COMPLIANCE_PARAM,
		VALVE_PARAM,
		HRV_PARAM,
		IRRIT_PARAM,
		SOUNDLVL_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		PACING_INPUT,
		FILLCV_INPUT,
		DRIVECV_INPUT,
		AFTERLOADCV_INPUT,
		SYNC_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		VOLUME_OUTPUT,
		PRESSURE_OUTPUT,
		FLOW_OUTPUT,     // signed: + filling, - ejection
		OUTFLOW_OUTPUT,  // unipolar aortic flow pulse (series-link jack)
		ARTERIAL_OUTPUT,
		ECG_OUTPUT,
		BEAT_OUTPUT,
		ATRIA_OUTPUT,
		SOUND_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		FILL_LIGHT,    // cyan: AV valve open, filling
		EJECT_LIGHT,   // warm: aortic valve open, ejection
		NUM_LIGHTS
	};

	CyberlineHeartCore core;
	CyberlineHeartCore::Params cachedParams;

	CyberlineHeart() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(MASS_PARAM, 0.f, 1.f, 0.74f, "Heart mass", " kg",
			400000.f, 0.005f);  // log scale 5 g..2000 kg; 0.74 = 70 kg human
		configParam(RATE_PARAM, 30.f, 220.f, 72.f, "Intrinsic rate", " bpm");
		configParam(AVDELAY_PARAM, 0.06f, 0.25f, 0.15f, "AV delay", " s");
		configParam(FILL_PARAM, 0.f, 1.f, 0.5f, "Venous return (preload)");
		configParam(DRIVE_PARAM, -1.f, 1.f, 0.f, "Drive (vagal - / symp +)");
		configParam(AFTERLOAD_PARAM, 0.f, 1.f, 0.4f, "Afterload");
		configParam(CONTRACT_PARAM, 0.f, 1.f, 0.6f, "Contractility");
		configParam(COMPLIANCE_PARAM, 0.f, 1.f, 0.5f, "Compliance");
		configParam(VALVE_PARAM, -1.f, 1.f, 0.f, "Valve (stenosis - / regurg +)");
		configParam(HRV_PARAM, 0.f, 1.f, 0.15f, "Rate variability");
		configParam(IRRIT_PARAM, 0.f, 1.f, 0.f, "Irritability");
		configParam(SOUNDLVL_PARAM, 0.f, 1.f, 0.7f, "Sound level");
		configInput(PACING_INPUT, "Pacing (trigger: pacer spike)");
		configInput(FILLCV_INPUT, "Fill CV (0-10 V)");
		configInput(DRIVECV_INPUT, "Drive CV (signed: -5..+5 V)");
		configInput(AFTERLOADCV_INPUT, "Afterload CV (0-10 V)");
		configInput(SYNC_INPUT, "Sync / Reset (trigger: hard-resets cardiac cycle)");
		configOutput(VOLUME_OUTPUT, "Fill level (ventricular volume, 0-10 V)");
		configOutput(PRESSURE_OUTPUT, "Pressure (ventricular, 0-10 V)");
		configOutput(FLOW_OUTPUT, "Flow (signed CV: + fill, - eject)");
		configOutput(OUTFLOW_OUTPUT, "Outflow (aortic pulse, 0-5 V)");
		configOutput(ARTERIAL_OUTPUT, "Arterial (Windkessel, 0-10 V)");
		configOutput(ECG_OUTPUT, "ECG (bipolar)");
		configOutput(BEAT_OUTPUT, "Beat (trigger at QRS)");
		configOutput(ATRIA_OUTPUT, "Atria (trigger at P wave)");
		configOutput(SOUND_OUTPUT, "Sound (phonocardiogram, audio)");
		configLight(FILL_LIGHT, "Filling");
		configLight(EJECT_LIGHT, "Ejecting");
	}

	void process(const ProcessArgs& args) override {
		if (args.sampleRate != core.sampleRate)
			core.setSampleRate(args.sampleRate);

		if (core.physCounter + 1 >= CyberlineHeartCore::PHYS_INTERVAL) {
			cachedParams.mass = params[MASS_PARAM].getValue();
			cachedParams.rate = params[RATE_PARAM].getValue();
			cachedParams.avDelay = params[AVDELAY_PARAM].getValue();
			cachedParams.fill = params[FILL_PARAM].getValue();
			cachedParams.drive = params[DRIVE_PARAM].getValue();
			cachedParams.afterload = params[AFTERLOAD_PARAM].getValue();
			cachedParams.contract = params[CONTRACT_PARAM].getValue();
			cachedParams.compliance = params[COMPLIANCE_PARAM].getValue();
			cachedParams.valve = params[VALVE_PARAM].getValue();
			cachedParams.hrv = params[HRV_PARAM].getValue();
			cachedParams.irrit = params[IRRIT_PARAM].getValue();
			cachedParams.fillCv = inputs[FILLCV_INPUT].getVoltage();
			cachedParams.driveCv = inputs[DRIVECV_INPUT].getVoltage();
			cachedParams.afterloadCv = inputs[AFTERLOADCV_INPUT].getVoltage();
		}
		cachedParams.soundLvl = params[SOUNDLVL_PARAM].getValue();
		cachedParams.pacing = inputs[PACING_INPUT].getVoltage() >= 1.f;
		cachedParams.sync = inputs[SYNC_INPUT].getVoltage() >= 1.f;

		auto o = core.process(cachedParams);

		outputs[VOLUME_OUTPUT].setVoltage(
			rack::math::clamp(o.volume * 10.f, 0.f, 10.f));
		outputs[PRESSURE_OUTPUT].setVoltage(
			rack::math::clamp(o.pressure * 10.f, 0.f, 10.f));
		outputs[FLOW_OUTPUT].setVoltage(
			rack::math::clamp(o.flow * 5.f, -5.f, 5.f));   // signed, +-5 V
		outputs[OUTFLOW_OUTPUT].setVoltage(
			rack::math::clamp(o.outflow * 5.f, 0.f, 5.f));
		outputs[ARTERIAL_OUTPUT].setVoltage(
			rack::math::clamp(o.arterial * 10.f, 0.f, 10.f));
		outputs[ECG_OUTPUT].setVoltage(
			rack::math::clamp(o.ecg * 5.f, -5.f, 5.f));
		outputs[BEAT_OUTPUT].setVoltage(o.beat > 0.f ? 5.f : 0.f);
		outputs[ATRIA_OUTPUT].setVoltage(o.atria > 0.f ? 5.f : 0.f);
		outputs[SOUND_OUTPUT].setVoltage(
			rack::math::clamp(o.sound * 5.5f, -5.f, 5.f));

		lights[FILL_LIGHT].setSmoothBrightness(core.filling, args.sampleTime);
		lights[EJECT_LIGHT].setSmoothBrightness(core.systolic, args.sampleTime);
	}

	void onReset() override { core.reset(); }
};

} // namespace cyberline
