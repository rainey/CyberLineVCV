// CyberlineGlottis — Fant–Liljencrants–Lin (1985) glottal-flow-derivative
// generator. Reimplemented from the paper's two-branch waveform formulation
// (open phase: exponentially-growing sinusoid; closing: exponential recovery),
// with period-quantized parameter uptake, drift/vibrato modulation, aspiration
// noise gating, a semi-coupled feedback drive path, and NaN guards.
//
// Used by CyberlineLarynx (and later CyberlineVoice). No Rack dependency.

#pragma once
#include <cmath>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace cyberline {

struct CyberlineGlottis {
	float sampleRate = 44100.f;
	float intensityAlpha = 0.13f;
	float fbAlpha = 0.055f;

	// Control targets (set externally)
	float targetF0 = 110.f;        // Hz (already clamped by caller)
	float targetTenseness = 0.6f;  // 0..1 fold tension
	float targetAmp = 0.8f;        // 0..1 subglottal pressure (loudness)
	float targetFoldLen = 14.f;    // mm (vocal-fold length for Rd mass/timbre)

	// Period-quantized parameters (crossfaded by lambda at period boundary)
	float oldF0 = 110.f, newF0 = 110.f;
	float oldTenseness = 0.6f, newTenseness = 0.6f;
	float curTenseness = 0.6f;
	float oneMinusSqrtTenseness = 0.2254f;

	// Smoothed amplitude envelope
	float intensity = 0.f;

	float timeInWaveform = 0.f;
	float waveformLength = 1.f / 110.f;
	float totalTime = 0.f;

	// LF shape coefficients (per period)
	float Rd = 1.2f;
	float alpha = 0.f, E0 = 0.f, epsilon = 0.f, shift = 0.f, Delta = 0.f;
	float Te = 0.f, omega = 0.f;

	// Aspiration noise gate (glottal open phase), exposed for NOISE MOD output
	float noiseModulator = 0.3f;

	// Feedback (tract loading) drive, smoothed per-sample by 1-pole filter
	float fbSmooth = 0.f;

	// Control-rate smoothing cadence
	static constexpr int SMOOTH_INTERVAL = 32;
	int smoothCounter = 0;

	// Vibrato / drift phases + organic low-passed drift state
	float vibPhase = 0.f;
	float driftPhase1 = 0.123f, driftPhase2 = 0.456f, driftPhase3 = 0.789f;
	float driftNoise1 = 0.f, driftNoise2 = 0.f;

	CyberlineGlottis() {
		setSampleRate(44100.f);
		reset();
	}

	void setSampleRate(float sr) {
		if (sr < 1000.f) sr = 1000.f;
		sampleRate = sr;
		float dtBlock = (float) SMOOTH_INTERVAL / sampleRate;
		intensityAlpha = 1.f - std::exp(-dtBlock / 0.005f);
		fbAlpha = 1.f - std::exp(-1.f / (sampleRate * 0.0004f));
	}

	void setTargets(float f0, float tenseness, float amp, float foldLen = 14.f) {
		if (f0 < 20.f) f0 = 20.f;
		if (f0 > 2000.f) f0 = 2000.f;
		targetF0 = f0;
		if (tenseness < 0.01f) tenseness = 0.01f;
		if (tenseness > 1.f) tenseness = 1.f;
		targetTenseness = tenseness;
		if (amp < 0.f) amp = 0.f;
		if (amp > 1.f) amp = 1.f;
		targetAmp = amp;
		if (foldLen < 2.f) foldLen = 2.f;
		if (foldLen > 45.f) foldLen = 45.f;
		targetFoldLen = foldLen;
	}

	// Derive LF coefficients for the current period (normalized to time = 1,
	// Ee = 1). Follows Fant, Liljencrants & Lin (1985).
	void setupWaveform(float f0, float tenseness) {
		if (f0 < 20.f) f0 = 20.f;
		if (tenseness < 0.01f) tenseness = 0.01f;
		if (tenseness > 1.f) tenseness = 1.f;
		curTenseness = tenseness;
		oneMinusSqrtTenseness = 1.f - std::sqrt(tenseness);

		// Fold-length mass/dim timbre effect (spec §3): longer/thicker folds
		// have a slightly higher Rd (longer return phase Ta -> darker tilt).
		float massScale = std::pow(targetFoldLen / 14.f, 0.25f);
		Rd = 3.f * (1.f - tenseness) * massScale;
		waveformLength = 1.f / f0;

		float RdC = Rd;
		if (RdC < 0.5f) RdC = 0.5f;
		if (RdC > 2.7f) RdC = 2.7f;

		float Ra = -0.01f + 0.048f * RdC;
		float Rk = 0.224f + 0.118f * RdC;
		float Rg = (Rk / 4.f) * (0.5f + 1.2f * Rk) / (0.11f * RdC - Ra * (0.5f + 1.2f * Rk));

		float Ta = Ra;
		float Tp = 1.f / (2.f * Rg);
		Te = Tp + Tp * Rk;

		epsilon = 1.f / Ta;
		shift = std::exp(-epsilon * (1.f - Te));
		Delta = 1.f - shift;
		if (Delta < 1e-6f) Delta = 1e-6f;

		float rhsIntegral = (1.f / epsilon) * (shift - 1.f) + (1.f - Te) * shift;
		rhsIntegral = rhsIntegral / Delta;

		float totalLowerIntegral = -(Te - Tp) / 2.f + rhsIntegral;
		float totalUpperIntegral = -totalLowerIntegral;

		omega = (float) M_PI / Tp;
		float s = std::sin(omega * Te);
		if (s > -1e-4f) s = -1e-4f;

		float y = -(float) M_PI * s * totalUpperIntegral / (Tp * 2.f);
		if (y <= 1e-6f) y = 1e-6f;
		float z = std::log(y);
		alpha = z / (Tp / 2.f - Te);
		E0 = -1.f / (s * std::exp(alpha * Te));
	}

