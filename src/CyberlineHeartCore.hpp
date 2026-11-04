// CyberlineHeartCore — heart pump DSP (rack-free, testable like
// Glottis/Tract). Spec: docs/spec.md §5c.
//
// Time-varying elastance pump (Suga–Sagawa family): atrium → ventricle →
// arterial Windkessel. Four rules generate the Wiggers cycle:
//   1. P = E(t)·V per chamber; E(t) is a contraction twitch (fast rise,
//      plateau, slow decay) fired by the activation event.
//   2. Valves are pressure comparisons (inflow open iff P_atrium >
//      P_ventricle; outflow open iff P_ventricle > P_artery).
//   3. The artery is a reservoir: fills during ejection, exponential
//      runoff toward the diastolic baseline between beats.
//   4. Activation: the SA node free-runs; an external pacer overdrives it;
//      when pacing stops the intrinsic rhythm returns (escape beats).
//
// Frank–Starling (more fill → harder beat), the isovolumetric plateaus,
// S1/S2 timing, and afterload sensitivity are all emergent — no per-phase
// envelopes are coded.
//
// State floor is 0 with no hidden constants (suite convention, spec §2).
// Physics runs at a 32-sample control cadence; ECG, phonocardiogram thumps
// and trigger pulses are generated per sample (ECG cannot be control-rate
// interpolated without visibly quantizing the QRS at high heart rates).
// Signed-CV convention applies to FLOW: + filling, − ejection.

#pragma once
#include <cmath>
#include <cstdint>

namespace cyberline {

inline float heart_clampf(float x, float lo, float hi) {
	return x < lo ? lo : (x > hi ? hi : x);
}

struct CyberlineHeartCore {
	// ---- parameter snapshot (module fills every sample) ----
	struct Params {
		float rate = 72.f;        // intrinsic SA rate, bpm (at human anatomy)
		float mass = 0.74f;       // 0..1 -> log body mass 5 g..2000 kg; 0.74 = 70 kg
		float avDelay = 0.15f;    // PR interval, s (AV node delay)
		float fill = 0.5f;        // venous return (preload)
		float drive = 0.f;        // autonomic tone: -1 vagal .. +1 sympathetic
		float afterload = 0.4f;   // arterial resistance
		float contract = 0.6f;    // contractility
		float compliance = 0.5f;  // ventricular wall compliance (0 stiff .. 1 soft)
		float valve = 0.f;        // -1 stenotic .. 0 healthy .. +1 regurgitant
		float hrv = 0.15f;        // heart-rate variability depth (0 = exact clock)
		float irrit = 0.f;        // ectopic-beat probability (0 = off)
		float soundLvl = 0.7f;    // phonocardiogram level
		float fillCv = 0.f;       // 0-10 V
		float afterloadCv = 0.f;  // 0-10 V
		float driveCv = 0.f;      // -5..+5 V
		bool pacing = false;      // external pacer level (edge detected here)
	};

	struct Outputs {
		float volume = 0.f;    // 0..1 ventricular volume (pre 10 V scale)
		float pressure = 0.f;  // 0..1 ventricular pressure
		float flow = 0.f;      // -1..1 signed: + filling, - ejection
		float outflow = 0.f;   // 0..1 aortic flow pulse (unipolar)
		float arterial = 0.f;  // 0..1 Windkessel pressure
		float ecg = 0.f;       // -1..1 stylized P-QRS-T (pre +-5 V scale)
		float beat = 0.f;      // 0/1 QRS trigger pulse
		float atria = 0.f;     // 0/1 P-wave trigger pulse
		float sound = 0.f;     // phonocardiogram (pre level scale)
	};

	float sampleRate = 44100.f;
	float sampleTime = 1.f / 44100.f;

	// ---- chamber state (normalized 0..1) ----
	float avol = 0.6f;    // atrium
	float vvol = 0.6f;    // ventricle
	float apart = 0.4f;   // artery (Windkessel)
	float flowIn = 0.f;
	float flowOut = 0.f;
	float vpres = 0.f;    // ventricular pressure (internal, may exceed 1)
	bool inflowOpen = false;
	bool outflowOpen = false;

