// CyberlineBroca — "Broca" (Broca-Brika) articulatory cortex & phoneme generator.
// Generates anatomical target CVs (Tongue X/Y, Velum, Constriction Pos/Width,
// Fricative, and Laryngeal Tension/Pressure) via vowel A/B morphing and
// consonant onset/manner articulation over polyphonic buses (TRACT POLY and
// LARYNX POLY).

#pragma once
#include <rack.hpp>

namespace cyberline {

struct CyberlineVowelTarget {
	const char* code;   // short HUD label, e.g. "EE"
	const char* ipa;    // IPA symbol, e.g. "/i/"
	float tongueX;      // 0..10 (0 = back/pharyngeal, 10 = front/alveolar)
	float tongueY;      // 0..10 (quadratic curve: 2.1 = close, 5.0 = mid, 9.2 = open)
	float lipWidth;     // 0..10 (10 = unrounded/open, ~6.0 = rounded lips via C.POS/CWID)
	float lipPos;       // 0 if unrounded, ~9.5 if strongly labialized
};

enum CyberlineConsonantManner {
	MANNER_NONE = 0,
	MANNER_NASAL,
	MANNER_GLIDE,
	MANNER_FRICATIVE,
	MANNER_STOP
};

struct CyberlineConsonantTarget {
	const char* code;   // short HUD label, e.g. "SH"
	const char* ipa;    // IPA / description
	CyberlineConsonantManner manner;
	bool voiced;        // true = modal fold tension, false = slack/aspirated folds
	float tongueX;      // 0..10 dorsal bias during consonant
	float tongueY;      // 0..10 dorsal height during consonant
	float velum;        // 0..1 (1.0 for nasals M, N, NG)
	float constrPos;    // 0..10 (0 = off, 3.5 = velar, 6.0 = postalv, 7.6 = alv, 9.5 = labial)
	float constrWidth;  // 0..10 (0 = stop/nasal closure, 4.8 = fricative window, 6.8 = glide)
	float fricative;    // 0..1 turbulence injection level
	float voicing;      // 0..1 steady-state fold vibration (1 = voiced, 0 = voiceless breath only)
	float pressure;     // 0..1 glottal pressure / airflow scale
};

struct CyberlineBroca : rack::engine::Module {
	enum ParamIds {
		VOWELA_PARAM,
		VMORPH_PARAM,
		VOWELB_PARAM,
		CONSONANT_PARAM,
		CVMORPH_PARAM,
		ONSET_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		TRIG_INPUT,
		VOWELA_INPUT,
		VMORPH_INPUT,
		CONSONANT_INPUT,
		CVMORPH_INPUT,
		SYNC_INPUT,
		VOWELB_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		TRACT_POLY_OUTPUT,   // 9-ch poly: TNGX, TNGY, VEL, CPOS, CWID, FRIC, TENS(0), PRESS, VOICING
		LARYNX_POLY_OUTPUT,  // 3-ch poly: TENS (ch 0), PRESS (ch 1), VOICING (ch 2)
		CVENV_OUTPUT,        // mono 0-10V: active Consonant-to-Vowel envelope
		BURST_OUTPUT,        // mono 0-10V: frication / plosive release burst gate
		NUM_OUTPUTS
	};
	enum LightIds {
		NUM_LIGHTS
	};

	static constexpr int NUM_VOWELS = 12;
	static constexpr int NUM_CONSONANTS = 21;

	static const CyberlineVowelTarget VOWELS[NUM_VOWELS];
	static const CyberlineConsonantTarget CONSONANTS[NUM_CONSONANTS];

	// Trigger & syllable onset state
	bool trigWasHigh = false;
	bool syncWasHigh = false;
	bool onsetActive = false;
	float onsetPhase = 1.f;     // 0..1 progress through triggered consonant onset
	int scanStep = 0;           // cyclical step offset (advanced on TRIG when SYNC is used, reset on SYNC)

	// Smoothed anatomical outputs (in parameter-space units before offset conversion)
	float curTongueX = 5.0f;
	float curTongueY = 5.0f;
	float curVelum = 0.01f;
	float curConstrPos = 0.0f;
	float curConstrWidth = 5.0f;
	float curFric = 0.0f;
	float curVoicing = 1.0f;
	float curPressure = 0.8f;

	// Telemetry for the HUD widget
	int activeVowelA = 0;
	int activeVowelB = 4;
	int activeConsonant = 11;
	float activeVMorph = 0.f;
	float activeCVMix = 0.f;
	float activeBurst = 0.f;

	CyberlineBroca() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);