	void reset() {
		targetF0 = oldF0 = newF0 = 110.f;
		targetTenseness = oldTenseness = newTenseness = curTenseness = 0.6f;
		targetAmp = 0.8f;
		targetFoldLen = 14.f;
		intensity = 0.f;
		timeInWaveform = 0.f;
		totalTime = 0.f;
		fbSmooth = 0.f;
		smoothCounter = 0;
		vibPhase = 0.f;
		driftPhase1 = 0.123f; driftPhase2 = 0.456f; driftPhase3 = 0.789f;
		driftNoise1 = driftNoise2 = 0.f;
		setupWaveform(110.f, 0.6f);
	}

	// One sample. noise: white noise in [-1, 1]. fbDrive: pre-scaled feedback
	// sample (module multiplies input by feedback gain). driftAmt: 0..1.
	// vibRate: Hz. Returns normalized voiced sample (roughly ±1).
	float process(float noise, float fbDrive, float driftAmt, float vibRate) {
		// Per-sample 1-pole smoothing on tract feedback drive (avoids aliasing
		// and HF loop instability, spec §3)
		if (std::isfinite(fbDrive))
			fbSmooth += (fbDrive - fbSmooth) * fbAlpha;
		else
			fbSmooth = 0.f;

		// Control-rate modulation + smoothing
		if (++smoothCounter >= SMOOTH_INTERVAL) {
			smoothCounter = 0;
			float dt = (float) SMOOTH_INTERVAL / sampleRate;

			oldF0 = newF0;
			oldTenseness = newTenseness;

			vibPhase += vibRate * dt;
			vibPhase -= std::floor(vibPhase);
			driftPhase1 += 0.47f * dt;
			driftPhase1 -= std::floor(driftPhase1);
			driftPhase2 += 1.31f * dt;
			driftPhase2 -= std::floor(driftPhase2);
			driftPhase3 += 2.93f * dt;
			driftPhase3 -= std::floor(driftPhase3);

			// Filtered random walk for non-periodic organic drift
			driftNoise1 += (noise - driftNoise1) * 0.08f;
			driftNoise2 += (driftNoise1 - driftNoise2) * 0.08f;

			float vib = 0.006f * std::sin(2.f * (float) M_PI * vibPhase);
			float d1 = driftAmt * (0.025f * std::sin(2.f * (float) M_PI * driftPhase1 + 0.31f)
				+ 0.015f * std::sin(2.f * (float) M_PI * driftPhase3 + 2.07f)
				+ 0.03f * driftNoise2);
			float d2 = driftAmt * (0.045f * std::sin(2.f * (float) M_PI * driftPhase2 + 1.13f)
				+ 0.04f * driftNoise1);

			newF0 = targetF0 * (1.f + vib + d1);
			if (newF0 < 20.f) newF0 = 20.f;
			if (newF0 > 2000.f) newF0 = 2000.f;
			newTenseness = targetTenseness + d2;
			if (newTenseness < 0.05f) newTenseness = 0.05f;
			if (newTenseness > 1.f) newTenseness = 1.f;

			// Amplitude: smooth chase toward pressure target (~5 ms)
			intensity += (targetAmp - intensity) * intensityAlpha;
		}

		// Advance period; uptake period-quantized parameters at boundary,
		// crossfaded by lambda across the control block (spec §3)
		timeInWaveform += 1.f / sampleRate;
		totalTime += 1.f / sampleRate;
		if (!(timeInWaveform < waveformLength) || waveformLength <= 0.f) {
			if (waveformLength > 0.f) {
				timeInWaveform -= waveformLength;
				if (timeInWaveform >= waveformLength || timeInWaveform < 0.f)
					timeInWaveform = 0.f;
			} else {
				timeInWaveform = 0.f;
			}
			float lambda = (float) smoothCounter / (float) SMOOTH_INTERVAL;
			float f0Period = (oldF0 + (newF0 - oldF0) * lambda)
				* (1.f - 0.03f * std::fabs(fbSmooth) + 0.02f * fbSmooth);
			float tensPeriod = oldTenseness + (newTenseness - oldTenseness) * lambda
				- 0.12f * fbSmooth;
			setupWaveform(f0Period, tensPeriod);
		}

		// Effective amplitude: pressure intensity plus smoothed feedback drive
		float ampEff = intensity + fbSmooth * 0.6f;
		if (ampEff < 0.f) ampEff = 0.f;
		if (ampEff > 1.5f) ampEff = 1.5f;

		// LF waveform (two branches)
		float t = timeInWaveform / waveformLength;
		float wave;
		if (t > Te)
			wave = (-std::exp(-epsilon * (t - Te)) + shift) / Delta;
		else
			wave = E0 * std::exp(alpha * t) * std::sin(omega * t);

		// Aspiration noise, gated to the open phase
		float voicedGate = 0.1f + 0.2f * std::fmax(0.f, std::sin(2.f * (float) M_PI * t));
		float ti = curTenseness * intensity;
		noiseModulator = ti * voicedGate + (1.f - ti) * 0.3f;
		float aspiration = intensity * oneMinusSqrtTenseness * noiseModulator * noise;

		float out = wave * ampEff + aspiration * 0.2f;

		// NaN/inf guard: reinitialize and continue
		if (!std::isfinite(out) || !std::isfinite(timeInWaveform) || !std::isfinite(totalTime)) {
			reset();
			out = 0.f;
		}
		return out;
	}
};

} // namespace cyberline