	// ---- timing / autonomic state ----
	float saTimer = 0.f;       // since last SA activation, s
	float avCountdown = -1.f;  // time to next ventricular activation (<0 none)
	float period = 0.8f;       // current beat period, s
	float ecgPeriod = 0.8f;    // period snapshot for ECG wave widths
	float hrvWander = 0.f;     // bounded per-beat rate wander, -hrv..+hrv
	float symp = 0.f;          // smoothed sympathetic drive (lags ~3 s)
	float vagal = 0.f;         // smoothed vagal drive (fast ~0.35 s)
	float diagRate = 72.f;     // effective bpm (diagnostics; = rateHz, in bpm)
	// allometric factors, refreshed each physics tick
	float mRateF = 1.f;        // rate  ∝ M^-0.25
	float mPitchF = 1.f;       // sound ∝ M^-1/3
	float mAvdF = 1.f;         // PR    ∝ M^+0.25

	bool pacingWas = false;
	bool pacePending = false;  // pacing edge caught between physics ticks

	// ---- event ages (audio domain, seconds) ----
	float tA = 10.f;  // since atrial activation (P wave)
	float tV = 10.f;  // since ventricular activation (QRS)

	// ---- stroke bookkeeping ----
	float ejected = 0.f;
	float lastStroke = 0.f;    // completed stroke volume of the previous beat

	// ---- lights / misc ----
	float systolic = 0.f;      // 1 during ejection (light)
	float filling = 0.f;       // 1 while AV valve open (light)

	// ---- phonocardiogram: S1 (AV valve), S2 (aortic), S3, S4 ----
	enum { S1 = 0, S2, S3, S4, NUM_THUMPS };
	float thumpAge[NUM_THUMPS] = {10.f, 10.f, 10.f, 10.f};
	float thumpAmp[NUM_THUMPS] = {0.f, 0.f, 0.f, 0.f};
	float thumpFreq[NUM_THUMPS] = {58.f, 90.f, 45.f, 38.f};
	float thumpTau[NUM_THUMPS] = {0.09f, 0.055f, 0.10f, 0.12f};
	float murmurGate = 0.f;
	float murmurLp = 0.f;
	bool s1Fired = false;
	bool s2Fired = false;
	bool s3Fired = false;

	// ---- trigger pulses ----
	float beatPulse = 0.f;   // countdown, s
	float atriaPulse = 0.f;

	// ---- RNG (xorshift32) ----
	uint32_t rngState = 0x1234abcd;
	float uniform() {
		uint32_t s = rngState;
		s ^= s << 13; s ^= s >> 17; s ^= s << 5;
		rngState = s;
		return (float)(s & 0xFFFFFF) / 16777216.f;
	}

	static constexpr int PHYS_INTERVAL = 32;
	int physCounter = 0;

	// per-sample smoothed outputs + control-rate targets
	float outVolume = 0.f, outPressure = 0.f, outFlow = 0.f;
	float outOutflow = 0.f, outArterial = 0.f;
	float tgtVolume = 0.f, tgtPressure = 0.f, tgtFlow = 0.f;
	float tgtOutflow = 0.f, tgtArterial = 0.f;

	void setSampleRate(float sr) {
		sampleRate = sr;
		sampleTime = 1.f / sr;
	}

	void reset() {
		avol = vvol = 0.6f; apart = 0.4f;
		flowIn = flowOut = vpres = 0.f;
		inflowOpen = outflowOpen = false;
		saTimer = 0.f; avCountdown = -1.f;
		period = ecgPeriod = 0.8f;
		hrvWander = symp = vagal = 0.f;
		pacingWas = pacePending = false;
		tA = tV = 10.f;
		ejected = lastStroke = 0.f;
		systolic = filling = 0.f;
		for (int i = 0; i < NUM_THUMPS; i++) {
			thumpAge[i] = 10.f; thumpAmp[i] = 0.f;
		}
		murmurGate = 0.f;
		murmurLp = 0.f;
		s1Fired = s2Fired = s3Fired = false;
		beatPulse = atriaPulse = 0.f;
		outVolume = outPressure = outFlow = outOutflow = outArterial = 0.f;
		tgtVolume = tgtPressure = tgtFlow = tgtOutflow = tgtArterial = 0.f;
		physCounter = 0;
	}

