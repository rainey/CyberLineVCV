// CyberlineVoice — module process.

#include "CyberlineVoice.hpp"
#include "CyberlineQuality.hpp"
#include "CyberlineDspGuard.hpp"

namespace cyberline {

void CyberlineVoice::process(const ProcessArgs& args) {
	bool measure = cpuMeter.frameStart();
	// Subnormal protection (glottis one-poles + damped waveguide lines).
	DenormalGuard denormalGuard;

	if (args.sampleRate != lastSampleRate) {
		lastSampleRate = args.sampleRate;
		glottis.setSampleRate(args.sampleRate);
		tract.setGeometry(vtlCm, args.sampleRate);
	}

	// ---- block-rate parameters (every 16 samples) ----
	if ((scanIndex++ & 15) == 0) {
		// Quality: module override (hidden param) resolved against the
		// plugin-wide global (default LOW). LOW enables the block-rate
		// coefficient refresh (tract) and the incremental-exponential /
		// fast-gate-sine approximations (glottis).
		bool low = CyberlineQuality::effectiveLow(
			params[V_QUALITY_PARAM].getValue());
		tract.lowQuality = low;
		glottis.lowQuality = low;
		bool tractPoly6 = (inputs[V_TONGUEX_INPUT].getChannels() >= 6);
		bool tractPoly8 = (inputs[V_TONGUEX_INPUT].getChannels() >= 8);
		bool tensPoly2  = (inputs[V_TENSION_INPUT].getChannels() >= 2);

		float tensCv = inputs[V_TENSION_INPUT].isConnected()
			? inputs[V_TENSION_INPUT].getVoltage(0)
			: (tractPoly8 ? inputs[V_TONGUEX_INPUT].getVoltage(6) : 0.f);
		float tension = rack::math::clamp(
			params[V_TENSION_PARAM].getValue() + tensCv * 0.1f,
			0.f, 1.f);

		float pKnob = params[V_PRESSURE_PARAM].getValue();
		float pressure;
		if (inputs[V_PRESSURE_INPUT].isConnected()) {
			pressure = rack::math::clamp(pKnob * inputs[V_PRESSURE_INPUT].getVoltage() * 0.1f, -1.f, 1.f);
		} else if (tensPoly2) {
			pressure = rack::math::clamp(pKnob * inputs[V_TENSION_INPUT].getVoltage(1) * 0.1f, -1.f, 1.f);
		} else if (tractPoly8) {
			pressure = rack::math::clamp(pKnob * inputs[V_TONGUEX_INPUT].getVoltage(7) * 0.1f, -1.f, 1.f);
		} else {
			pressure = rack::math::clamp(pKnob, 0.f, 1.f);
		}
		storedTension = tension;
		storedPressure = pressure;

		// Voicing (0..1 fold vibration): Ch 3 of TENS/POLY, else Ch 9 of the
		// 9-ch TRACT POLY on TNGX/POLY; absent = fully voiced.
		float voicingAmt = 1.f;
		if (tensPoly2 && inputs[V_TENSION_INPUT].getChannels() >= 3)
			voicingAmt = rack::math::clamp(inputs[V_TENSION_INPUT].getVoltage(2) * 0.1f, 0.f, 1.f);
		else if (inputs[V_TONGUEX_INPUT].getChannels() >= 9)
			voicingAmt = rack::math::clamp(inputs[V_TONGUEX_INPUT].getVoltage(8) * 0.1f, 0.f, 1.f);

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
		glottis.setTargets(f0, storedTension, storedPressure, foldLen, voicingAmt);

		// VTL: 5 ms one-pole slew so CV jitter cannot retrigger geometry
		// rebuilds (setGeometry early-returns while the rounded section
		// count is unchanged).
		float vtlTarget = params[V_VTL_PARAM].getValue();
		float dtBlock = 16.f / args.sampleRate;
		float vtlAlpha = 1.f - std::exp(-dtBlock / 0.005f);
		vtlSlew += (vtlTarget - vtlSlew) * vtlAlpha;
		if (!std::isfinite(vtlSlew)) vtlSlew = vtlTarget;
		if (vtlSlew != vtlCm) {
			vtlCm = vtlSlew;
			tract.setGeometry(vtlCm, args.sampleRate);
		}

		float txCv = inputs[V_TONGUEX_INPUT].getVoltage(0);
		float tyCv = inputs[V_TONGUEY_INPUT].isConnected()
			? inputs[V_TONGUEY_INPUT].getVoltage()
			: (tractPoly6 ? inputs[V_TONGUEX_INPUT].getVoltage(1) : 0.f);
		float velCv = inputs[V_VELUM_INPUT].isConnected()
			? inputs[V_VELUM_INPUT].getVoltage()
			: (tractPoly6 ? inputs[V_TONGUEX_INPUT].getVoltage(2) : 0.f);

		float tx = rack::math::clamp(params[V_TONGUEX_PARAM].getValue() + txCv, 0.f, 10.f) / 10.f;
		float ty = rack::math::clamp(params[V_TONGUEY_PARAM].getValue() + tyCv, 0.f, 10.f) / 10.f;
		tract.setTongue(tract.bladeStart + tx * (tract.tipStart - tract.bladeStart),
			CyberlineTract::mapTongueYDiameter(ty));
		tract.setVelum(rack::math::clamp(params[V_VELUM_PARAM].getValue()
			+ velCv * 0.1f, 0.f, 1.f));

		// When a >= 6-channel Tract Poly cable is plugged into TNGX/POLY on Voice,
		// channels 4-6 unlock consonant constriction & frication (CPOS, CWID, FRIC).
		if (tractPoly6) {
			float cp = inputs[V_TONGUEX_INPUT].getVoltage(3);
			float cwNorm = rack::math::clamp(5.0f + inputs[V_TONGUEX_INPUT].getVoltage(4), 0.f, 10.f) * 0.1f;
			float cw = CyberlineTract::mapConstrWidth(cwNorm);
			float airDrive = rack::math::clamp(std::fabs(storedPressure) * 1.25f, 0.f, 1.f);
			float fr = rack::math::clamp(inputs[V_TONGUEX_INPUT].getVoltage(5) * 0.1f, 0.f, 1.f) * airDrive;
			float cpos = (cp <= 0.01f) ? -1.f
				: tract.bladeStart + rack::math::clamp(cp, 0.f, 10.f) / 10.f
					* (tract.n - tract.bladeStart);
			tract.setConstriction(cpos, cw, fr);
		} else {
			tract.setConstriction(-1.f, 3.f, 0.f);
		}

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
	float exc = voiced;
	float lip = tract.process(args.sampleTime, exc, 0.f, 0.f, noise);
	float nose = tract.lastNoseOutput;

	outputs[V_VOICED_OUTPUT].setVoltage(rack::math::clamp(voiced * 5.f, -5.f, 5.f));
	outputs[V_LIPS_OUTPUT].setVoltage(rack::math::clamp(lip * 6.5f, -5.f, 5.f));
	outputs[V_NOSE_OUTPUT].setVoltage(rack::math::clamp(nose * 6.5f, -5.f, 5.f));
	lights[V_PHASE_LIGHT].setSmoothBrightness(
		glottis.noiseModulator, args.sampleTime);

	cpuMeter.frameEnd(measure, args.sampleRate);
}

} // namespace cyberline
