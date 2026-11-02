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

	// Control targets (set externally every sample)
	float targetF0 = 110.f;        // Hz (already clamped by caller)
	float targetTenseness = 0.6f;  // 0..1 fold tension
	float targetAmp = 0.8f;        // 0..1 subglottal pressure (loudness)

	// Period-quantized parameters (uptake at period boundary)
	float oldF0 = 110.f, newF0 = 110.f;
	float oldTenseness = 0.6f, newTenseness = 0.6f;

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

	// Feedback (tract loading) drive, smoothed
	float fbSmooth = 0.f;

	// Control-rate smoothing cadence
	static constexpr int SMOOTH_INTERVAL = 32;
	int smoothCounter = 0;

	// Vibrato / drift phases
	float vibPhase = 0.f;
	float driftPhase1 = 0.123f, driftPhase2 = 0.456f;

	void setSampleRate(float sr) {
		if (sr < 1000.f) sr = 1000.f;
		sampleRate = sr;
	}

	void setTargets(float f0, float tenseness, float amp) {
		if (f0 < 20.f) f0 = 20.f;
		if (f0 > 2000.f) f0 = 2000.f;
		targetF0 = f0;
		if (tenseness < 0.01f) tenseness = 0.01f;
		if (tenseness > 1.f) tenseness = 1.f;
		targetTenseness = tenseness;
		if (amp < 0.f) amp = 0.f;
		if (amp > 1.f) amp = 1.f;
		targetAmp = amp;
	}

	// Derive LF coefficients for the current period (normalized to time = 1,
	// Ee = 1). Follows Fant, Liljencrants & Lin (1985).
	void setupWaveform(float f0, float tenseness) {
		Rd = 3.f * (1.f - tenseness);
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

		float rhsIntegral = (1.f / epsilon) * (shift - 1.f) + (1.f - Te) * shift;
		rhsIntegral = rhsIntegral / Delta;

		float totalLowerIntegral = -(Te - Tp) / 2.f + rhsIntegral;
		float totalUpperIntegral = -totalLowerIntegral;

		omega = M_PI / Tp;
		float s = std::sin(omega * Te);
		if (s < 1e-4f) s = 1e-4f;

		float y = -M_PI * s * totalUpperIntegral / (Tp * 2.f);
		if (y <= 1e-6f) y = 1e-6f;
		float z = std::log(y);
		alpha = z / (Tp / 2.f - Te);
		E0 = -1.f / (s * std::exp(alpha * Te));
	}

	void reset() {
		targetF0 = oldF0 = newF0 = 110.f;
		targetTenseness = oldTenseness = newTenseness = 0.6f;
		targetAmp = 0.8f;
		intensity = 0.f;
		timeInWaveform = 0.f;
		totalTime = 0.f;
		fbSmooth = 0.f;
		setupWaveform(110.f, 0.6f);
	}

	// One sample. noise: white noise in [-1, 1]. fbDrive: pre-scaled feedback
	// sample (module multiplies input by feedback gain). driftAmt: 0..1.
	// vibRate: Hz. Returns normalized voiced sample (roughly ±1).
	float process(float noise, float fbDrive, float driftAmt, float vibRate) {
		// Control-rate modulation + smoothing
		if (++smoothCounter >= SMOOTH_INTERVAL) {
			smoothCounter = 0;
			float dt = SMOOTH_INTERVAL / sampleRate;

			vibPhase += vibRate * dt;
			driftPhase1 += 0.7f * dt;
			driftPhase2 += 1.9f * dt;

			float vib = 0.006f * std::sin(2.f * M_PI * vibPhase);
			float d1 = 0.04f * driftAmt * std::sin(2.f * M_PI * driftPhase1 + 0.31f);
			float d2 = 0.06f * driftAmt * std::sin(2.f * M_PI * driftPhase2 + 1.13f);

			newF0 = targetF0 * (1.f + vib + d1);
			if (newF0 < 20.f) newF0 = 20.f;
			newTenseness = targetTenseness + d2;
			if (newTenseness < 0.05f) newTenseness = 0.05f;
			if (newTenseness > 1.f) newTenseness = 1.f;

			// Amplitude: smooth chase toward pressure target (~5 ms)
			intensity += (targetAmp - intensity) * 0.13f;

			// Feedback drive smoothing (avoid loop harshness/instability)
			fbSmooth += (fbDrive - fbSmooth) * 0.06f;
		}

		// Advance period; uptake period-quantized parameters at boundary
		timeInWaveform += 1.f / sampleRate;
		totalTime += 1.f / sampleRate;
		if (!(timeInWaveform < waveformLength) || waveformLength <= 0.f) {
			timeInWaveform = 0.f;
			oldF0 = newF0;
			oldTenseness = newTenseness;
			setupWaveform(newF0, newTenseness);
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
		float voicedGate = 0.1f + 0.2f * std::fmax(0.f, std::sin(2.f * M_PI * t));
		noiseModulator = targetTenseness * intensity * voicedGate
			+ (1.f - targetTenseness * intensity) * 0.3f;
		float aspiration = intensity
			* (1.f - std::sqrt(std::fmax(targetTenseness, 0.f)))
			* noiseModulator * noise;

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