	Outputs process(const Params& in) {
		// pacing edge detection must be per sample (short triggers can
		// fall between physics ticks); consumed at the next tick
		if (in.pacing && !pacingWas) pacePending = true;
		pacingWas = in.pacing;

		if (++physCounter >= PHYS_INTERVAL) {
			physCounter = 0;
			tickPhysics(in, PHYS_INTERVAL * sampleTime);
		}

		// ---- audio-rate event machinery ----
		tA = fminf(tA + sampleTime, 3.f);
		tV = fminf(tV + sampleTime, 3.f);

		Outputs o;
		float P = ecgPeriod;
		// stylized P-QRS-T: gaussians anchored to the activation ages,
		// widths relative to the beat period (auto-scaling with mass/rate).
		// Amplitude ratios follow the code (R ~3x T, T ~1.8x P)
		float ecg = 0.55f * g(tA, 0.050f * P, 0.030f * P)   // P
			- 0.70f * g(tV, 0.008f * P, 0.0060f * P)       // Q
			+ 3.00f * g(tV, 0.020f * P, 0.0075f * P)       // R
			- 1.10f * g(tV, 0.036f * P, 0.0080f * P)       // S
			+ 1.00f * g(tV, 0.300f * P, 0.070f * P);       // T
		o.ecg = heart_clampf(ecg * 0.32f, -1.f, 1.f);

		// phonocardiogram: smooth attack-decay thumps + low-passed murmur noise
		float sound = 0.f;
		for (int i = 0; i < NUM_THUMPS; i++) {
			if (thumpAmp[i] > 1e-4f && thumpAge[i] < 8.f * thumpTau[i]) {
				thumpAge[i] += sampleTime;
				// 3.5 ms soft onset ramp prevents derivative clicks at t = 0
				float attack = 1.f - std::exp(-thumpAge[i] / 0.0035f);
				float decay = std::exp(-thumpAge[i] / thumpTau[i]);
				sound += thumpAmp[i] * 1.15f * attack * decay
					* std::sin(6.2831853f * thumpFreq[i] * thumpAge[i]);
			}
		}
		float rawNoise = uniform() * 2.f - 1.f;
		murmurLp += (rawNoise - murmurLp) * 0.14f;
		sound = sound * 1.15f + murmurGate * 0.55f * murmurLp;
		o.sound = std::tanh(sound * in.soundLvl * 1.25f);

		// trigger pulses
		beatPulse = fmaxf(0.f, beatPulse - sampleTime);
		atriaPulse = fmaxf(0.f, atriaPulse - sampleTime);
		o.beat = beatPulse > 0.f ? 1.f : 0.f;
		o.atria = atriaPulse > 0.f ? 1.f : 0.f;

		o.volume = outVolume;
		o.pressure = outPressure;
		o.flow = outFlow;
		o.outflow = outOutflow;
		o.arterial = outArterial;

		// ---- per-sample output interpolation (Lungs pattern) ----
		// one-pole toward the control-rate targets: no stair-stepping on
		// scopes, no zipper noise when the CVs drive audio-rate targets
		float a = 0.08f;
		outVolume   += (tgtVolume   - outVolume)   * a;
		outPressure += (tgtPressure - outPressure) * a;
		outFlow     += (tgtFlow     - outFlow)     * a;
		outOutflow  += (tgtOutflow  - outOutflow)  * a;
		outArterial += (tgtArterial - outArterial) * a;
		if (!std::isfinite(outVolume)) reset();
		return o;
	}

