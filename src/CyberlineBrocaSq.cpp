// CyberlineBrocaSq — dual phoneme Turing machine process.
// Clocked / phase-driven register scan with per-step mutation, feeding the
// same articulatory onset engine and poly bus conventions as CyberlineBroca.

#include "CyberlineBrocaSq.hpp"
#include <cmath>

namespace cyberline {

static inline float sq_lerpf(float a, float b, float t) {
	return a + (b - a) * t;
}

// Wrap a phase value into [0,1); tolerates unipolar ramps slightly out of
// range and reversed phasors (negative saws) by pure fmod arithmetic.
static inline float sq_wrapPhase(float ph) {
	return ph - std::floor(ph);
}

void CyberlineBrocaSq::process(const ProcessArgs& args) {
	bool measure = cpuMeter.frameStart();
	float dt = args.sampleTime;

	int vLen, cLen;
	getLengths(&vLen, &cLen);
	mode = (int) rack::math::clamp(std::round(params[MODE_PARAM].getValue()), 0.f, 2.f);
	telVLen = vLen;
	telCLen = cLen;
	telMode = mode;

	// ---- 0. Reset trigger & manual step button ----
	bool resetHigh = inputs[RESET_INPUT].getVoltage() >= 1.f;
	bool resetRising = (resetHigh && !resetWasHigh);
	resetWasHigh = resetHigh;

	bool stepPressed = params[STEP_PARAM].getValue() > 0.5f;
	bool stepRising = (stepPressed && !stepWasPressed);
	stepWasPressed = stepPressed;

	bool anyGesture = false;
	auto fireGesture = [&]() { onsetActive = true; onsetPhase = 0.f; anyGesture = true; };

	// Mutation roll for one register; on a hit the current slot is rewritten
	// with a random phoneme (consonant rolls may land on NONE = rest).
	auto mutate = [&](bool vowelLane, int pos) {
		float p = vowelLane
			? rack::math::clamp(params[MUTV_PARAM].getValue() + inputs[PV_INPUT].getVoltage() * 0.1f, 0.f, 1.f)
			: rack::math::clamp(params[MUTC_PARAM].getValue() + inputs[PC_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
		if (p > 0.f && rack::random::uniform() < p) {
			if (vowelLane)
				vowelReg[pos] = (uint8_t) (rack::random::u32() % NUM_VOWELS);
			else
				consReg[pos] = (uint8_t) (rack::random::u32() % NUM_CONSONANTS);
		}
	};

	// ---- 1. Clocking: resolve both register positions & step events ----
	if (resetRising) {
		step = 0;
		freePhase = 0.f;
		vPos = 0;
		cPos = 0;
	}

	if (mode == MODE_FREE || mode == MODE_CLOCK) {
		// Shared step counter; polymeter comes from the differing lengths.
		bool stepEvent = false;
		if (mode == MODE_FREE) {
			float rate = rack::math::clamp(params[RATE_PARAM].getValue() * 19.5f + 0.5f, 0.5f, 20.f);
			freePhase += dt * rate;
			if (freePhase >= 1.f) {
				freePhase -= std::floor(freePhase);
				stepEvent = true;
			}
		} else {
			// CLOCK mode: external trigger advances the shared step; with no
			// clock patched, fall back to the internal RATE clock.
			bool clockHigh = inputs[CLOCK_INPUT].getVoltage() >= 1.f;
			bool clockRising = (clockHigh && !clockWasHigh);
			clockWasHigh = clockHigh;
			if (clockRising)
				stepEvent = true;
			else if (!inputs[CLOCK_INPUT].isConnected()) {
				float rate = rack::math::clamp(params[RATE_PARAM].getValue() * 19.5f + 0.5f, 0.5f, 20.f);
				freePhase += dt * rate;
				if (freePhase >= 1.f) {
					freePhase -= std::floor(freePhase);
					stepEvent = true;
				}
			}
		}
		if (stepRising)
			stepEvent = true;

		if (stepEvent) {
			step++;
			vPos = ((step % vLen) + vLen) % vLen;
			cPos = ((step % cLen) + cLen) % cLen;
			mutate(true, vPos);
			mutate(false, cPos);
			fireGesture();
		}
	} else {
		// PHASOR mode: each register's position is a pure function of its own
		// phase (stateless). Unpatched phasor follows the other's; both
		// unpatched -> internal RATE phasor. Fast jumps snap: only the final
		// position change fires (the normal old->new morph handles it).
		float vPhase, cPhase;
		bool vPatched = inputs[VPH_INPUT].isConnected();
		bool cPatched = inputs[CPH_INPUT].isConnected();
		if (vPatched)
			vPhase = sq_wrapPhase(inputs[VPH_INPUT].getVoltage() * 0.1f);
		else if (cPatched)
			vPhase = sq_wrapPhase(inputs[CPH_INPUT].getVoltage() * 0.1f);
		else {
			float rate = rack::math::clamp(params[RATE_PARAM].getValue() * 19.5f + 0.5f, 0.5f, 20.f);
			freePhase += dt * rate;
			vPhase = sq_wrapPhase(freePhase);
		}
		if (cPatched)
			cPhase = sq_wrapPhase(inputs[CPH_INPUT].getVoltage() * 0.1f);
		else
			cPhase = vPhase;

		int vNew = (int) std::fmin(vPhase * vLen, vLen - 1);
		int cNew = (int) std::fmin(cPhase * cLen, cLen - 1);

		if (vNew != vPos) {
			vPos = vNew;
			mutate(true, vPos);
			anyGesture = true;
		}
		if (cNew != cPos) {
			cPos = cNew;
			mutate(false, cPos);
			anyGesture = true;
		}
		if (stepRising) {
			mutate(true, vPos);
			mutate(false, cPos);
			anyGesture = true;
		}
		if (anyGesture)
			fireGesture();
	}

	// ---- 2. Resolve the current vowel & consonant targets (Broca math) ----
	const auto& VOWELS = CyberlineBroca::VOWELS;
	const auto& CONSONANTS = CyberlineBroca::CONSONANTS;
	const CyberlineVowelTarget& v = VOWELS[vowelReg[vPos]];
	const CyberlineConsonantTarget& cons = CONSONANTS[consReg[cPos]];

	float onsetDur = rack::math::clamp(params[ONSET_PARAM].getValue(), 0.02f, 0.35f);

	if (anyGesture) {
		// The gate/env gesture fires on every step (a rest step is still a
		// voiced vowel); the consonant mix is what the NONE manner zeroes.
		onsetActive = true;
		onsetPhase = 0.f;
	}

	float trigEnv = 0.f;
	float burstGate = 0.f;
	if (onsetActive) {
		onsetPhase += dt / onsetDur;
		if (onsetPhase >= 1.f) {
			onsetPhase = 1.f;
			onsetActive = false;
		} else {
			if (onsetPhase < 0.60f)
				trigEnv = 1.f;
			else {
				float u = (onsetPhase - 0.60f) / 0.40f;
				trigEnv = 0.5f * (1.f + std::cos(u * (float) M_PI));
			}
		}
	}

	// Broca² has no manual C/V morph knob; the onset envelope owns the mix.
	float cvMix = trigEnv;
	if (cons.manner == MANNER_NONE)
		cvMix = 0.f;

	// ---- 3. Consonant -> vowel articulatory trajectory (same as Broca) ----
	float vowTx = v.tongueX, vowTy = v.tongueY;
	float vowCpos = v.lipPos, vowCwid = v.lipWidth;
	float vowVel = 0.01f, vowFric = 0.0f, vowVoic = 1.0f, vowPress = 0.80f;

	float tgtTx = vowTx, tgtTy = vowTy, tgtVel = vowVel;
	float tgtCpos = vowCpos, tgtCwid = vowCwid;
	float tgtFric = vowFric, tgtVoic = vowVoic, tgtPress = vowPress;

	if (cons.manner != MANNER_NONE && cvMix > 0.001f) {
		float dorsalBlend = (cons.manner == MANNER_STOP && cons.tongueY < 0.5f)
			? cvMix
			: (0.65f * cvMix);
		tgtTx = sq_lerpf(vowTx, cons.tongueX, dorsalBlend);
		tgtTy = sq_lerpf(vowTy, cons.tongueY, dorsalBlend);
		tgtVel = sq_lerpf(vowVel, cons.velum, cvMix);
		tgtCpos = (cvMix > 0.08f) ? cons.constrPos : vowCpos;

		if (cons.manner == MANNER_STOP) {
			bool inClosure = onsetActive
				? (onsetPhase < 0.55f)
				: (cvMix > 0.65f);
			bool inBurst = onsetActive
				? (onsetPhase >= 0.55f && onsetPhase < 0.88f)
				: (cvMix >= 0.22f && cvMix <= 0.65f);

			if (inClosure) {
				tgtCwid = 0.0f;
				if (cons.tongueY < 0.5f) tgtTy = 0.0f;
				tgtFric = 0.0f;
				tgtVoic = cons.voiced ? 0.5f : 0.0f;
				tgtPress = cons.voiced ? 0.65f : 0.15f;
			} else if (inBurst) {
				tgtCwid = 4.8f;
				tgtFric = cons.fricative;
				tgtVoic = cons.voicing;
				tgtPress = cons.pressure;
				burstGate = 1.f;
			} else {
				tgtCwid = sq_lerpf(vowCwid, 4.8f, cvMix);
				tgtFric = cons.fricative * cvMix;
				tgtVoic = sq_lerpf(vowVoic, cons.voicing, cvMix);
				tgtPress = sq_lerpf(vowPress, cons.pressure, cvMix);
			}
		} else {
			tgtCwid = sq_lerpf(vowCwid, cons.constrWidth, cvMix);
			tgtFric = sq_lerpf(vowFric, cons.fricative, cvMix);
			tgtVoic = sq_lerpf(vowVoic, cons.voicing, cvMix);
			tgtPress = sq_lerpf(vowPress, cons.pressure, cvMix);
			if (cons.manner == MANNER_FRICATIVE && cvMix > 0.35f)
				burstGate = cvMix;
		}
	}

	// ---- 4. Morph (articulator slew between phonemes) ----
	// MORPH 0 = Broca's crisp defaults (~4.5 ms stops / ~14 ms vowels);
	// higher values blend adjacent phonemes into half-formed speech.
	float morph = rack::math::clamp(
		params[MORPH_PARAM].getValue() + inputs[MORPH_INPUT].getVoltage() * 0.1f,
		0.f, 1.f);
	float tauBase = 0.0045f + morph * 0.18f;
	float tau = (cons.manner == MANNER_STOP)
		? rack::math::clamp(tauBase * 0.3f, 0.0045f, 0.06f)
		: tauBase;
	float alpha = rack::math::clamp(dt / (tau + dt), 0.0005f, 1.f);

	curTongueX     += (tgtTx - curTongueX) * alpha;
	curTongueY     += (tgtTy - curTongueY) * alpha;
	curVelum       += (tgtVel - curVelum) * alpha;
	curConstrPos   += (tgtCpos - curConstrPos) * alpha;
	curConstrWidth += (tgtCwid - curConstrWidth) * alpha;
	curFric        += (tgtFric - curFric) * alpha;
	curVoicing     += (tgtVoic - curVoicing) * alpha;
	curPressure    += (tgtPress - curPressure) * alpha;

	// ---- 5. Output voltages (identical conventions to Broca) ----
	float vTngX  = rack::math::clamp(curTongueX - 5.0f, -5.f, 5.f);
	float vTngY  = rack::math::clamp(curTongueY - 5.0f, -5.f, 5.f);
	float vVel   = rack::math::clamp((curVelum - 0.01f) * 10.f, 0.f, 10.f);
	float vCpos  = rack::math::clamp(curConstrPos, 0.f, 10.f);
	float vCwid  = rack::math::clamp(curConstrWidth - 5.0f, -5.f, 5.f);
	float vFric  = rack::math::clamp(curFric * 10.f, 0.f, 10.f);
	float vTens  = 0.f;
	float vVoic  = rack::math::clamp(curVoicing * 10.f, 0.f, 10.f);
	float vPress = rack::math::clamp((curPressure / 0.8f) * 10.f, 0.f, 10.f);

	outputs[TRACT_POLY_OUTPUT].setChannels(9);
	outputs[TRACT_POLY_OUTPUT].setVoltage(vTngX,  0);
	outputs[TRACT_POLY_OUTPUT].setVoltage(vTngY,  1);
	outputs[TRACT_POLY_OUTPUT].setVoltage(vVel,   2);
	outputs[TRACT_POLY_OUTPUT].setVoltage(vCpos,  3);
	outputs[TRACT_POLY_OUTPUT].setVoltage(vCwid,  4);
	outputs[TRACT_POLY_OUTPUT].setVoltage(vFric,  5);
	outputs[TRACT_POLY_OUTPUT].setVoltage(vTens,  6);
	outputs[TRACT_POLY_OUTPUT].setVoltage(vPress, 7);
	outputs[TRACT_POLY_OUTPUT].setVoltage(vVoic,  8);

	outputs[LARYNX_POLY_OUTPUT].setChannels(3);
	outputs[LARYNX_POLY_OUTPUT].setVoltage(vTens,  0);
	outputs[LARYNX_POLY_OUTPUT].setVoltage(vPress, 1);
	outputs[LARYNX_POLY_OUTPUT].setVoltage(vVoic,  2);

	outputs[CVENV_OUTPUT].setVoltage(rack::math::clamp(cvMix * 10.f, 0.f, 10.f));
	outputs[BURST_OUTPUT].setVoltage(rack::math::clamp(burstGate * 10.f, 0.f, 10.f));

	// Syllable gesture gate: high while the onset gesture runs, with a short
	// release tail so a stepped scan reads as a train of gate pulses.
	float gateTarget = onsetActive ? 1.f : 0.f;
	float gateAlpha = rack::math::clamp(dt / (0.004f + dt), 0.f, 1.f);
	gateLevel += (gateTarget - gateLevel) * gateAlpha;
	outputs[GATE_OUTPUT].setVoltage(rack::math::clamp(gateLevel * 10.f, 0.f, 10.f));

	// Index CVs: full-scale over each table
	outputs[VOWCV_OUTPUT].setVoltage(rack::math::clamp((float) vowelReg[vPos] * (10.f / (NUM_VOWELS - 1)), 0.f, 10.f));
	outputs[CONSCV_OUTPUT].setVoltage(rack::math::clamp((float) consReg[cPos] * (10.f / (NUM_CONSONANTS - 1)), 0.f, 10.f));

	// Telemetry for the 4x4 grid HUD
	telVPos = vPos;
	telCPos = cPos;
	activeVowel = vowelReg[vPos];
	activeConsonant = consReg[cPos];
	activeCVMix = cvMix;
	activeBurst = burstGate;

	cpuMeter.frameEnd(measure, args.sampleRate);
}

} // namespace cyberline
