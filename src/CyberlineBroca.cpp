// CyberlineBroca — articulatory cortex & phoneme generator process.

#include "CyberlineBroca.hpp"
#include <cmath>

namespace cyberline {

const CyberlineVowelTarget CyberlineBroca::VOWELS[NUM_VOWELS] = {
	// code,   ipa,   tngX, tngY, lipW,  lipPos
	{"EE",  "/i/",   8.4f, 2.2f, 10.0f, 0.0f},  // 0: close front unrounded
	{"IH",  "/ɪ/",   7.4f, 3.5f, 10.0f, 0.0f},  // 1: near-close front
	{"EH",  "/e/",   6.8f, 5.0f, 10.0f, 0.0f},  // 2: mid front
	{"AE",  "/æ/",   6.2f, 7.4f, 10.0f, 0.0f},  // 3: near-open front
	{"AH",  "/ɑ/",   1.8f, 9.0f, 10.0f, 0.0f},  // 4: open back unrounded
	{"AW",  "/ɔ/",   1.6f, 6.6f,  6.8f, 9.4f},  // 5: open-mid back rounded
	{"OH",  "/o/",   2.0f, 4.6f,  5.8f, 9.5f},  // 6: close-mid back rounded
	{"OO",  "/u/",   2.4f, 2.4f,  5.2f, 9.6f},  // 7: close back rounded
	{"UH",  "/ʊ/",   3.2f, 3.8f,  6.6f, 9.4f},  // 8: near-close back rounded
	{"UX",  "/ʌ/",   4.2f, 6.8f, 10.0f, 0.0f},  // 9: open-mid central
	{"ER",  "/ɜ/",   5.2f, 4.6f,  6.2f, 6.0f},  // 10: rhotic mid-central (postalv bunched)
	{"SCH", "/ə/",   5.0f, 5.4f, 10.0f, 0.0f}   // 11: neutral schwa
};

const CyberlineConsonantTarget CyberlineBroca::CONSONANTS[NUM_CONSONANTS] = {
	// code, ipa,   manner,           voiced, tngX, tngY, vel,   cPos, cWid, fric, voic, press
	{"-",  "none", MANNER_NONE,      true,   5.0f, 5.0f, 0.01f, 0.0f, 5.0f, 0.00f, 1.00f, 0.80f}, // 0
	{"M",  "/m/",  MANNER_NASAL,     true,   4.8f, 5.2f, 0.96f, 9.6f, 0.0f, 0.00f, 1.00f, 0.82f}, // 1: bilabial nasal
	{"N",  "/n/",  MANNER_NASAL,     true,   7.4f, 3.6f, 0.96f, 7.6f, 0.0f, 0.00f, 1.00f, 0.82f}, // 2: alveolar nasal
	{"NG", "/ŋ/",  MANNER_NASAL,     true,   2.4f, 0.0f, 0.96f, 3.4f, 0.0f, 0.00f, 1.00f, 0.82f}, // 3: velar nasal
	{"W",  "/w/",  MANNER_GLIDE,     true,   2.2f, 2.3f, 0.01f, 9.6f, 5.3f, 0.05f, 1.00f, 0.80f}, // 4: labial-velar glide
	{"Y",  "/j/",  MANNER_GLIDE,     true,   8.6f, 2.0f, 0.01f, 4.8f, 6.0f, 0.05f, 1.00f, 0.80f}, // 5: palatal glide
	{"L",  "/l/",  MANNER_GLIDE,     true,   6.4f, 5.8f, 0.01f, 7.8f, 6.2f, 0.05f, 1.00f, 0.80f}, // 6: alveolar lateral
	{"R",  "/ɹ/",  MANNER_GLIDE,     true,   5.4f, 4.4f, 0.01f, 6.0f, 6.0f, 0.05f, 1.00f, 0.80f}, // 7: postalv approximant
	{"F",  "/f/",  MANNER_FRICATIVE, false,  5.0f, 5.2f, 0.01f, 9.2f, 4.8f, 0.80f, 0.00f, 0.68f}, // 8: voiceless labiodental
	{"V",  "/v/",  MANNER_FRICATIVE, true,   5.0f, 5.2f, 0.01f, 9.2f, 4.9f, 0.68f, 1.00f, 0.82f}, // 9: voiced labiodental
	{"TH", "/θ/",  MANNER_FRICATIVE, false,  6.8f, 4.6f, 0.01f, 8.4f, 4.9f, 0.74f, 0.00f, 0.68f}, // 10: voiceless dental
	{"S",  "/s/",  MANNER_FRICATIVE, false,  7.6f, 3.4f, 0.01f, 7.6f, 4.7f, 0.95f, 0.00f, 0.72f}, // 11: voiceless alveolar
	{"Z",  "/z/",  MANNER_FRICATIVE, true,   7.6f, 3.4f, 0.01f, 7.6f, 4.8f, 0.84f, 1.00f, 0.85f}, // 12: voiced alveolar
	{"SH", "/ʃ/",  MANNER_FRICATIVE, false,  6.6f, 3.6f, 0.01f, 6.1f, 4.7f, 0.95f, 0.00f, 0.72f}, // 13: voiceless postalveolar
	{"H",  "/h/",  MANNER_FRICATIVE, false,  4.8f, 5.6f, 0.01f, 1.4f, 4.9f, 0.76f, 0.00f, 0.75f}, // 14: glottal aspirate
	{"P",  "/p/",  MANNER_STOP,      false,  4.8f, 5.2f, 0.01f, 9.6f, 0.0f, 0.78f, 0.00f, 0.68f}, // 15: voiceless bilabial stop
	{"B",  "/b/",  MANNER_STOP,      true,   4.8f, 5.2f, 0.01f, 9.6f, 0.0f, 0.35f, 1.00f, 0.82f}, // 16: voiced bilabial stop
	{"T",  "/t/",  MANNER_STOP,      false,  7.5f, 3.0f, 0.01f, 7.6f, 0.0f, 0.88f, 0.00f, 0.70f}, // 17: voiceless alveolar stop
	{"D",  "/d/",  MANNER_STOP,      true,   7.5f, 3.0f, 0.01f, 7.6f, 0.0f, 0.40f, 1.00f, 0.82f}, // 18: voiced alveolar stop
	{"K",  "/k/",  MANNER_STOP,      false,  2.6f, 0.0f, 0.01f, 3.5f, 0.0f, 0.82f, 0.00f, 0.70f}, // 19: voiceless velar stop
	{"G",  "/g/",  MANNER_STOP,      true,   2.6f, 0.0f, 0.01f, 3.5f, 0.0f, 0.40f, 1.00f, 0.82f}  // 20: voiced velar stop
};

static inline float broca_lerpf(float a, float b, float t) {
	return a + (b - a) * t;
}

void CyberlineBroca::process(const ProcessArgs& args) {
	bool measure = cpuMeter.frameStart();
	publishPhonemeMode(this, ptMode);
	float dt = args.sampleTime;

	// ---- 0. Detect SYNC / RESET and TRIG Edges ----
	bool syncHigh = inputs[SYNC_INPUT].getVoltage() >= 1.f;
	bool syncRising = (syncHigh && !syncWasHigh);
	syncWasHigh = syncHigh;

	bool trigHigh = inputs[TRIG_INPUT].getVoltage() >= 1.f;
	bool trigRising = (trigHigh && !trigWasHigh);
	trigWasHigh = trigHigh;

	if (syncRising) {
		scanStep = 0;
		onsetPhase = 0.f;
	} else if (trigRising && inputs[SYNC_INPUT].isConnected() && !inputs[VOWELA_INPUT].isConnected()) {
		// When SYNC is patched and VOWEL CV is unpatched, each TRIG steps cyclically
		// between Vowel A and Vowel B (and through the vowel table), reset to 0 on SYNC
		scanStep = (scanStep + 1) % NUM_VOWELS;
	}

	// ---- 1. Resolve Vowel A, Vowel B, and Vowel A-B Morph ----
	float vACv = inputs[VOWELA_INPUT].isConnected()
		? inputs[VOWELA_INPUT].getVoltage() * ((float)(NUM_VOWELS - 1) / 10.f)
		: (inputs[SYNC_INPUT].isConnected() ? (float) scanStep : 0.f);
	int idxA = ((int) std::round(params[VOWELA_PARAM].getValue() + vACv) % NUM_VOWELS + NUM_VOWELS) % NUM_VOWELS;
	float vBCv = inputs[VOWELB_INPUT].isConnected()
		? inputs[VOWELB_INPUT].getVoltage() * ((float)(NUM_VOWELS - 1) / 10.f)
		: 0.f;
	int idxB = rack::math::clamp(
		(int) std::round(params[VOWELB_PARAM].getValue() + vBCv),
		0, NUM_VOWELS - 1);
	float vMorph = rack::math::clamp(
		params[VMORPH_PARAM].getValue() + inputs[VMORPH_INPUT].getVoltage() * 0.1f,
		0.f, 1.f);

	const CyberlineVowelTarget& vA = VOWELS[idxA];
	const CyberlineVowelTarget& vB = VOWELS[idxB];

	float vowTx = broca_lerpf(vA.tongueX, vB.tongueX, vMorph);
	float vowTy = broca_lerpf(vA.tongueY, vB.tongueY, vMorph);
	float vowCpos = broca_lerpf(vA.lipPos, vB.lipPos, vMorph);
	float vowCwid = broca_lerpf(vA.lipWidth, vB.lipWidth, vMorph);
	float vowVel = 0.01f;
	float vowFric = 0.0f;
	float vowVoic = 1.0f;     // vowels are fully voiced; tension stays modal so F0 never dips
	float vowPress = 0.80f;

	// ---- 2. Resolve Consonant Selection & Syllable Trigger Envelope ----
	float cCv = inputs[CONSONANT_INPUT].isConnected()
		? inputs[CONSONANT_INPUT].getVoltage() * ((float)(NUM_CONSONANTS - 1) / 10.f)
		: 0.f;
	int idxC = rack::math::clamp(
		(int) std::round(params[CONSONANT_PARAM].getValue() + cCv),
		0, NUM_CONSONANTS - 1);
	const CyberlineConsonantTarget& cons = CONSONANTS[idxC];

	float onsetDur = rack::math::clamp(params[ONSET_PARAM].getValue(), 0.02f, 0.35f);

	// Rising edge on TRIG or SYNC fires a consonant-to-vowel syllable gesture
	if (trigRising || syncRising) {
		onsetActive = (cons.manner != MANNER_NONE);
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
			// Hold consonant posture during first 60% of onset, then smooth cosine release to vowel
			if (onsetPhase < 0.60f) {
				trigEnv = 1.f;
			} else {
				float u = (onsetPhase - 0.60f) / 0.40f;
				trigEnv = 0.5f * (1.f + std::cos(u * (float) M_PI));
			}
		}
	}