	// ---------------- physics tick (control rate) ----------------
	void tickPhysics(const Params& in, float dt) {
		float fill = heart_clampf(in.fill + in.fillCv * 0.1f, 0.f, 1.f);
		float afterl = heart_clampf(in.afterload + in.afterloadCv * 0.1f, 0.f, 1.f);
		float drive = heart_clampf(in.drive + in.driveCv * 0.2f, -1.f, 1.f);

		// autonomic smoothing: sympathetic acts over seconds, vagal fast
		symp += (fmaxf(drive, 0.f) - symp) * (1.f - std::exp(-dt / 3.0f));
		vagal += (fmaxf(-drive, 0.f) - vagal) * (1.f - std::exp(-dt / 0.35f));

		// body mass allometry: 0..1 -> log 5 g .. 2000 kg
		float mass = std::exp(
			heart_clampf(in.mass, 0.f, 1.f)
				* (std::log(2000.f) - std::log(0.005f)) + std::log(0.005f));
		float m = mass / 70.f;  // 1 = human
		mRateF = std::pow(m, -0.25f);
		mPitchF = std::pow(m, -1.f / 3.f);
		mAvdF = std::pow(m, 0.25f);

		// intrinsic rate: knob × mass × autonomic × per-beat wander
		// (all in bpm; clamped to the physiological 25..1000 window)
		float rateHz = in.rate * mRateF
			* (1.f + 0.55f * symp - 0.35f * vagal)
			* (1.f + hrvWander);
		rateHz = heart_clampf(rateHz, 25.f, 1000.f);
		period = 60.f / rateHz;
		diagRate = rateHz;

		saTimer += dt;

		// Ventricular refractory window: no second activation this soon
		// after a beat (real myocardium cannot be re-triggered). Purely
		// relative to the period so it stays strictly shorter than the
		// clamped AV delay (otherwise paced fast rates hit the boundary
		// and randomly drop beats). Blocks stacked ectopics and drops a
		// sinus impulse arriving too early — which is what makes the
		// compensatory pause after an ectopic emerge (the next conducted
		// beat starts one full period later).
		float refr = 0.25f * period;

		// ectopic beat (irritability): premature ventricular activation,
		// SA timer NOT reset -> the compensatory pause and the stronger
		// post-ectopic beat (extra filling -> Starling) both emerge
		if (in.irrit > 0.001f && avCountdown < 0.f && tV >= refr) {
			if (uniform() < in.irrit * in.irrit * 0.12f * dt)
				fireVentricular();
		}

		// intrinsic SA firing
		if (saTimer >= period) {
			saTimer -= period;
			fireAtrial(in);
		}

		// external pacer: overdrive pacing (resets the SA clock, so when
		// pacing stops the intrinsic rhythm returns as escape beats)
		if (pacePending) {
			pacePending = false;
			saTimer = 0.f;
			fireAtrial(in);
		}

		// AV node conduction countdown
		if (avCountdown > 0.f) {
			avCountdown -= dt;
			if (avCountdown <= 0.f) {
				avCountdown = -1.f;
				// refractory ventricle: the P wave happens, the impulse
				// is dropped (no QRS) — the following beat arrives one
				// period late (compensatory pause)
				if (tV >= refr)
					fireVentricular();
			}
		}

		// ---- elastance -> pressures ----
		// ventricle: base stiffness (compliance) + contraction twitch
		// Sympathetic lusitropy slightly relaxes diastolic wall stiffness
		// while positive inotropy boosts peak systolic elastance.
		// Use ecgPeriod (snapshotted at fireVentricular) so HRV updates at
		// fireAtrial do not discontinuously jump Ev mid-diastole.
		float Ebase = (0.10f / (0.35f + 0.65f * in.compliance)) * (1.f - 0.18f * symp);
		float ampV = 2.2f * (0.35f + 0.65f * in.contract) * (1.f + 0.50f * symp);
		float r = 0.05f * ecgPeriod, d = 0.17f * ecgPeriod;
		float Ev = Ebase + ampV * normTw(tV, r, d);
		vpres = Ev * vvol;

		// atrium: venous pressure behind it (preload + sympathetic
		// venoconstriction) plus a weaker twitch fired at the P wave —
		// the "atrial kick" tops up passive filling
		float venous = (0.03f + 0.13f * fill) * (1.f + 0.55f * symp);
		float ampA = 0.55f * (0.35f + 0.65f * in.contract) * (1.f + 0.25f * symp);
		float ra = 0.025f * period, da = 0.07f * period;
		float apres = venous + (0.06f + ampA * normTw(tA, ra, da)) * avol;

		// ---- valves: pressure comparisons ----
		float sten = fmaxf(-in.valve, 0.f);
		float reg = fmaxf(in.valve, 0.f);

		bool inflowWas = inflowOpen;
		inflowOpen = apres > vpres + 0.004f;
		float kIn = 14.f * (1.f + 1.1f * symp) * (1.f - 0.55f * sten);
		flowIn = inflowOpen ? fminf(kIn * (apres - vpres), 5.f) : 0.f;
		avol = heart_clampf(avol - flowIn * dt, 0.f, 1.f);
		vvol = heart_clampf(vvol + flowIn * dt, 0.f, 1.f);
		if (!s1Fired && inflowWas && !inflowOpen && tV < 0.08f * ecgPeriod && vvol > 0.05f) {
			// S1: AV valve slams shut once at the onset of ventricular systole
			// (diastolic pressure equilibration / diastasis on long HRV beats
			// gently floats the leaflets closed without slamming S1)
			s1Fired = true;
			thumpAmp[S1] = 0.55f + 0.45f * in.contract;
			thumpAge[S1] = 0.f;
			thumpFreq[S1] = 58.f * mPitchF;
		}
		if (!s3Fired && s2Fired && !inflowWas && inflowOpen) {
			// S3: rapid early-diastolic passive filling thump (once per beat after S2)
			s3Fired = true;
			thumpAmp[S3] = 0.30f * in.compliance
				* heart_clampf((apres - vpres) * 6.f, 0.f, 1.f);
			thumpAge[S3] = 0.f;
			thumpFreq[S3] = 45.f * mPitchF;
		}

		bool outflowWas = outflowOpen;
		outflowOpen = vpres > apart + 0.004f;
		float kOut = 8.f * (1.f + 0.60f * symp) / (1.f + 2.2f * afterl) * (1.f - 0.45f * sten);
		flowOut = outflowOpen ? fminf(kOut * (vpres - apart), 6.f) : 0.f;
		vvol = heart_clampf(vvol - flowOut * dt, 0.f, 1.f);
		if (!s2Fired && outflowWas && !outflowOpen) {
			// S2: aortic valve closure, end of ejection (once per beat)
			s2Fired = true;
			lastStroke = ejected;
			ejected = 0.f;
			thumpAmp[S2] = 0.45f + 0.35f * in.contract;
			thumpAge[S2] = 0.f;
			thumpFreq[S2] = 90.f * mPitchF;
		}
		if (outflowOpen) ejected += flowOut * dt;

		// regurgitant leaks: backward flow through the closed valve when
		// the pressure gradient is adverse (volume is lost to the system;
		// Starling compensation then raises contractility demand)
		float leakAV = reg * 2.5f * (!inflowOpen ? fmaxf(vpres - apres, 0.f) : 0.f);
		float leakAo = reg * 2.5f * (!outflowOpen ? fmaxf(apart - vpres, 0.f) : 0.f);
		avol = heart_clampf(avol + leakAV * dt, 0.f, 1.f);
		vvol = heart_clampf(vvol - leakAV * dt + leakAo * dt, 0.f, 1.f);
		apart = heart_clampf(apart - leakAo * dt, 0.f, 1.f);

		// ---- arterial Windkessel: ejection fills it, runoff drains it ----
		float tauW = (0.20f + 0.35f * afterl) / mRateF;
		float dias = 0.30f + 0.18f * afterl;
		apart += (dias - apart) * (dt / tauW) + flowOut * dt * 0.8f;
		apart = heart_clampf(apart, 0.f, 1.2f);

		// venous return refills the atrium toward preload-dependent capacity,
		// accelerated by sympathetic venoconstriction / muscle pump
		float vCap = heart_clampf(0.28f + 0.72f * fill, 0.f, 1.f);
		float vrRate = (0.25f + 1.15f * fill) * (1.f + 1.8f * symp);
		if (avol < vCap)
			avol = fminf(avol + vrRate * dt, vCap);

		// murmur gate: stenosis -> turbulence where valves flow open;
		// regurgitation -> turbulence where pressure builds behind a
		// leaking closed valve
		float flowAct = heart_clampf((std::fabs(flowIn) + std::fabs(flowOut)) * 0.4f, 0.f, 1.f);
		float pressAct = heart_clampf(vpres * 3.f, 0.f, 1.f);
		float mg = fmaxf(sten * flowAct, reg * pressAct);
		murmurGate += (mg - murmurGate) * 0.2f;

		// lights
		systolic = outflowOpen ? 1.f : 0.f;
		filling = flowIn > 0.05f ? 1.f : 0.f;

		// NaN guard: reinitialize state, keep running (suite convention)
		if (!std::isfinite(avol) || !std::isfinite(vvol)
			|| !std::isfinite(apart) || !std::isfinite(tV)) {
			reset();
		}

		// output targets (smoothed per sample by process())
		tgtVolume = vvol;
		tgtPressure = heart_clampf(vpres, 0.f, 1.f);
		tgtFlow = heart_clampf((flowIn - flowOut) * 0.28f, -1.f, 1.f);
		tgtOutflow = heart_clampf(flowOut * 0.5f, 0.f, 1.f);
		tgtArterial = heart_clampf(apart, 0.f, 1.f);
	}

