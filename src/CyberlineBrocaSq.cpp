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

	// ---- 0. Register editor state machine ----
	// POS encoder (wraps) -> editPos. EDIT button toggles armed state.
	// Arming initializes the V/C encoders from the cell's stored phonemes;
	// moving POS while armed re-loads them. While armed the edited pair
	// is written from the encoders every block and locked from mutation.
	int newEditPos = (int) rack::math::clamp(std::round(params[EDIT_POS_PARAM].getValue()), 0.f, (float) (REG_LEN - 1));
	bool posChanged = newEditPos != editPos;
	bool wantArm = false;
	bool editBut = params[EDIT_BUT_PARAM].getValue() > 0.f;
	if (editBut && !editButWas)
		wantArm = true;
	editButWas = editBut;
	if (wantArm)
		editActive = !editActive;
	if (newEditPos != editPos) {
		editPos = newEditPos;
		// Cell change while armed: show the new cell on the encoders
		if (editActive) {
			params[EDIT_V_PARAM].setValue((float) vowelReg[editPos]);
			params[EDIT_C_PARAM].setValue((float) consReg[editPos]);
		}
	}
	if (editActive && wantArm) {
		// Just armed: load the V/C encoders with the cell's phonemes
		params[EDIT_V_PARAM].setValue((float) vowelReg[editPos]);
		params[EDIT_C_PARAM].setValue((float) consReg[editPos]);
	}
	if (editActive) {
		// Live-write the edited pair (the widgets are the source of truth)
		vowelReg[editPos] = (uint8_t) rack::math::clamp((int) std::lround(params[EDIT_V_PARAM].getValue()), 0, NUM_VOWELS - 1);
		consReg[editPos] = (uint8_t) rack::math::clamp((int) std::lround(params[EDIT_C_PARAM].getValue()), 0, NUM_CONSONANTS - 1);
	}
	// LK-V / LK-C toggle the persistent mutation lock at editPos
	bool lkv = params[LKV_BUT_PARAM].getValue() > 0.f;
	if (lkv && !lkvWas)
		vowelLockMask ^= (1u << editPos);
	lkvWas = lkv;
	bool lkc = params[LKC_BUT_PARAM].getValue() > 0.f;
	if (lkc && !lkcWas)
		consLockMask ^= (1u << editPos);
	lkcWas = lkc;

	// Utility buttons: copy-left / fill / copy-right. A press copies
	// immediately (instant feedback), and *while held* the copy repeats
	// on every sequence advance — clock steps in FREE/CLOCK, phasor
	// segment crossings in PHASOR — so a held button paints the register
	// in sync with the musical grid. FILL is rising-edge one-shot.
	// copyHoldDir is consumed by the clocking block below.
	auto doCopyStep = [&](int dir) {
		copyCell(dir);
		editPos = (editPos + dir + REG_LEN) % REG_LEN;
		params[EDIT_POS_PARAM].setValue((float) editPos);   // keep the CELL encoder on the moved selection
	};
	copyHoldDir = 0;
	bool copyl = params[COPYL_BUT_PARAM].getValue() > 0.f;
	if (copyl && !copylWas)
		doCopyStep(-1);
	copylWas = copyl;
	bool copyr = params[COPYR_BUT_PARAM].getValue() > 0.f;
	if (copyr && !copyrWas)
		doCopyStep(+1);
	copyrWas = copyr;
	if (copyl)
		copyHoldDir = -1;
	else if (copyr)
		copyHoldDir = +1;
	bool fillB = params[FILL_BUT_PARAM].getValue() > 0.f;
	if (fillB && !fillWas)
		fillCells();
	fillWas = fillB;

	// Editor indicator lights (lock state of the selected cell, armed flag)
	lights[LKV_LIGHT].setBrightness(((vowelLockMask >> editPos) & 1u) ? 1.f : 0.f);
	lights[LKC_LIGHT].setBrightness(((consLockMask >> editPos) & 1u) ? 1.f : 0.f);
	lights[EDIT_LIGHT].setBrightness(editActive ? 1.f : 0.f);

	// Mutation roll for one register; on a hit the current slot is rewritten
	// with a random phoneme (consonant rolls may land on NONE = rest).
	// Consonant rolls are reweighted by FAVOR_PARAM: 0 = only nasals/
	// glides/rests (continuants, smooth morphs), 1 = fricatives + stops
	// (noisy/percussive), 0.5 = uniform over all 21. Rejection sampling
	// keeps it a pure shape change of the roll distribution.
	// Per-lane mutation locks: a locked cell is never rewritten. While
	// the editor is armed, the edited cell pair is locked too.
	auto mutate = [&](bool vowelLane, int pos) {
		if (editActive && pos == editPos)
			return;
		if (vowelLane
			? ((vowelLockMask >> pos) & 1u)
			: ((consLockMask >> pos) & 1u))
			return;
		float p = vowelLane
			? rack::math::clamp(params[MUTV_PARAM].getValue() + inputs[PV_INPUT].getVoltage() * 0.1f, 0.f, 1.f)
			: rack::math::clamp(params[MUTC_PARAM].getValue() + inputs[PC_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
		if (p > 0.f && rack::random::uniform() < p) {
			if (vowelLane) {
				vowelReg[pos] = (uint8_t) (rack::random::u32() % NUM_VOWELS);
			} else {
				float favor = rack::math::clamp(params[FAVOR_PARAM].getValue(), 0.f, 1.f);
				float wSoft = 1.f - favor;   // NONE + nasals + glides
				float wStop = favor;         // fricatives + stops
				float wMax = std::max(wSoft, wStop);
				for (int tries = 0; tries < 64; tries++) {
					int idx = (int) (rack::random::u32() % NUM_CONSONANTS);
					bool isNoisy = CyberlineBroca::CONSONANTS[idx].manner
						== MANNER_FRICATIVE
						|| CyberlineBroca::CONSONANTS[idx].manner
							== MANNER_STOP;
					if (rack::random::uniform() < (isNoisy ? wStop : wSoft) / wMax) {
						consReg[pos] = (uint8_t) idx;
						break;
					}
				}
			}
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
			// CLOCK mode is strictly external: no clock patched = no steps
			// (no silent fallback to the internal RATE clock — switch to
			// FREE mode for that). The manual STEP button still works.
			bool clockHigh = inputs[CLOCK_INPUT].getVoltage() >= 1.f;
			bool clockRising = (clockHigh && !clockWasHigh);
			clockWasHigh = clockHigh;
			if (clockRising)
				stepEvent = true;
		}
		if (stepRising)
			stepEvent = true;

		if (stepEvent) {
			step++;
			vPos = ((step % vLen) + vLen) % vLen;
			cPos = ((step % cLen) + cLen) % cLen;
			mutate(true, vPos);
			mutate(false, cPos);
			// Held copy button paints one cell per sequence advance
			if (copyHoldDir != 0)
				doCopyStep(copyHoldDir);
			fireGesture();
		}
	} else {
		// PHASOR mode: each register's position is a pure function of its own
		// phase (stateless). Strictly external: an unpatched register phasor
		// holds that register still (no internal fallback — switch to FREE
		// for that); an unpatched C-PH follows the V-PH phase. Fast jumps
		// snap: only the final position change fires (the normal old->new
		// morph handles it).
		float vPhase, cPhase;
		bool vPatched = inputs[VPH_INPUT].isConnected();
		bool cPatched = inputs[CPH_INPUT].isConnected();
		if (vPatched)
			vPhase = sq_wrapPhase(inputs[VPH_INPUT].getVoltage() * 0.1f);
		else if (cPatched)
			vPhase = sq_wrapPhase(inputs[CPH_INPUT].getVoltage() * 0.1f);
		else
			vPhase = sq_wrapPhase((float) vPos / (float) vLen);  // both unpatched: hold
		if (cPatched)
			cPhase = sq_wrapPhase(inputs[CPH_INPUT].getVoltage() * 0.1f);
		else
			cPhase = vPhase;

		int vNew = (int) std::fmin(vPhase * vLen, vLen - 1);
		int cNew = (int) std::fmin(cPhase * cLen, cLen - 1);

		bool vCross = (vNew != vPos), cCross = (cNew != cPos);
		if (vCross) {
			vPos = vNew;
			mutate(true, vPos);
			anyGesture = true;
		}
		if (cCross) {
			cPos = cNew;
			mutate(false, cPos);
			anyGesture = true;
		}
		// Held copy button paints one cell per phasor segment crossing
		if (copyHoldDir != 0 && (vCross || cCross)) {
			doCopyStep(copyHoldDir);
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
				? (onsetPhase >= 0.55f && onsetPhase < 0.68f)
				: (cvMix >= 0.38f && cvMix <= 0.65f);

			if (inClosure) {
				tgtCwid = 0.0f;
				if (cons.tongueY < 0.5f) tgtTy = 0.0f;
				tgtFric = 0.0f;
				tgtVoic = cons.voiced ? 0.5f : 0.0f;
				tgtPress = cons.voiced ? 0.65f : 0.15f;
			} else if (inBurst) {
				tgtCwid = 4.8f;
				tgtFric = cons.fricative * 0.45f;
				tgtVoic = cons.voicing;
				tgtPress = cons.pressure;
				burstGate = 1.f;
			} else {
				tgtCwid = sq_lerpf(vowCwid, 4.8f, cvMix);
				tgtFric = 0.0f;
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
	telEditPos = editPos;
	telEditActive = editActive;
	telVLocks = vowelLockMask;
	telCLocks = consLockMask;

	cpuMeter.frameEnd(measure, args.sampleRate);
}

} // namespace cyberline