	// Combine manual/CV C/V morph with triggered syllable onset envelope
	float manualCV = rack::math::clamp(
		params[CVMORPH_PARAM].getValue() + inputs[CVMORPH_INPUT].getVoltage() * 0.1f,
		0.f, 1.f);
	float cvMix = rack::math::clamp( std::fmax(manualCV, trigEnv), 0.f, 1.f );
	if (cons.manner == MANNER_NONE) {
		cvMix = 0.f;
	}

	// ---- 3. Compute Consonant -> Vowel Articulatory Trajectory by Manner ----
	float tgtTx = vowTx;
	float tgtTy = vowTy;
	float tgtVel = vowVel;
	float tgtCpos = vowCpos;
	float tgtCwid = vowCwid;
	float tgtFric = vowFric;
	float tgtVoic = vowVoic;
	float tgtPress = vowPress;

	if (cons.manner != MANNER_NONE && cvMix > 0.001f) {
		// Coarticulation: dorsal tongue moves partway toward consonant place while
		// anticipating the target vowel
		float dorsalBlend = (cons.manner == MANNER_STOP && cons.tongueY < 0.5f)
			? cvMix                  // velar stops K/G require full dorsal contact
			: (0.65f * cvMix);       // labial/alveolar consonants coarticulate with vowel tongue body
		tgtTx = broca_lerpf(vowTx, cons.tongueX, dorsalBlend);
		tgtTy = broca_lerpf(vowTy, cons.tongueY, dorsalBlend);
		tgtVel = broca_lerpf(vowVel, cons.velum, cvMix);
		tgtCpos = (cvMix > 0.08f) ? cons.constrPos : vowCpos;

		if (cons.manner == MANNER_STOP) {
			// Plosive articulation:
			// - When triggered: [0..0.55] = full occlusion (CWID = 0),
			//   [0.55..0.68] = crisp release burst (~10-15 ms burst window + calibrated FRIC burst),
			//   [0.68..1.0] = release into vowel.
			// - When swept manually via C/V MORPH: cvMix > 0.65 = occlusion,
			//   0.38..0.65 = burst release window, < 0.38 = vowel.
			bool inClosure = onsetActive
				? (onsetPhase < 0.55f)
				: (cvMix > 0.65f);
			bool inBurst = onsetActive
				? (onsetPhase >= 0.55f && onsetPhase < 0.68f)
				: (cvMix >= 0.38f && cvMix <= 0.65f);

			if (inClosure) {
				tgtCwid = 0.0f;
				if (cons.tongueY < 0.5f) tgtTy = 0.0f; // velar stop occlusion
				tgtFric = 0.0f;
				// Voiceless stops (P, T, K) silence the folds during closure;
				// voiced stops (B, D, G) keep a partial voice-bar
				tgtVoic = cons.voiced ? 0.5f : 0.0f;
				tgtPress = cons.voiced ? 0.65f : 0.15f;
			} else if (inBurst) {
				tgtCwid = 4.8f; // passes squarely through [0.3, 0.7] cm turbulence window
				tgtFric = cons.fricative * 0.45f;
				tgtVoic = cons.voicing;
				tgtPress = cons.pressure;
				burstGate = 1.f;
			} else {
				tgtCwid = broca_lerpf(vowCwid, 4.8f, cvMix);
				tgtFric = 0.0f;
				tgtVoic = broca_lerpf(vowVoic, cons.voicing, cvMix);
				tgtPress = broca_lerpf(vowPress, cons.pressure, cvMix);
			}
		} else {
			// Nasals, Glides, and Fricatives morph smoothly between vowel and consonant targets
			tgtCwid = broca_lerpf(vowCwid, cons.constrWidth, cvMix);
			tgtFric = broca_lerpf(vowFric, cons.fricative, cvMix);
			tgtVoic = broca_lerpf(vowVoic, cons.voicing, cvMix);
			tgtPress = broca_lerpf(vowPress, cons.pressure, cvMix);
			if (cons.manner == MANNER_FRICATIVE && cvMix > 0.35f) {
				burstGate = cvMix;
			}
		}
	}

