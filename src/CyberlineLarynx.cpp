// CyberlineLarynx — module process.

#include "CyberlineLarynx.hpp"
#include "CyberlineQuality.hpp"
#include "CyberlineDspGuard.hpp"

namespace cyberline {

void CyberlineLarynx::process(const ProcessArgs& args) {
	bool measure = cpuMeter.frameStart();
	// Subnormal protection (glottis one-poles decay to silence between
	// phonemes; FTZ/DAZ keeps the FPU off the microcode-assist path).
	DenormalGuard denormalGuard;

	// Quality: module override (hidden param) resolved against the
	// plugin-wide global (default LOW). LOW enables the incremental-
	// exponential waveform and the fast aspiration-gate sine (~0.0016 max
	// sine error, inaudible in a noise-gate modulation).
	glottis.lowQuality = CyberlineQuality::effectiveLow(
		params[QUALITY_PARAM].getValue());

	if (glottis.sampleRate != args.sampleRate)
		glottis.setSampleRate(args.sampleRate);

	// Evaluate pitch/allometry targets at glottis control cadence
	if (glottis.smoothCounter + 1 >= CyberlineGlottis::SMOOTH_INTERVAL
			|| lastFoldLen < 0.f) {
		// CV + param mix, 0..10 V -> 0..1
		// TENS/POLY (TENSION_INPUT): Ch 1 = Tension CV; Ch 2 = Pressure scale when PRESS is unpatched;
		// Ch 3 = Voicing (0-10 V -> 0..1 fold vibration; absent = fully voiced)
		float voicingAmt = (inputs[TENSION_INPUT].getChannels() >= 3)
			? rack::math::clamp(inputs[TENSION_INPUT].getVoltage(2) * 0.1f, 0.f, 1.f)
			: 1.f;
		float tensCvScale = params[TENSIONCV_PARAM].getValue();
		float tension = rack::math::clamp(
			params[TENSION_PARAM].getValue()
			+ inputs[TENSION_INPUT].getVoltage(0) * 0.1f * tensCvScale, 0.f, 1.f);
		float pKnob = params[PRESSURE_PARAM].getValue();
		float pressure;
		if (inputs[PRESSURE_INPUT].isConnected()) {
			pressure = rack::math::clamp(pKnob * inputs[PRESSURE_INPUT].getVoltage() * 0.1f, -1.f, 1.f);
		} else if (inputs[TENSION_INPUT].getChannels() >= 2) {
			pressure = rack::math::clamp(pKnob * inputs[TENSION_INPUT].getVoltage(1) * 0.1f, -1.f, 1.f);
		} else {
			pressure = rack::math::clamp(pKnob, 0.f, 1.f);
		}

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

		glottis.setTargets(f0, tension, pressure, foldLen, voicingAmt);
	}

	float noise = rack::random::uniform() * 2.f - 1.f;
	float fb = inputs[FEEDBACK_INPUT].getVoltage() * 0.2f
		* params[FBGAIN_PARAM].getValue();

	float out = glottis.process(noise, fb,
		params[DRIFT_PARAM].getValue(), params[VIBRATE_PARAM].getValue());

	outputs[VOICED_OUTPUT].setVoltage(rack::math::clamp(out * 5.f, -5.f, 5.f));
	outputs[NOISEMOD_OUTPUT].setVoltage(
		rack::math::clamp(glottis.noiseModulator * 10.f, 0.f, 5.f));

	cpuMeter.frameEnd(measure, args.sampleRate);
}

} // namespace cyberline
