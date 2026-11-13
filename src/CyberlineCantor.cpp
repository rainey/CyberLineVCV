// CyberlineCantor — module process.

#include "CyberlineCantor.hpp"
#include "CyberlineQuality.hpp"
#include "CyberlineDspGuard.hpp"

namespace cyberline {

void CyberlineCantor::process(const ProcessArgs& args) {
	bool measure = cpuMeter.frameStart();
	DenormalGuard denormalGuard;

	glottis.lowQuality = CyberlineQuality::effectiveLow(
		params[QUALITY_PARAM].getValue());

	if (glottis.sampleRate != args.sampleRate)
		glottis.setSampleRate(args.sampleRate);

	if (glottis.smoothCounter + 1 >= CyberlineGlottis::SMOOTH_INTERVAL || firstFrame) {
		firstFrame = false;

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

		// Compute musical pitch F0
		float v = inputs[PITCH_INPUT].getVoltage();
		float oct = std::round(params[OCT_PARAM].getValue());
		float tuneSt = params[TUNE_PARAM].getValue();
		float pBendSt = params[PCOUPLE_PARAM].getValue() * 2.f * std::fabs(pressure);
		float totalOct = v + oct + (tuneSt + pBendSt) / 12.f;
		float f0 = getRefFreq() * std::pow(2.f, totalOct);

		// Compute effective vocal fold length from f0 via inverse mammalian allometric law
		// log10(F0) = 4.01 - 1.43 * log10(L_mm)
		float baseFoldMm = std::pow(10.f, (4.01f - std::log10(rack::math::clamp(f0, 20.f, 2000.f))) / 1.43f);
		float massMult = std::pow(4.f, params[MASS_PARAM].getValue());
		float effFoldMm = rack::math::clamp(baseFoldMm * massMult, 2.f, 45.f);

		telemetryF0 = f0;
		telemetryEffFoldMm = effFoldMm;

		glottis.setTargets(f0, tension, pressure, effFoldMm, voicingAmt);
	}

	float carrIn = 0.f;
	if (inputs[CARRIER_INPUT].isConnected()) {
		carrIn = rack::math::clamp(inputs[CARRIER_INPUT].getVoltage() * 0.2f, -1.5f, 1.5f);
		if (carrierHardSync) {
			if (lastCarrierSample <= 0.f && carrIn > 0.f) {
				glottis.timeInWaveform = glottis.waveformLength;
			}
		}
		lastCarrierSample = carrIn;
	} else {
		lastCarrierSample = 0.f;
	}

	float noise = rack::random::uniform() * 2.f - 1.f;
	float glottalOut = glottis.process(noise, 0.f,
		params[DRIFT_PARAM].getValue(), params[VIBRATE_PARAM].getValue());

	float out = glottalOut;
	if (inputs[CARRIER_INPUT].isConnected()) {
		float carrMix = params[CARRMIX_PARAM].getValue();
		if (carrMix > 0.f) {
			float env = glottis.intensity;
			float asp = (1.f - std::sqrt(glottis.curTenseness)) * glottis.noiseModulator * noise * (0.2f + 0.02f * env);
			float shapedCarrier = (carrIn * glottis.voicing + asp) * env * glottis.loudness();
			out = (1.f - carrMix) * glottalOut + carrMix * shapedCarrier;
		}
	}

	outputs[VOICED_OUTPUT].setVoltage(rack::math::clamp(out * 5.f, -5.f, 5.f));
	outputs[NOISEMOD_OUTPUT].setVoltage(
		rack::math::clamp(glottis.noiseModulator * 10.f, 0.f, 5.f));

	cpuMeter.frameEnd(measure, args.sampleRate);
}

} // namespace cyberline