	// ---- 4. Articulator Slew Smoothing (prevents step clicks on CV jumps) ----
	// Plosive release uses fast slew (~4 ms) so CyberlineTract's transient detector
	// sees a clean occlusion-to-open transition; vowel glides use smooth (~14 ms) slew.
	float tau = (cons.manner == MANNER_STOP) ? 0.0045f : 0.014f;
	float alpha = rack::math::clamp(dt / (tau + dt), 0.001f, 1.f);

	curTongueX     += (tgtTx - curTongueX) * alpha;
	curTongueY     += (tgtTy - curTongueY) * alpha;
	curVelum       += (tgtVel - curVelum) * alpha;
	curConstrPos   += (tgtCpos - curConstrPos) * alpha;
	curConstrWidth += (tgtCwid - curConstrWidth) * alpha;
	curFric        += (tgtFric - curFric) * alpha;
	curVoicing     += (tgtVoic - curVoicing) * alpha;
	curPressure    += (tgtPress - curPressure) * alpha;

	// ---- 5. Output Voltages (Referenced to Sink Default Knob Positions) ----
	// Sink default knobs:
	//   Oral Chamber / Voice: TNG X = 5.0, TNG Y = 5.0, VELUM = 0.01,
	//                         C.POS = 0.0, C.WIDTH = 5.0, FRIC = 0.0
	//   Larynx / Voice:       TENSION = 0.6, PRESSURE = 0.8 (multiplicative 0-10V)
	// Tension is held at the receiver's own knob (0 V offset): voicing is carried by
	// its own channel so voiceless sounds never drag F0 down through sqrt(tension).
	float vTngX  = rack::math::clamp(curTongueX - 5.0f, -5.f, 5.f);
	float vTngY  = rack::math::clamp(curTongueY - 5.0f, -5.f, 5.f);
	float vVel   = rack::math::clamp((curVelum - 0.01f) * 10.f, 0.f, 10.f);
	float vCpos  = rack::math::clamp(curConstrPos, 0.f, 10.f);
	float vCwid  = rack::math::clamp(curConstrWidth - 5.0f, -5.f, 5.f);
	float vFric  = rack::math::clamp(curFric * 10.f, 0.f, 10.f);
	float vTens  = 0.f;
	float vVoic  = rack::math::clamp(curVoicing * 10.f, 0.f, 10.f);
	float vPress = rack::math::clamp((curPressure / 0.8f) * 10.f, 0.f, 10.f);

