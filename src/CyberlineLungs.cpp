// CyberlineLungs — module process.

#include "CyberlineLungs.hpp"

namespace cyberline {

void CyberlineLungs::process(const ProcessArgs& args) {
	// Per-sample trigger/gate edge detection (short pulses must not fall
	// between 32-sample physics ticks)
	bool gate = inputs[GATE_INPUT].getVoltage() >= 1.f;
	if (gate && !gateWasHigh) gateRisePending = true;
	if (!gate && gateWasHigh) gateFallPending = true;
	gateWasHigh = gate;

	bool trig = inputs[TRIGGER_INPUT].getVoltage() >= 1.f;
	if (trig && !trigWasHigh) trigPending = true;
	trigWasHigh = trig;

	// ---- physics at control rate ----
	if (++physCounter >= PHYS_INTERVAL) {
		physCounter = 0;
		float dt = PHYS_INTERVAL / args.sampleRate;

		bool trigRising = trigPending;
		bool gateRising = gateRisePending;
		bool gateFalling = gateFallPending;
		trigPending = false;
		gateRisePending = false;
		gateFallPending = false;

		Mode mode = (Mode) std::round(params[MODE_PARAM].getValue());
		if (mode < CYCLE) mode = CYCLE;
		if (mode > PHASOR) mode = PHASOR;

		float depth = rack::math::clamp(
			params[DEPTH_PARAM].getValue()
			+ inputs[DEPTHCV_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
		float effort = params[EFFORT_PARAM].getValue();
		float inhaleTime = params[INHALE_PARAM].getValue();
		float maxFlow = params[MAXFLOW_PARAM].getValue();

			if (mode == PHASOR) {
				// Phase-driven: the GATE input is the phase CV
				// (0-10 V -> 0-1 of the breath cycle). 0-50% inhale,
				// 50-100% exhale; EFFORT shapes the drain curve, while
				// INHALE/MAX FLOW are ignored (the external phasor owns
				// the timing). FLOW reports the actual dV/dt, so its
				// magnitude follows the phasor speed — including jumps.
				float p = rack::math::clamp(
					inputs[GATE_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
				if (p <= 0.5f) {
					volume = depth * (p / 0.5f);
					exhaling = false;
					phase = 0.f;
				} else {
					float t = 1.f - (p - 0.5f) / 0.5f; // 1 start, 0 end
					float k = 0.4f + 2.6f * (1.f - effort);
					float e = (std::exp(-k * (1.f - t)) - std::exp(-k))
						/ (1.f - std::exp(-k));
					volume = depth * ((1.f - effort) * e + effort * t);
					exhaling = true;
					phase = 1.f;
				}
				if (!std::isfinite(volume)) volume = 0.f;
				flow = rack::math::clamp(
					(volume - lastPhasorVolume) / std::fmax(dt, 1e-6f),
					-1.f, 1.f);
				lastPhasorVolume = volume;
			} else {
			// Start a breath cycle (CYCLE auto, TRIG/CLOSED on trigger)
			bool start = false;
			if (mode == CYCLE) {
				float rate = params[RATE_PARAM].getValue();
				// Free-running breath loop. RATE sets the breath period
				// (60/rate) as a *floor*: after a breath completes, the lungs
				// rest until the period elapses, then top up and exhale. When
				// the natural physics (inhale + drain law) is slower than the
				// period, the natural duration wins — the drain is never
				// accelerated to hit RATE.
				cycleTimer += dt;
				if (!exhaling && cycleTimer >= 60.f / rate
						&& volume < depth * 0.999f) {
					start = true;
				}
				// resync inhale duration roughly to cycle period
				inhaleTime = std::fmin(inhaleTime, 60.f / rate * 0.5f);
			} else if (trigRising) {
				start = true;
			}
			if (gateRising) start = true;   // gate edge also starts a cycle

			if (start) {
				// catch-breath: refill from current volume
				exhaling = false;
				float it = std::fmax(inhaleTime, 0.05f);
				// inhale flow magnitude: reach depth within it, scaled by maxFlow
				float f = (depth - volume) / it;
				flow = fmaxf(f, 0.f) * (0.3f + 0.7f * maxFlow);
				if (flow < 0.01f) flow = 0.01f;
				phase = 0.f;
				cycleTimer = 0.f;   // breath period measured from inhale start
			}

			// Gate pedal (all modes): held high = inhale toward depth, then
			// HOLD the breath in; also cuts an in-progress exhale
			// (catch-breath). Exhale happens on release (below) or when a
			// one-shot attack completes with the gate low.
			if (gate) {
				exhaling = false;
				float f = (depth - volume) / std::fmax(inhaleTime, 0.05f);
				flow = fmaxf(f, 0.f) * (0.3f + 0.7f * maxFlow);
				phase = 0.f;
			}

			// Gate release: let the held breath go
			if (gateFalling && !start && !exhaling && volume > 0.001f)
				exhaling = true;

			// Integrate volume
			if (!exhaling) {
				volume += flow * dt;
				if (volume >= depth) {
					volume = depth;
					// One-shot attack complete: exhale begins automatically
					// unless the gate pedal holds the breath in. (Previously
					// this required trig/gate to STILL be high here, which
					// stranded any breath whose trigger ended during the
					// ~0.65 s attack at full volume — the "pressure stuck
					// high, never oscillates" bug.)
					if (!gate) exhaling = true;
				}
			} else {
				// Exhale: flow driven by recoil + effort regulation.
				// NOTE: the state floor is 0 (no explicit residual-volume
				// constant — see spec §5b DSP notes). The physiological
				// end-of-phrase exponential taper comes from drive ∝ volume:
				// at EFFORT 0 the drain is exponential (drive shrinks with the
				// remaining air), at EFFORT 1 it is a flattened plateau until
				// the reservoir empties.
				float recoil = volume;                 // pressure available
				float drive = (1.f - effort) * recoil  // unregulated decay
					+ effort * depth;                  // flattened plateau
				// exponential drain toward residual
				float k = 0.4f + 2.6f * (1.f - effort);
				volume -= drive * k * dt;
				if (volume < 0.f) volume = 0.f;
				flow = -drive;
				phase = 1.f;
				if (volume <= 0.001f) {
					// breath over: system at rest in ALL modes — flow 0, phase
					// gate low, exhaling cleared. Previously this only cleaned
					// up in CYCLE, leaving FLOW stuck at -drive and PHASE stuck
					// high in TRIG/CLOSED until the next trigger.
					volume = 0.f;
					flow = 0.f;
					phase = 0.f;
					exhaling = false;
				}
			}
			}


		// Pressure output from volume: recoil + regulated component
		float recoil = volume;
		float regulated = effort * depth * (volume > 0.02f ? 1.f : volume / 0.02f);
		pressure = (1.f - effort) * recoil + regulated;
		if (pressure < 0.f) pressure = 0.f;
		if (pressure > 1.f) pressure = 1.f;

		// Output targets
		tgtPressure = pressure;
		tgtVolume = volume;
		tgtFlow = rack::math::clamp(flow * 5.f, -5.f, 5.f) / 5.f;
		tgtPhase = phase;
	}

	// ---- per-sample output interpolation ----
	float a = 0.08f; // smoothing across the 32-sample control block
	outPressure += (tgtPressure - outPressure) * a;
	outVolume   += (tgtVolume   - outVolume)   * a;
	outFlow     += (tgtFlow     - outFlow)     * a;
	outPhase    += (tgtPhase    - outPhase)    * a;

	if (!std::isfinite(outPressure)) reset();

	outputs[PRESSURE_OUTPUT].setVoltage(
		rack::math::clamp(outPressure * 10.f, 0.f, 10.f));
	outputs[VOLUME_OUTPUT].setVoltage(
		rack::math::clamp(outVolume * 10.f, 0.f, 10.f));
	outputs[FLOW_OUTPUT].setVoltage(
		rack::math::clamp(outFlow * 5.f, -5.f, 5.f));   // signed, +-5 V
	outputs[PHASE_OUTPUT].setVoltage(outPhase > 0.5f ? 5.f : 0.f);
	lights[PHASE_LIGHT].setSmoothBrightness(outPhase > 0.5f ? 1.f : 0.f,
		args.sampleTime);
}

} // namespace cyberline
