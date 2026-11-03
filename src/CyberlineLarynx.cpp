// CyberlineLarynx — module process.

#include "CyberlineLarynx.hpp"

namespace cyberline {

void CyberlineLarynx::process(const ProcessArgs& args) {
	if (glottis.sampleRate != args.sampleRate)
		glottis.setSampleRate(args.sampleRate);

	// Evaluate pitch/allometry targets at glottis control cadence
	if (glottis.smoothCounter + 1 >= CyberlineGlottis::SMOOTH_INTERVAL
			|| lastFoldLen < 0.f) {
		// CV + param mix, 0..10 V -> 0..1
		float tension = rack::math::clamp(
			params[TENSION_PARAM].getValue()
			+ inputs[TENSION_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
		float pKnob = params[PRESSURE_PARAM].getValue();
		float pressure = inputs[PRESSURE_INPUT].isConnected()
			? rack::math::clamp(pKnob * inputs[PRESSURE_INPUT].getVoltage() * 0.1f, -1.f, 1.f)
			: rack::math::clamp(pKnob, 0.f, 1.f);

		// Allometric base frequency from fold length (mm):
		// log10(F0) = -1.43 * log10(L_mm) + 4.01   (mammal mean-F0 law)
		float foldLen = params[FOLDLEN_PARAM].getValue();
		if (foldLen != lastFoldLen) {
			lastFoldLen = foldLen;
			cachedAllo = std::pow(10.f, 4.01f - 1.43f * std::log10(foldLen));
		}

		// Musical F0: allometric base x 2^(V/oct + octave) x sqrt-tension scaling
		float v = inputs[PITCH_INPUT].getVoltage();
		float oct = std::round(params[OCT_PARAM].getValue());
		float f0 = cachedAllo * std::pow(2.f, v + oct)
			* std::sqrt(std::fmax(tension, 0.01f) / 0.6f);

		// Pressure -> pitch coupling, additive in Hz (2-3 Hz per cmH2O in vivo,
		// scaled: full pressure ~= +30 Hz at coupling max)
		f0 += params[PCOUPLE_PARAM].getValue() * std::fabs(pressure) * 30.f;

		glottis.setTargets(f0, tension, pressure, foldLen);
	}

	float noise = rack::random::uniform() * 2.f - 1.f;
	float fb = inputs[FEEDBACK_INPUT].getVoltage() * 0.2f
		* params[FBGAIN_PARAM].getValue();

	float out = glottis.process(noise, fb,
		params[DRIFT_PARAM].getValue(), params[VIBRATE_PARAM].getValue());

	outputs[VOICED_OUTPUT].setVoltage(rack::math::clamp(out * 5.f, -5.f, 5.f));
	outputs[NOISEMOD_OUTPUT].setVoltage(
		rack::math::clamp(glottis.noiseModulator * 10.f, 0.f, 5.f));
}

} // namespace cyberline