	// 9-channel TRACT POLY bus (drives Oral Chamber Ch 1-6, and Voice Ch 1-9).
	// In Pink Trombone mode the bus re-encodes to a 10-channel PT-oriented
	// map (see PtBusEncode in CyberlineModels.hpp) for the PT Breakout.
	float busIn[9] = {vTngX, vTngY, vVel, vCpos, vCwid, vFric, vTens, vPress, vVoic};
	if (ptMode) {
		float busOut[10];
		PtBusEncode::encode(busIn, busOut);
		outputs[TRACT_POLY_OUTPUT].setChannels(10);
		for (int c = 0; c < 10; c++)
			outputs[TRACT_POLY_OUTPUT].setVoltage(busOut[c], c);
	} else {
		outputs[TRACT_POLY_OUTPUT].setChannels(9);
		for (int c = 0; c < 9; c++)
			outputs[TRACT_POLY_OUTPUT].setVoltage(busIn[c], c);
	}

	// 3-channel LARYNX POLY bus (Ch 1: TENSION offset, Ch 2: PRESSURE envelope, Ch 3: VOICING)
	outputs[LARYNX_POLY_OUTPUT].setChannels(3);
	outputs[LARYNX_POLY_OUTPUT].setVoltage(vTens,  0);
	outputs[LARYNX_POLY_OUTPUT].setVoltage(vPress, 1);
	outputs[LARYNX_POLY_OUTPUT].setVoltage(vVoic,  2);

	// Mono envelope & burst outputs
	outputs[CVENV_OUTPUT].setVoltage(rack::math::clamp(cvMix * 10.f, 0.f, 10.f));
	outputs[BURST_OUTPUT].setVoltage(rack::math::clamp(burstGate * 10.f, 0.f, 10.f));

	// Save telemetry for HUD
	activeVowelA = idxA;
	activeVowelB = idxB;
	activeConsonant = idxC;
	activeVMorph = vMorph;
	activeCVMix = cvMix;
	activeBurst = burstGate;

	cpuMeter.frameEnd(measure, args.sampleRate);
}

} // namespace cyberline
