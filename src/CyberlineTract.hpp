// CyberlineTract — Kelly–Lochbaum waveguide vocal tract (pure DSP, no Rack).
// Spec: docs/spec.md §4.
//
// Structure follows the verified PT/tract arrangement (single-sample sections,
// 2x oversample stepping with reflection-coefficient crossfade per step pair,
// three-way mouth/nose junction, bidirectional turbulence injection,
// obstruction transients, per-section amplitude meters), reimplemented from
// the KL model / Julius Smith's PASP with our own changes:
//   - explicit CV parameters (no negative-diameter UI overloading)
//   - per-branch damping (PT left the nasal branch undamped)
//   - variable section count from VTL (species scaling)
//   - external audio injection at arbitrary position (INJECT)
//   - NaN guards on all recursive paths

#pragma once
#include <cmath>
#include <cstring>
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace cyberline {

// Hard limits
static constexpr int TRACT_SECTIONS_MAX = 192;   // ~65 cm at 48k/2x
static constexpr int NOSE_SECTIONS_MAX = 64;
static constexpr int TRACT_TRANSIENTS_MAX = 20;

struct CyberlineTransient {
	int position = 0;
	float timeAlive = 0.f;
	float lifeTime = 0.2f;
	float strength = 0.3f;
	float exponent = 200.f;
	bool living = false;
};

struct CyberlineTract {
	float sampleRate = 44100.f;
	bool initialized = false;
	bool transientsEnabled = true;

	// Section counts (derived from VTL)
	int n = 44;              // oral tract sections
	int noseStart = 32;      // index where nasal branch joins
	int noseLength = 28;

	// Geometry (in normalized section units)
	static constexpr float bladeStartF = 10.f / 44.f;
	static constexpr float tipStartF = 32.f / 44.f;
	static constexpr float lipStartF = 39.f / 44.f;
	int bladeStart = 10;
	int tipStart = 32;
	int lipStart = 39;

	// Termination / loss parameters
	float glottalReflection = 0.75f;
	float lipReflection = -0.85f;
	float wallDamping = 0.999f;      // per-step loss, both branches

	// Articulation state (targets, smoothed by muscleSpeed)
	float muscleSpeed = 15.f;        // cm/s articulator inertia
	float tongueIndex = 19.f;        // blade..tip
	float tongueDiameter = 3.5f;     // cm-ish normalized
	float velum = 0.01f;             // nasal branch opening 0..1
	float constrPos = -1.f;          // <0 = no constriction
	float constrWidth = 3.f;
	float fricativeAmount = 0.f;

	// Traveling-wave state
	float R[TRACT_SECTIONS_MAX];
	float L[TRACT_SECTIONS_MAX];
	float Rn[NOSE_SECTIONS_MAX];
	float Ln[NOSE_SECTIONS_MAX];

	// Coefficients
	float reflection[TRACT_SECTIONS_MAX + 1];
	float newReflection[TRACT_SECTIONS_MAX + 1];
	float reflectionLeft = 0.f, reflectionRight = 0.f, reflectionNose = 0.f;
	float newReflectionLeft = 0.f, newReflectionRight = 0.f, newReflectionNose = 0.f;
	float noseReflection[NOSE_SECTIONS_MAX + 1];

	// Junction outputs (scratch)
	float jr[TRACT_SECTIONS_MAX + 1];
	float jl[TRACT_SECTIONS_MAX + 1];
	float jrn[NOSE_SECTIONS_MAX + 1];
	float jln[NOSE_SECTIONS_MAX + 1];

	// Meters (for panel + SHAPE output)
	float amplitude[TRACT_SECTIONS_MAX];
	float noseAmplitude[NOSE_SECTIONS_MAX];
	int meterCounter = 0;
	static constexpr int METER_INTERVAL = 32;

	// Transient pool
	CyberlineTransient transients[TRACT_TRANSIENTS_MAX];
	int transientCount = 0;
	int lastObstruction = -1;

	// Diameter buffers
	float diameter[TRACT_SECTIONS_MAX];
	float restDiameter[TRACT_SECTIONS_MAX];
	float targetDiameter[TRACT_SECTIONS_MAX];
	float noseDiameter[NOSE_SECTIONS_MAX];
	float noseTargetDiameter[NOSE_SECTIONS_MAX];

	CyberlineTract() { reset(); }

	void reset() {
		std::memset(R, 0, sizeof(R));
		std::memset(L, 0, sizeof(L));
		std::memset(Rn, 0, sizeof(Rn));
		std::memset(Ln, 0, sizeof(Ln));
		std::memset(reflection, 0, sizeof(reflection));
		std::memset(newReflection, 0, sizeof(newReflection));
		std::memset(noseReflection, 0, sizeof(noseReflection));
		std::memset(jr, 0, sizeof(jr));
		std::memset(jl, 0, sizeof(jl));
		std::memset(jrn, 0, sizeof(jrn));
		std::memset(jln, 0, sizeof(jln));
		std::memset(amplitude, 0, sizeof(amplitude));
		std::memset(noseAmplitude, 0, sizeof(noseAmplitude));
		reflectionLeft = reflectionRight = reflectionNose = 0.f;
		newReflectionLeft = newReflectionRight = newReflectionNose = 0.f;
		for (int i = 0; i < TRACT_TRANSIENTS_MAX; i++) transients[i].living = false;
		transientCount = 0;
		lastObstruction = -1;
		lastNoseOutput = 0.f;
		initialized = false;
		setTractLength(44);
		setTongue(tongueIndex, tongueDiameter);
		syncReflections();
	}

	// VTL in cm -> section count: n = VTL * 2 * SR / c. Call on sample-rate
	// or VTL change. Preserves per-section smoothed state when section count
	// changes so VTL sweeps do not click.
	void setGeometry(float vtlCm, float sr) {
		if (sr >= 1000.f) sampleRate = sr;
		float twoX = 2.f * sampleRate;
		float c = 35000.f;                 // cm/s
		int sections = (int) std::round(vtlCm * twoX / c);
		setTractLength(sections);
	}

	void setTractLength(int sections) {
		if (sections < 8) sections = 8;
		if (sections > 176) sections = 176;
		if (initialized && sections == n) return;

		int oldN = n;
		int oldNoseLen = noseLength;
		float oldDiam[TRACT_SECTIONS_MAX];
		if (initialized) {
			std::memcpy(oldDiam, diameter, sizeof(float) * oldN);
		}

		n = sections;
		bladeStart = (int) std::floor(10.f * n / 44.f);
		tipStart = (int) std::floor(32.f * n / 44.f);
		lipStart = (int) std::floor(39.f * n / 44.f);
		noseStart = (int) std::floor(32.f * n / 44.f);
		noseLength = (int) std::floor(28.f * n / 44.f);
		if (noseLength < 4) noseLength = 4;
		if (noseLength > NOSE_SECTIONS_MAX - 1) noseLength = NOSE_SECTIONS_MAX - 1;
		if (noseStart + noseLength > n) noseStart = n - noseLength;
		if (noseStart < bladeStart) noseStart = bladeStart;

		// default profile: pharynx narrow, mouth wide (PT rest shape)
		for (int i = 0; i < n; i++) {
			float d;
			if (i < bladeStart) d = 0.6f;
			else if (i < (int) std::floor(12.f * n / 44.f)) d = 1.1f;
			else d = 1.5f;
			restDiameter[i] = targetDiameter[i] = d;
			if (initialized && oldN > 1) {
				// Resample previous smoothed diameter to preserve state
				float src = (float) i * (float) (oldN - 1) / (float) (n - 1);
				int s0 = (int) std::floor(src);
				int s1 = (s0 + 1 < oldN) ? s0 + 1 : s0;
				float frac = src - (float) s0;
				diameter[i] = oldDiam[s0] * (1.f - frac) + oldDiam[s1] * frac;
			} else {
				diameter[i] = d;
			}
		}
		if (initialized) {
			for (int i = oldN; i < n; i++) {
				R[i] = L[i] = amplitude[i] = 0.f;
			}
			for (int i = oldNoseLen; i < noseLength; i++) {
				Rn[i] = Ln[i] = noseAmplitude[i] = 0.f;
			}
			tongueIndex = tongueIndex * (float) n / (float) oldN;
		}
		for (int i = 0; i < noseLength; i++) {
			float d = 2.f * ((float) i / (float) noseLength);
			float dn = (d < 1.f) ? 0.4f + 1.6f * d : 0.5f + 1.5f * (2.f - d);
			if (dn > 1.9f) dn = 1.9f;
			noseDiameter[i] = noseTargetDiameter[i] = dn;
		}
		noseDiameter[0] = velum;
		if (initialized) {
			setTongue(tongueIndex, tongueDiameter);
		}
		calculateNoseReflections();
		calculateReflections();
		syncReflections();
		initialized = true;
	}

	void setTongue(float index, float diam) {
		tongueIndex = index;
		tongueDiameter = diam;
		int pharynxEnd = (int) std::floor(12.f * n / 44.f);
		for (int i = 0; i < n; i++) {
			float d;
			if (i < bladeStart) d = 0.6f;
			else if (i < pharynxEnd) d = 1.1f;
			else d = 1.5f;
			restDiameter[i] = d;
		}
		for (int i = bladeStart; i < lipStart; i++) {
			float t = 1.1f * (float) M_PI * (float) (tongueIndex - i)
				/ (float) (tipStart - bladeStart);
			float fixed = 2.f + (tongueDiameter - 2.f) / 1.5f;
			float curve = (1.5f - fixed + 1.7f) * std::cos(t);
			if (i == bladeStart - 2 || i == lipStart - 1) curve *= 0.8f;
			if (i == bladeStart || i == lipStart - 2) curve *= 0.94f;
			float d = 1.5f - curve;
			if (d < 0.f) d = 0.f;           // 0 = full occlusion (stop consonant)
			if (d > 5.f) d = 5.f;
			restDiameter[i] = d;
		}
		for (int i = 0; i < n; i++) targetDiameter[i] = restDiameter[i];
	}

	void setVelum(float v) {
		if (v < 0.f) v = 0.f;
		if (v > 1.f) v = 1.f;
		velum = v;
	}

	void setConstriction(float pos, float width, float amount) {
		constrPos = pos;
		constrWidth = width;
		fricativeAmount = amount;
	}

	// One oversample step (call 2x per audio sample with lambda 0.5 then 1.0).
	// source: excitation at glottal end. inject: external source sample.
	// injectPos: section index (fractional) for inject. noise: white noise.
	// Returns lip output.
	float step(float source, float inject, float injectPos, float noise,
		float lambda) {
		// transients
		processTransients();

		// turbulence at constriction
		if (constrPos >= 2.f && constrPos < (float) n && fricativeAmount > 0.f) {
			float d = constrictionDiameterAt(constrPos);
			float thinness = std::fmax(0.f, std::fmin(8.f * (0.7f - d), 1.f));
			float openness = std::fmax(0.f, std::fmin(30.f * (d - 0.3f), 1.f));
			float amt = 0.66f * noise * fricativeAmount * thinness * openness;
			addTurbulence(amt, constrPos);
		}

		// external injection at fractional position — added to the state
		// BEFORE the scattering loop so it enters the junction computation
		// (added after, the mouth update overwrites R/L and it is lost)
		if (inject != 0.f) {
			int ip = (int) std::floor(injectPos);
			if (ip < 0) ip = 0;
			if (ip > n - 2) ip = n - 2;
			float frac = injectPos - (float) ip;
			R[ip] += inject * (1.f - frac) * 0.5f;
			L[ip] += inject * (1.f - frac) * 0.5f;
			R[ip + 1] += inject * frac * 0.5f;
			L[ip + 1] += inject * frac * 0.5f;
		}

		// glottal-end junction (source + external inject at glottal end
		// handled by caller feeding `source`)
		jr[0] = L[0] * glottalReflection + source;
		jl[n] = R[n - 1] * lipReflection;

		// scattering
		for (int i = 1; i < n; i++) {
			float r = reflection[i] * (1.f - lambda) + newReflection[i] * lambda;
			float w = r * (R[i - 1] + L[i]);
			jr[i] = R[i - 1] - w;
			jl[i] = L[i] + w;
		}

		// three-way junction with nose (crossfaded old -> new)
		int ns = noseStart;
		float r = reflectionLeft * (1.f - lambda) + newReflectionLeft * lambda;
		jl[ns] = r * R[ns - 1] + (1.f + r) * (Ln[0] + L[ns]);
		r = reflectionRight * (1.f - lambda) + newReflectionRight * lambda;
		jr[ns] = r * L[ns] + (1.f + r) * (R[ns - 1] + Ln[0]);
		r = reflectionNose * (1.f - lambda) + newReflectionNose * lambda;
		jrn[0] = r * Ln[0] + (1.f + r) * (L[ns] + R[ns - 1]);

		// mouth update with damping
		for (int i = 0; i < n; i++) {
			R[i] = jr[i] * wallDamping;
			L[i] = jl[i + 1] * wallDamping;
		}

		// nose scattering
		jln[noseLength] = Rn[noseLength - 1] * lipReflection;
		for (int i = 1; i < noseLength; i++) {
			float w = noseReflection[i] * (Rn[i - 1] + Ln[i]);
			jrn[i] = Rn[i - 1] - w;
			jln[i] = Ln[i] + w;
		}
		for (int i = 0; i < noseLength; i++) {
			Rn[i] = jrn[i] * wallDamping;
			Ln[i] = jln[i + 1] * wallDamping;
		}

		// meters (deterministic cadence)
		if (++meterCounter >= METER_INTERVAL) {
			meterCounter = 0;
			for (int i = 0; i < n; i++) {
				float a = std::fabs(R[i] + L[i]);
				if (a > amplitude[i]) amplitude[i] = a;
				else amplitude[i] *= 0.995f;
			}
			for (int i = 0; i < noseLength; i++) {
				float a = std::fabs(Rn[i] + Ln[i]);
				if (a > noseAmplitude[i]) noseAmplitude[i] = a;
				else noseAmplitude[i] *= 0.995f;
			}
		}

		float lipOut = R[n - 1];
		float noseOut = Rn[noseLength - 1];
		// caller reads noseOut via lastNoseOutput
		lastNoseOutput = noseOut;

		// NaN guard
		if (!std::isfinite(lipOut) || !std::isfinite(noseOut)) {
			reset();
			return 0.f;
		}
		return lipOut;
	}

	float lastNoseOutput = 0.f;

	// Shape update (call once per audio sample before the 2x step pair, or per
	// block). deltaTime in seconds. Moves diameters toward targets (muscle speed).
	void reshape(float deltaTime) {
		float amount = deltaTime * muscleSpeed * 2.f; // section-units per s
		int newLastObstruction = -1;
		for (int i = 0; i < n; i++) {
			float d = diameter[i];
			float td = targetDiameter[i];
			if (d <= 0.05f) newLastObstruction = i;
			float slowReturn;
			if (i < noseStart) slowReturn = 0.6f;
			else if (i >= tipStart) slowReturn = 1.f;
			else slowReturn = 0.6f + 0.4f * (float) (i - noseStart)
				/ (float) (tipStart - noseStart);
			float step = slowReturn * amount;
			float stepDown = 2.f * amount;
			if (d < td) d = std::fmin(d + step, td);
			else d = std::fmax(d - stepDown, td);
			diameter[i] = d;
		}
		if (transientsEnabled && lastObstruction > -1 && newLastObstruction == -1
			&& noseArea(0) < 0.05f) {
			addTransient(lastObstruction);
		}
		lastObstruction = newLastObstruction;

		// velum moves toward target
		float vstep = amount * 0.25f;
		float vdown = amount * 0.1f;
		float v = noseDiameter[0];
		if (v < velum) v = std::fmin(v + vstep, velum);
		else v = std::fmax(v - vdown, velum);
		noseDiameter[0] = v;
	}

	// Apply constriction to target diameters (block-rate, from CV state).
	void applyConstriction() {
		setTongue(tongueIndex, tongueDiameter);
		if (constrPos < 2.f || constrPos >= (float) n) return;
		float diam = constrictionWidthTarget();
		if (diam >= 3.f) return;
		long width = 2;
		if (constrPos < bladeStart + 4) width = 10;
		else if (constrPos >= (float) tipStart) width = 5;
		else width = (long) (10.f - 5.f * (constrPos - (float) bladeStart)
			/ (float) (tipStart - bladeStart));
		long idx = (long) std::floor(constrPos + 0.5f);
		for (long i = -width - 1; i < width + 1; i++) {
			long j = idx + i;
			if (j < 0 || j >= n) continue;
			float relpos = std::fabs((float) ((int) constrPos + (int) i)
				- constrPos) - 0.5f;
			float shrink;
			if (relpos <= 0.f) shrink = 0.f;
			else if (relpos > (float) width) shrink = 1.f;
			else shrink = 0.5f * (1.f - std::cos((float) M_PI * relpos / (float) width));
			if (diam < targetDiameter[j]) {
				targetDiameter[j] = diam
					+ (targetDiameter[j] - diam) * shrink;
			}
		}
	}

	// Coefficient refresh (call once per audio sample after reshape).
	void calculateReflectionsPublic() { calculateReflections(); }

private:
	float area(int i) {
		float d = diameter[i];
		return d * d;
	}
	float noseArea(int i) {
		float d = noseDiameter[i];
		return d * d;
	}

	float constrictionDiameterAt(float pos) {
		int i = (int) std::floor(pos);
		if (i < 0) i = 0;
		if (i >= n) i = n - 1;
		return diameter[i];
	}

	float constrictionWidthTarget() {
		// explicit parameter: constriction target diameter from width knob
		// (width 0..10 -> target diameter 0..3 cm-ish)
		float w = constrWidth;
		if (w < 0.f) w = 0.f;
		if (w > 10.f) w = 10.f;
		return w * 0.3f;
	}

	void syncReflections() {
		for (int i = 1; i < n; i++) reflection[i] = newReflection[i];
		reflectionLeft = newReflectionLeft;
		reflectionRight = newReflectionRight;
		reflectionNose = newReflectionNose;
	}

	void calculateReflections() {
		for (int i = 1; i < n; i++) {
			reflection[i] = newReflection[i];
			float a1 = area(i - 1), a2 = area(i);
			if (a1 + a2 == 0.f) newReflection[i] = 0.f;
			else if (a2 == 0.f) newReflection[i] = 0.999f;
			else if (a1 == 0.f) newReflection[i] = -0.999f;
			else newReflection[i] = (a1 - a2) / (a1 + a2);
		}
		reflectionLeft = newReflectionLeft;
		reflectionRight = newReflectionRight;
		reflectionNose = newReflectionNose;

		int ns = noseStart;
		float sum = area(ns) + area(ns + 1) + noseArea(0);
		if (sum > 0.f) {
			newReflectionLeft = (2.f * area(ns) - sum) / sum;
			newReflectionRight = (2.f * area(ns + 1) - sum) / sum;
			newReflectionNose = (2.f * noseArea(0) - sum) / sum;
		} else {
			newReflectionLeft = newReflectionRight = newReflectionNose = 0.f;
		}

		// Update first nasal junction reflection as velum (noseDiameter[0]) moves
		if (noseLength > 1) {
			float na0 = noseArea(0), na1 = noseArea(1);
			if (na0 + na1 > 0.f)
				noseReflection[1] = (na0 - na1) / (na0 + na1);
		}
	}

	void calculateNoseReflections() {
		for (int i = 1; i < noseLength; i++) {
			float a1 = noseArea(i - 1), a2 = noseArea(i);
			if (a1 + a2 == 0.f) noseReflection[i] = 0.f;
			else noseReflection[i] = (a1 - a2) / (a1 + a2);
		}
	}

	void addTurbulence(float noise, float index) {
		if (index < 0.f || index >= (float) n) return;
		int i = (int) std::floor(index);
		float delta = index - (float) i;
		if (i + 1 < n) {
			R[i + 1] += noise * (1.f - delta) * 0.5f;
			L[i + 1] += noise * (1.f - delta) * 0.5f;
		}
		if (i + 2 < n) {
			R[i + 2] += noise * delta * 0.5f;
			L[i + 2] += noise * delta * 0.5f;
		}
	}

	void addTransient(int position) {
		CyberlineTransient* t = nullptr;
		for (int i = 0; i < TRACT_TRANSIENTS_MAX; i++) {
			if (!transients[i].living) { t = &transients[i]; break; }
		}
		if (!t) return;
		t->position = position;
		t->timeAlive = 0.f;
		t->lifeTime = 0.2f;
		t->strength = 0.3f;
		t->exponent = 200.f;
		t->living = true;
		transientCount++;
	}

	void processTransients() {
		if (transientCount <= 0) return;
		float dt = 0.5f / sampleRate;
		for (int i = 0; i < TRACT_TRANSIENTS_MAX; i++) {
			CyberlineTransient* t = &transients[i];
			if (!t->living) continue;
			float amp = t->strength
				* std::exp(-0.69314718f * t->exponent * t->timeAlive);
			int pos = t->position;
			if (pos >= 0 && pos < n) {
				R[pos] += amp * 0.5f;
				L[pos] += amp * 0.5f;
			}
			t->timeAlive += dt;
			if (t->timeAlive >= t->lifeTime) {
				t->living = false;
				transientCount--;
			}
		}
	}
};

} // namespace cyberline
