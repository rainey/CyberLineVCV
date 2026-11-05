// CyberlineSagittaCore — vocal-system fluoroscopy core (rack-free, testable
// like the other Cyberline cores). Spec: docs/spec.md §5f.
//
// A display-only tap of the lungs->larynx->tract pipeline: it mirrors the
// CV contract of the real consumers (Oral Chamber / Larynx, see their
// configInput descriptions and CyberlineBroca's "sink default knob"
// encoding) and runs its own replica CyberlineTract waveguide from the
// tapped values, so the drawn anatomy (tongue profile, velum flap,
// constriction) IS the model geometry, and the internal sound-pressure
// meters (amplitude[]) can glow inside the cavity while it speaks.
//
// Inputs (as voltages, decoded here):
//   TRACT  poly 6-9 ch: TNGX, TNGY, VEL, CPOS, CWID, FRIC[, TENS, PRESS, VOIC]
//   LARYNX poly 3 ch:  TENS, PRESS, VOIC  (used when TRACT has < 9 ch)
//   lungs pressure (−5..+10 V), volume (0..10 V), signed flow (±5 V)
//   voiced audio (drives the replica's glottal source + fold tracking)
//
// Two CV interpretations (mirrors how the sinks sum knob + CV):
//   BUS    — Broca convention: tongue/constriction channels are offsets
//            around the sink default knobs (5.0/5.0/-/0/5.0/0), velum and
//            fricative are absolute 0-10 V.
//   DIRECT — channels are absolute 0-10 V parameters.
//
// F0 is tracked from the voiced input with a hysteresis zero-crossing
// counter (drives the fold-wobble animation).
//
// Telemetry: a double-buffered Snapshot (diameter + amplitude profiles,
// decoded scalars) published at a 32-sample control cadence; the widget
// reads the completed one through an atomic index (same pattern as the
// Visual1 frames). No locks in the audio path; NaN guards throughout.

#pragma once
#include <cmath>
#include <cstring>
#include <atomic>
#include "CyberlineTract.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace cyberline {

struct CyberlineSagittaCore {
	static constexpr float VTL_CM = 17.5f;   // fixed adult replica geometry
	static constexpr int CTL_INTERVAL = 32;  // snapshot cadence (samples)

	struct Snapshot {
		// tract geometry (section counts from the replica)
		int n = 44;
		int noseStart = 32;
		int noseLength = 28;
		int bladeStart = 10;
		int tipStart = 32;
		// oral profile (cm) + sound-pressure meters
		float diam[TRACT_SECTIONS_MAX];
		float amp[TRACT_SECTIONS_MAX];
		// nasal branch
		float noseDiam[NOSE_SECTIONS_MAX];
		float noseAmp[NOSE_SECTIONS_MAX];
		// decoded articulation (for HUD + larynx drawing)
		float tongueX = 5.f, tongueY = 5.f;
		float velum = 0.01f, constrPos = 0.f, constrWidth = 5.f, fricative = 0.f;
		float tension = 0.6f, press = 0.8f, voicing = 1.f;
		// lungs telemetry (normalized)
		float lungsVol = 0.f;      // 0..1
		float lungsPress = 0.f;    // -0.5..1
		float lungsFlow = 0.f;     // -1..1 signed (+ inhale)
		// audio telemetry
		float f0 = 0.f;            // Hz, 0 = no pitch tracked
		float voicedEnv = 0.f;     // |voiced| envelope
		float lipEnv = 0.f;        // replica lip radiation envelope
		float noseEnv = 0.f;       // replica nose radiation envelope
		float foldPhase = 0.f;     // 0..1 fold cycle phase at snapshot time
		bool busMode = true;
		bool alive = false;        // set once the first snapshot lands
	};

	CyberlineTract tract;

	bool busMode = true;
	float gain = 1.f;      // voiced input gain
	float sampleRate = 44100.f;

	// decoded articulation (HUD mirror)
	float tongueX = 5.f, tongueY = 5.f;
	float velumCv = 0.01f, constrPosCv = 0.f, constrWidthCv = 5.f, fricCv = 0.f;
	float tension = 0.6f, press = 0.8f, voicing = 1.f;

	// lungs telemetry
	float lungsVol = 0.f, lungsPress = 0.f, lungsFlow = 0.f;

	// audio telemetry / fold tracking
	float voicedEnv = 0.f;
	float f0 = 0.f;
	float foldPhase = 0.f;
	float crossTimer = 0.f;
	float lastCross = 0.f;
	int crossState = 0;     // -1/+1 hysteresis
	float lipEnv = 0.f, noseEnv = 0.f;

	// snapshot plumbing (double buffer + atomic publish)
	Snapshot snaps[2];
	std::atomic<int> snapIdx;
	int writeIdx = 1;
	uint32_t ctlCounter = 0;

	CyberlineSagittaCore() : snapIdx(0) {
		std::memset(snaps, 0, sizeof(snaps));
		setSampleRate(sampleRate);
		reset();
	}

