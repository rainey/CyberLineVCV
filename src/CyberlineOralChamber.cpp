// CyberlineOralChamber — module process.

#include "CyberlineOralChamber.hpp"

namespace cyberline {

void CyberlineOralChamber::process(const ProcessArgs& args) {
	if (args.sampleRate != lastSampleRate) {
		lastSampleRate = args.sampleRate;
		tract.setGeometry(vtlCm, args.sampleRate);
		tract.setTongue(tract.tongueIndex, tract.tongueDiameter);
	}

	// ---- block-rate parameter read (every 16 samples) ----
	bool blockUpdate = (scanIndex % 16) == 0;
	if (blockUpdate) {
		float vtl = params[VTL_PARAM].getValue();
		if (vtl != vtlCm) {
			vtlCm = vtl;
			tract.setGeometry(vtlCm, args.sampleRate);
		}

		// Tongue X: 0..10 V -> blade..tip index. Y: knob bottom = closed
		// (stop consonants), ~6 = neutral tract, top = wide open.
		// setTongue maps tongue diameter dia to a tract minimum of
		// d_min = 0.3 + (dia-2)/1.5, so this knob value inverts that:
		// dia = 1.5 + ty*3.8  ->  d_min ~= 2.53*ty - 0.03 (clamped at 0.05),
		// i.e. closed at 0, neutral (1.5) at ~6, wide (2.5) at 10.
		float tx = rack::math::clamp(params[TONGUEX_PARAM].getValue()
			+ inputs[TONGUEX_INPUT].getVoltage(), 0.f, 10.f) / 10.f;
		float ty = rack::math::clamp(params[TONGUEY_PARAM].getValue()
			+ inputs[TONGUEY_INPUT].getVoltage(), 0.f, 10.f) / 10.f;
		float idx = tract.bladeStart + tx * (tract.tipStart - tract.bladeStart);
		float dia = 1.5f + ty * 3.8f;
		tract.setTongue(idx, dia);

		// Velum: 0..10 V -> 0..1
		float vel = rack::math::clamp(params[VELUM_PARAM].getValue()
			+ inputs[VELUM_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
		tract.setVelum(vel);

		// Constriction position/width/amount (position -1 = off when param+CV
		// both zero)
		float cp = params[CONSTRPOS_PARAM].getValue()
			+ inputs[CONSTRPOS_INPUT].getVoltage();
		float cw = params[CONSTRW_PARAM].getValue()
			+ inputs[CONSTRW_INPUT].getVoltage();
		float fr = rack::math::clamp(params[FRIC_PARAM].getValue()
			+ inputs[FRIC_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
		float cpos = (cp <= 0.01f) ? -1.f
			: tract.bladeStart + rack::math::clamp(cp, 0.f, 10.f) / 10.f
				* (tract.n - tract.bladeStart);
		tract.setConstriction(cpos, cw, fr);

		// Muscle speed: 0..1 -> 2..40 cm/s; wall damping param
		tract.muscleSpeed = 2.f + params[SPEED_PARAM].getValue() * 38.f;
		tract.wallDamping = params[WALL_PARAM].getValue();

		tract.applyConstriction();
	}

	// ---- per-sample ----
	tract.reshape(args.sampleTime);
	tract.calculateReflectionsPublic();

	float source = inputs[SOURCE_INPUT].getVoltage() * 0.2f;
	float inject = inputs[INJECT_INPUT].getVoltage() * 0.2f;
	float injPos = params[INJPOS_PARAM].getValue() * 0.1f * (tract.n - 2);
	if (inputs[INJPOS_INPUT].isConnected())
		injPos = rack::math::clamp(inputs[INJPOS_INPUT].getVoltage() * 0.1f,
			0.f, 1.f) * (tract.n - 2);
	float noise = rack::random::uniform() * 2.f - 1.f;

	// 2x oversample stepping
	float lip = tract.step(source, inject, injPos, noise, 0.5f);
	lip = tract.step(source, inject, injPos, noise, 1.0f);
	float nose = tract.lastNoseOutput;

	outputs[LIPS_OUTPUT].setVoltage(rack::math::clamp(lip * 5.f, -5.f, 5.f));
	outputs[NOSE_OUTPUT].setVoltage(rack::math::clamp(nose * 5.f, -5.f, 5.f));

	// SHAPE: cycle one section amplitude per sample
	if (scanIndex >= tract.n) scanIndex = 0;
	outputs[SHAPE_OUTPUT].setVoltage(
		rack::math::clamp(tract.amplitude[scanIndex] * 5.f, 0.f, 8.f));

	// PROBE: pressure at ~mid oral section (R+L sum at probe index)
	int probe = tract.n / 3;
	float p = (tract.R[probe] + tract.L[probe]) * 5.f;
	outputs[PROBE_OUTPUT].setVoltage(rack::math::clamp(p, -8.f, 8.f));

	scanIndex++;
}

} // namespace cyberline
