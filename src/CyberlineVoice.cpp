// CyberlineVoice — module process.

#include "CyberlineVoice.hpp"

namespace cyberline {

void CyberlineVoice::process(const ProcessArgs& args) {
	if (args.sampleRate != lastSampleRate) {
		lastSampleRate = args.sampleRate;
		glottis.setSampleRate(args.sampleRate);
		tract.setGeometry(vtlCm, args.sampleRate);
	}

	// ---- block-rate parameters (every 16 samples) ----
	if ((scanIndex++ & 15) == 0) {
		float tension = rack::math::clamp(
			params[V_TENSION_PARAM].getValue() + inputs[V_TENSION_INPUT].getVoltage() * 0.1f,
			0.f, 1.f);
		float pKnob = params[V_PRESSURE_PARAM].getValue();
		float pressure = inputs[V_PRESSURE_INPUT].isConnected()
			? rack::math::clamp(pKnob * inputs[V_PRESSURE_INPUT].getVoltage() * 0.1f, -1.f, 1.f)
			: rack::math::clamp(pKnob, 0.f, 1.f);
		storedTension = tension;
		storedPressure = pressure;

		float foldLen = params[V_FOLDLEN_PARAM].getValue();
		if (foldLen != lastFoldLen) {
			lastFoldLen = foldLen;
			cachedAllo = std::pow(10.f, 4.01f - 1.43f * std::log10(foldLen));
		}
		float v = inputs[V_PITCH_INPUT].getVoltage();
		float oct = std::round(params[V_PITCHOCT_PARAM].getValue());
		float f0 = cachedAllo * std::pow(2.f, v + oct)
			* std::sqrt(std::fmax(storedTension, 0.01f) / 0.6f);
		f0 += params[V_PCOUPLE_PARAM].getValue() * std::fabs(storedPressure) * 30.f;
		glottis.setTargets(f0, storedTension, storedPressure, foldLen);

		float vtl = params[V_VTL_PARAM].getValue();
		if (vtl != vtlCm) {
			vtlCm = vtl;
			tract.setGeometry(vtlCm, args.sampleRate);
		}
		float tx = rack::math::clamp(params[V_TONGUEX_PARAM].getValue()
			+ inputs[V_TONGUEX_INPUT].getVoltage(), 0.f, 10.f) / 10.f;
		float ty = rack::math::clamp(params[V_TONGUEY_PARAM].getValue()
			+ inputs[V_TONGUEY_INPUT].getVoltage(), 0.f, 10.f) / 10.f;
		tract.setTongue(tract.bladeStart + tx * (tract.tipStart - tract.bladeStart),
			CyberlineTract::mapTongueYDiameter(ty));
		tract.setVelum(rack::math::clamp(params[V_VELUM_PARAM].getValue()
			+ inputs[V_VELUM_INPUT].getVoltage() * 0.1f, 0.f, 1.f));
		tract.muscleSpeed = 2.f + params[V_SPEED_PARAM].getValue() * 38.f;
		tract.applyConstriction();
	}

	// ---- semi-coupled loop: tract probe -> glottis feedback ----
	float probeP = (tract.R[tract.n / 3] + tract.L[tract.n / 3])
		* params[V_FB_PARAM].getValue();
	if (!std::isfinite(probeP)) probeP = 0.f;

	float noise = rack::random::uniform() * 2.f - 1.f;
	float voiced = glottis.process(noise, probeP,
		params[V_DRIFT_PARAM].getValue(), 6.f);

	// ---- tract ----
	tract.reshape(args.sampleTime);
	tract.calculateReflectionsPublic();
	float exc = voiced;
	float lip = tract.step(exc, 0.f, 0.f, noise, 0.5f);
	lip = tract.step(exc, 0.f, 0.f, noise, 1.0f);
	float nose = tract.lastNoseOutput;

	outputs[V_VOICED_OUTPUT].setVoltage(rack::math::clamp(voiced * 5.f, -5.f, 5.f));
	outputs[V_LIPS_OUTPUT].setVoltage(rack::math::clamp(lip * 5.f, -5.f, 5.f));
	outputs[V_NOSE_OUTPUT].setVoltage(rack::math::clamp(nose * 5.f, -5.f, 5.f));
	lights[V_PHASE_LIGHT].setSmoothBrightness(
		glottis.noiseModulator, args.sampleTime);
}

} // namespace cyberline