	void setSampleRate(float sr) {
		if (!(sr >= 1000.f)) sr = 44100.f;
		sampleRate = sr;
		tract.setGeometry(VTL_CM, sr);
	}

	void reset() {
		tract.reset();
		tract.muscleSpeed = 15.f;
		tongueX = 5.f; tongueY = 5.f;
		velumCv = 0.01f; constrPosCv = 0.f; constrWidthCv = 5.f; fricCv = 0.f;
		tension = 0.6f; press = 0.8f; voicing = 1.f;
		lungsVol = lungsPress = lungsFlow = 0.f;
		voicedEnv = f0 = 0.f;
		foldPhase = crossTimer = lastCross = 0.f;
		crossState = 0;
		lipEnv = noseEnv = 0.f;
		ctlCounter = 0;
		publishSnapshot();
	}

	static float clampf(float x, float lo, float hi) {
		return x < lo ? lo : (x > hi ? hi : x);
	}

	// ---- one audio sample ----
	// tractBus: up to 9 channel voltages (ch count tractCh; missing -> 0 V)
	// larynxBus: up to 3 channel voltages
	// lungsPressV/lungsVolV/lungsFlowV: raw Lungs output voltages
	// voiced: voiced-audio input (post gain), volts
	void process(const float* tractBus, int tractCh,
			const float* larynxBus, int larynxCh,
			float lungsPressV, float lungsVolV, float lungsFlowV,
			float voiced, float sampleTime) {
		// ---- decode CVs (NaN guards) ----
		float tb[9] = {0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f};
		int tc = tractCh < 0 ? 0 : (tractCh > 9 ? 9 : tractCh);
		for (int i = 0; i < tc; i++) {
			float v = tractBus[i];
			tb[i] = std::isfinite(v) ? v : 0.f;
		}
		float lb[3] = {0.f, 0.f, 0.f};
		int lc = larynxCh < 0 ? 0 : (larynxCh > 3 ? 3 : larynxCh);
		for (int i = 0; i < lc; i++) {
			float v = larynxBus[i];
			lb[i] = std::isfinite(v) ? v : 0.f;
		}
		if (!std::isfinite(lungsPressV)) lungsPressV = 0.f;
		if (!std::isfinite(lungsVolV)) lungsVolV = 0.f;
		if (!std::isfinite(lungsFlowV)) lungsFlowV = 0.f;
		if (!std::isfinite(voiced)) voiced = 0.f;

		// Tract channels: BUS = offsets around sink default knobs (Broca),
		// DIRECT = absolute 0-10 V. Mirrors CyberlineOralChamber's decode.
		float txBase = busMode ? 5.f : 0.f;
		float tyBase = busMode ? 5.f : 0.f;
		float velBase = busMode ? 0.01f : 0.f;
		float cwBase = busMode ? 5.f : 0.f;
		tongueX = clampf(txBase + tb[0], 0.f, 10.f);
		tongueY = clampf(tyBase + tb[1], 0.f, 10.f);
		velumCv = clampf(velBase + tb[2] * 0.1f, 0.f, 1.f);
		constrPosCv = tb[3];
		constrWidthCv = clampf(cwBase + tb[4], 0.f, 10.f);
		fricCv = clampf(tb[5] * 0.1f, 0.f, 1.f);

		// Larynx channels: prefer the 9-ch tract bus tail, else the larynx
		// bus, else sink defaults (mirrors CyberlineLarynx's decode).
		if (tc >= 9) {
			tension = clampf(0.6f + tb[6] * 0.1f, 0.f, 1.f);
			press = clampf(0.8f * tb[7] * 0.1f, 0.f, 1.25f);
			voicing = clampf(tb[8] * 0.1f, 0.f, 1.f);
		} else if (lc >= 2) {
			tension = clampf(0.6f + lb[0] * 0.1f, 0.f, 1.f);
			press = clampf(0.8f * lb[1] * 0.1f, 0.f, 1.25f);
			voicing = (lc >= 3) ? clampf(lb[2] * 0.1f, 0.f, 1.f) : 1.f;
		} else {
			tension = 0.6f; press = 0.8f; voicing = 1.f;
		}

		// Lungs telemetry (same scaling as the Lungs outputs)
		lungsPress = clampf(lungsPressV, -5.f, 10.f) / 10.f;
		lungsVol = clampf(lungsVolV, 0.f, 10.f) / 10.f;
		lungsFlow = clampf(lungsFlowV, -5.f, 5.f) / 5.f;

		// ---- feed the replica tract (control-rate shape, mirrors sink) ----
		if (ctlCounter == 0) {
			int n = tract.n;
			float txN = tongueX / 10.f;
			float tyN = tongueY / 10.f;
			int idx = tract.bladeStart + (int) (txN * (tract.tipStart - tract.bladeStart));
			tract.setTongue(idx, CyberlineTract::mapTongueYDiameter(tyN));
			tract.setVelum(velumCv);
			float cw = CyberlineTract::mapConstrWidth(constrWidthCv / 10.f);
			float cpos = (constrPosCv <= 0.01f) ? -1.f
				: tract.bladeStart + clampf(constrPosCv, 0.f, 10.f) / 10.f
					* (float) (n - tract.bladeStart);
			tract.setConstriction(cpos, cw, fricCv);
			tract.applyConstriction();
		}

		// ---- voiced input -> glottal source + envelope + F0 ----
		float src = voiced * 0.2f;   // same scaling as Oral Chamber SOURCE
		voicedEnv += (std::fabs(voiced) - voicedEnv)
			* clampf(sampleTime / 0.012f, 0.f, 0.5f);

		// hysteresis zero-crossing F0 tracker
		crossTimer += sampleTime;
		float th = clampf(0.35f * voicedEnv, 0.045f, 1.f);
		if (crossState >= 0 && voiced < -th) {
			crossState = -1;
		} else if (crossState <= 0 && voiced > th) {
			if (crossState < 0) noteCross();   // rising edges only (full periods)
			crossState = 1;
		}
		if (f0 > 0.f)
			foldPhase += f0 * sampleTime;
		else
			foldPhase += 95.f * sampleTime; // idle flutter when unvoiced
		if (foldPhase >= 1.f) foldPhase -= std::floor(foldPhase);

		// ---- run the replica waveguide ----
		float noise = (float) ((rand() & 0xffff) - 0x8000) / 32768.f;
		float lip = tract.process(sampleTime, src, 0.f, -1.f, noise);
		lipEnv += (std::fabs(lip) - lipEnv) * clampf(sampleTime / 0.010f, 0.f, 0.5f);
		noseEnv += (std::fabs(tract.lastNoseOutput) - noseEnv)
			* clampf(sampleTime / 0.010f, 0.f, 0.5f);

		// ---- publish telemetry at control cadence ----
		if (++ctlCounter >= CTL_INTERVAL) {
			ctlCounter = 0;
			publishSnapshot();
		}
	}

