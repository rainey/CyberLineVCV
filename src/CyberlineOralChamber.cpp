// CyberlineOralChamber — module process.

#include "CyberlineOralChamber.hpp"
#include "CyberlineDspGuard.hpp"

namespace cyberline {

void CyberlineOralChamber::process(const ProcessArgs& args) {
	// Subnormal protection: the waveguide lines decay exponentially toward
	// zero; without FTZ/DAZ the x86 FPU traps to microcode assists during
	// ring-out (~60x CPU). Restores the previous MXCSR on scope exit.
	DenormalGuard denormalGuard;

	if (args.sampleRate != lastSampleRate) {
		lastSampleRate = args.sampleRate;
		tract.setGeometry(vtlCm, args.sampleRate);
		tract.setTongue(tract.tongueIndex, tract.tongueDiameter);
	}

	// ---- block-rate parameter read (every 16 samples) ----
	if ((blockCounter++ & 15) == 0) {
		// Quality: LOW decouples the coefficient refresh from the audio
		// rate (16-sample fade) at a ~90% division-load reduction during
		// articulation. Toggled from the context menu.
		tract.lowQuality = params[QUALITY_PARAM].getValue() > 0.5f;

		// VTL: 5 ms one-pole slew on the CV so sample jitter cannot
		// repeatedly trigger geometry rebuilds (setGeometry still
		// early-returns while the rounded section count is unchanged).
		float vtlTarget = rack::math::clamp(
			params[VTL_PARAM].getValue() + inputs[VTL_INPUT].getVoltage() * 5.4f,
			6.f, 60.f);
		float dtBlock = 16.f / args.sampleRate;
		float vtlAlpha = 1.f - std::exp(-dtBlock / 0.005f);
		vtlSlew += (vtlTarget - vtlSlew) * vtlAlpha;
		if (!std::isfinite(vtlSlew)) vtlSlew = vtlTarget;
		if (vtlSlew != vtlCm) {
			vtlCm = vtlSlew;
			tract.setGeometry(vtlCm, args.sampleRate);
		}

		// Tongue X: 0..10 V -> blade..tip index.
		// Tongue Y: quadratic response curve (u = 0.22*ty + 0.78*ty^2) to give
		// ~2x finer resolution in the narrow-constriction region (d_min = 0..0.8 cm)
		// where waveguide area ratios (A = d^2) produce the strongest formant shifts:
		//   knob 0   -> d_min = 0.00 cm (full occlusion / stop consonant)
		//   knob 2.5 -> d_min ~ 0.21 cm (close vowels /i/, /u/, approximants)
		//   knob 5.0 -> d_min ~ 0.69 cm (mid vowels right at 12 o'clock default)
		//   knob 7.7 -> d_min ~ 1.50 cm (neutral uniform tract)
		//   knob 10  -> d_min ~ 2.30 cm (wide open /a/)
		// Poly-bus normalling on TONGUEX_INPUT (TNGX/POLY):
		// When a >= 6-channel cable (e.g. from BROCA's TRACT POLY) is plugged into
		// TNGX/POLY, channels 1-6 normal to TNGX, TNGY, VEL, CPOS, CWID, FRIC unless
		// overridden by a mono cable in that individual jack.
		bool polyBus = (inputs[TONGUEX_INPUT].getChannels() >= 6);
		float txCv = inputs[TONGUEX_INPUT].getVoltage(0);
		float tyCv = inputs[TONGUEY_INPUT].isConnected()
			? inputs[TONGUEY_INPUT].getVoltage()
			: (polyBus ? inputs[TONGUEX_INPUT].getVoltage(1) : 0.f);
		float velCv = inputs[VELUM_INPUT].isConnected()
			? inputs[VELUM_INPUT].getVoltage()
			: (polyBus ? inputs[TONGUEX_INPUT].getVoltage(2) : 0.f);
		float cpCv = inputs[CONSTRPOS_INPUT].isConnected()
			? inputs[CONSTRPOS_INPUT].getVoltage()
			: (polyBus ? inputs[TONGUEX_INPUT].getVoltage(3) : 0.f);
		float cwCv = inputs[CONSTRW_INPUT].isConnected()
			? inputs[CONSTRW_INPUT].getVoltage()
			: (polyBus ? inputs[TONGUEX_INPUT].getVoltage(4) : 0.f);
		float frCv = inputs[FRIC_INPUT].isConnected()
			? inputs[FRIC_INPUT].getVoltage()
			: (polyBus ? inputs[TONGUEX_INPUT].getVoltage(5) : 0.f);

		float tx = rack::math::clamp(params[TONGUEX_PARAM].getValue() + txCv, 0.f, 10.f) / 10.f;
		float ty = rack::math::clamp(params[TONGUEY_PARAM].getValue() + tyCv, 0.f, 10.f) / 10.f;
		float idx = tract.bladeStart + tx * (tract.tipStart - tract.bladeStart);
		tract.setTongue(idx, CyberlineTract::mapTongueYDiameter(ty));

		// Velum: 0..10 V -> 0..1
		float vel = rack::math::clamp(params[VELUM_PARAM].getValue() + velCv * 0.1f, 0.f, 1.f);
		tract.setVelum(vel);

		// Constriction position/width/amount (position -1 = off when param+CV
		// both zero).
		// C.WIDTH uses CyberlineTract::mapConstrWidth's quadratic curve mapped
		// to d = 0..1.50 cm so the upper half of the knob isn't wasted above
		// the 1.5 cm neutral oral diameter, and the default 12 o'clock position
		// (5.0 -> d = 0.42 cm) sits right in the peak of the [0.30, 0.70] cm
		// fricative turbulence window.
		float cp = params[CONSTRPOS_PARAM].getValue() + cpCv;
		float cwNorm = rack::math::clamp(params[CONSTRW_PARAM].getValue() + cwCv, 0.f, 10.f) * 0.1f;
		float cw = CyberlineTract::mapConstrWidth(cwNorm);
		float fr = rack::math::clamp(params[FRIC_PARAM].getValue() + frCv * 0.1f, 0.f, 1.f);
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

	float source = inputs[SOURCE_INPUT].getVoltage() * 0.2f;
	float inject = inputs[INJECT_INPUT].getVoltage() * 0.2f;
	float injPos = rack::math::clamp(
		params[INJPOS_PARAM].getValue() + inputs[INJPOS_INPUT].getVoltage(),
		0.f, 10.f) * 0.1f * (tract.n - 2);
	float noise = rack::random::uniform() * 2.f - 1.f;

	// shape update + sample-rate-appropriate waveguide stepping (2x oversample
	// at <= 51.2 kHz, 1x above, decimated + interpolated at very high rates)
	float lip = tract.process(args.sampleTime, source, inject, injPos, noise);
	float nose = tract.lastNoseOutput;

	outputs[LIPS_OUTPUT].setVoltage(rack::math::clamp(lip * 6.5f, -5.f, 5.f));
	outputs[NOSE_OUTPUT].setVoltage(rack::math::clamp(nose * 6.5f, -5.f, 5.f));

	// SHAPE: cycle one section amplitude per sample
	if (scanIndex >= tract.n) scanIndex = 0;
	outputs[SHAPE_OUTPUT].setVoltage(
		rack::math::clamp(tract.amplitude[scanIndex] * 5.f, 0.f, 8.f));
	scanIndex++;

	// PROBE: pressure at ~mid oral section (R+L sum at probe index)
	int probe = tract.n / 3;
	float p = (tract.R[probe] + tract.L[probe]) * 5.f;
	outputs[PROBE_OUTPUT].setVoltage(rack::math::clamp(p, -8.f, 8.f));
}

} // namespace cyberline