	void fireAtrial(const Params& in) {
		tA = 0.f;
		atriaPulse = 0.012f;
		// HRV: bounded per-beat wander, recomputed each beat (perturbs the
		// interval length only — nothing integrates, HRV 0 = exact clock)
		if (in.hrv > 0.001f) {
			float target = (2.f * uniform() - 1.f) * in.hrv;
			hrvWander = heart_clampf(
				0.6f * hrvWander + 0.6f * target, -in.hrv, in.hrv);
		} else {
			hrvWander = 0.f;
		}
		// AV delay, mass-scaled; clamped to <=30% of the cycle (sympathetic
		// conduction speeds up at high rates — protects the atrial kick)
		avCountdown = fminf(fmaxf(in.avDelay * mAvdF, 0.02f), 0.30f * period);
		// S4: atrial kick into a stiff ventricle
		if (in.compliance < 0.35f) {
			thumpAmp[S4] = 0.30f * (0.35f - in.compliance) / 0.35f;
			thumpAge[S4] = 0.f;
			thumpFreq[S4] = 38.f * mPitchF;
		}
	}

	void fireVentricular() {
		tV = 0.f;
		ecgPeriod = period;
		beatPulse = 0.012f;
		s1Fired = false;
		s2Fired = false;
		s3Fired = false;
	}

	// normalized two-exponential twitch: fast rise (r), slow decay (d)
	static float normTw(float age, float r, float d) {
		if (age <= 0.f || r <= 0.f || d <= 0.f || age > 8.f * d) return 0.f;
		float f = (1.f - std::exp(-age / r)) * std::exp(-age / d);
		float ts = r * std::log(1.f + d / r);  // argmax
		float fmax = (1.f - std::exp(-ts / r)) * std::exp(-ts / d);
		return fmax > 1e-6f ? f / fmax : 0.f;
	}

	static float g(float x, float c, float s) {
		float z = (x - c) / s;
		float z2 = z * z;
		if (z2 > 16.f) return 0.f;
		return std::exp(-0.5f * z2);
	}
};

} // namespace cyberline