	void noteCross() {
		// Full-period timing: measure rising-edge to rising-edge only (the
		// falling edge just flips the hysteresis state), so F0 is not
		// doubled by counting every zero crossing.
		float period = crossTimer - lastCross;
		lastCross = crossTimer;
		if (period >= 0.0012f && period <= 0.03f) {
			float raw = 1.f / period;
			if (raw >= 40.f && raw <= 900.f) {
				f0 += (raw - f0) * 0.25f;   // fast lock
				return;
			}
		}
		// no valid period: drift toward silence slowly
		f0 *= 0.98f;
	}

	// ---- telemetry publish (writer side) ----
	void publishSnapshot() {
		Snapshot& s = snaps[writeIdx];
		s.n = tract.n;
		s.noseStart = tract.noseStart;
		s.noseLength = tract.noseLength;
		s.bladeStart = tract.bladeStart;
		s.tipStart = tract.tipStart;
		std::memcpy(s.diam, tract.diameter, sizeof(float) * tract.n);
		std::memcpy(s.amp, tract.amplitude, sizeof(float) * tract.n);
		std::memcpy(s.noseDiam, tract.noseDiameter,
			sizeof(float) * tract.noseLength);
		std::memcpy(s.noseAmp, tract.noseAmplitude,
			sizeof(float) * tract.noseLength);
		s.tongueX = tongueX;
		s.tongueY = tongueY;
		s.velum = velumCv;
		s.constrPos = constrPosCv;
		s.constrWidth = constrWidthCv;
		s.fricative = fricCv;
		s.tension = tension;
		s.press = press;
		s.voicing = voicing;
		s.lungsVol = lungsVol;
		s.lungsPress = lungsPress;
		s.lungsFlow = lungsFlow;
		s.f0 = f0;
		s.voicedEnv = voicedEnv;
		s.lipEnv = lipEnv;
		s.noseEnv = noseEnv;
		s.foldPhase = foldPhase;
		s.busMode = busMode;
		s.alive = true;
		// NaN guard: kill non-finite scalars (profiles come from the tract's
		// own guards; still sweep them cheaply)
		for (int i = 0; i < s.n; i++) {
			if (!std::isfinite(s.diam[i])) s.diam[i] = 0.6f;
			if (!std::isfinite(s.amp[i])) s.amp[i] = 0.f;
		}
		for (int i = 0; i < s.noseLength; i++) {
			if (!std::isfinite(s.noseDiam[i])) s.noseDiam[i] = 0.1f;
			if (!std::isfinite(s.noseAmp[i])) s.noseAmp[i] = 0.f;
		}
		snapIdx = writeIdx;   // acquire/release: publish the complete frame
		writeIdx ^= 1;
	}

	// Widget side: pointer to the most recent complete snapshot.
	const Snapshot* snapshot() const {
		return &snaps[snapIdx.load(std::memory_order_acquire)];
	}
};

} // namespace cyberline