		configSwitch(VOWELA_PARAM, 0.f, (float)(NUM_VOWELS - 1), 0.f, "Vowel A", {
			"EE /i/ (close front)",
			"IH /ɪ/ (near-close front)",
			"EH /e/ (mid front)",
			"AE /æ/ (near-open front)",
			"AH /ɑ/ (open back)",
			"AW /ɔ/ (open-mid back)",
			"OH /o/ (close-mid back)",
			"OO /u/ (close back rounded)",
			"UH /ʊ/ (near-close back)",
			"UX /ʌ/ (open-mid central)",
			"ER /ɜ/ (rhotic mid central)",
			"SCHWA /ə/ (neutral mid central)"
		});
		configParam(VMORPH_PARAM, 0.f, 1.f, 0.f, "Vowel A-B morph", "%", 0.f, 100.f);
		configSwitch(VOWELB_PARAM, 0.f, (float)(NUM_VOWELS - 1), 4.f, "Vowel B", {
			"EE /i/ (close front)",
			"IH /ɪ/ (near-close front)",
			"EH /e/ (mid front)",
			"AE /æ/ (near-open front)",
			"AH /ɑ/ (open back)",
			"AW /ɔ/ (open-mid back)",
			"OH /o/ (close-mid back)",
			"OO /u/ (close back rounded)",
			"UH /ʊ/ (near-close back)",
			"UX /ʌ/ (open-mid central)",
			"ER /ɜ/ (rhotic mid central)",
			"SCHWA /ə/ (neutral mid central)"
		});

		configSwitch(CONSONANT_PARAM, 0.f, (float)(NUM_CONSONANTS - 1), 11.f, "Consonant", {
			"NONE (pure vowel)",
			"M /m/ (voiced bilabial nasal)",
			"N /n/ (voiced alveolar nasal)",
			"NG /ŋ/ (voiced velar nasal)",
			"W /w/ (voiced labial-velar glide)",
			"Y /j/ (voiced palatal glide)",
			"L /l/ (voiced alveolar lateral)",
			"R /ɹ/ (voiced postalveolar approximant)",
			"F /f/ (voiceless labiodental fricative)",
			"V /v/ (voiced labiodental fricative)",
			"TH /θ/ (voiceless dental fricative)",
			"S /s/ (voiceless alveolar sibilant)",
			"Z /z/ (voiced alveolar sibilant)",
			"SH /ʃ/ (voiceless postalveolar sibilant)",
			"H /h/ (voiceless glottal aspirate)",
			"P /p/ (voiceless bilabial plosive)",
			"B /b/ (voiced bilabial plosive)",
			"T /t/ (voiceless alveolar plosive)",
			"D /d/ (voiced alveolar plosive)",
			"K /k/ (voiceless velar plosive)",
			"G /g/ (voiced velar plosive)"
		});
		configParam(CVMORPH_PARAM, 0.f, 1.f, 0.f, "Consonant/Vowel mix", "%", 0.f, 100.f);
		configParam(ONSET_PARAM, 0.02f, 0.35f, 0.11f, "Consonant onset / glide time", " s");

		configInput(TRIG_INPUT, "Syllable trigger (fires consonant onset -> vowel; steps scan when SYNC is patched)");
		configInput(VOWELA_INPUT, "Vowel A scan CV (±5 V / 0-10 V)");
		configInput(VMORPH_INPUT, "Vowel A-B morph CV (0-10 V)");
		configInput(VOWELB_INPUT, "Vowel B scan CV (±5 V / 0-10 V)");
		configInput(CONSONANT_INPUT, "Consonant scan CV (±5 V / 0-10 V)");
		configInput(CVMORPH_INPUT, "Consonant/Vowel mix CV (0-10 V)");
		configInput(SYNC_INPUT, "Sync / Reset (trigger: resets syllable cycle & scan step to 0)");

		configOutput(TRACT_POLY_OUTPUT, "Tract Poly Bus (Ch 1-6: TNGX, TNGY, VEL, CPOS, CWID, FRIC; Ch 7-9: TENS, PRESS, VOICING)");
		configOutput(LARYNX_POLY_OUTPUT, "Larynx Poly Bus (Ch 1: TENS, Ch 2: PRESS, Ch 3: VOICING)");
		configOutput(CVENV_OUTPUT, "Consonant onset envelope (0-10 V)");
		configOutput(BURST_OUTPUT, "Fricative / plosive burst gate (0-10 V)");
	}

	void process(const ProcessArgs& args) override;

	void onReset() override {
		trigWasHigh = false;
		syncWasHigh = false;
		onsetActive = false;
		onsetPhase = 1.f;
		scanStep = 0;
		curTongueX = 5.0f;
		curTongueY = 5.0f;
		curVelum = 0.01f;
		curConstrPos = 0.0f;
		curConstrWidth = 5.0f;
		curFric = 0.0f;
		curVoicing = 1.0f;
		curPressure = 0.8f;
		activeBurst = 0.f;
	}
};

} // namespace cyberline
