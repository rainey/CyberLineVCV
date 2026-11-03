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
		bool muteInhale = params[INHMUTE_PARAM].getValue() >= 0.5f;

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
			} else {
				float t = 1.f - (p - 0.5f) / 0.5f; // 1 start, 0 end
				float k = 0.4f + 2.6f * (1.f - effort);
				float e = (std::exp(-k * (1.f - t)) - std::exp(-k))
					/ (1.f - std::exp(-k));
				volume = depth * ((1.f - effort) * e + effort * t);
			}
			if (!std::isfinite(volume)) volume = 0.f;
			flow = rack::math::clamp(
				(volume - lastPhasorVolume) / std::fmax(dt, 1e-6f),
				-1.f, 1.f);
			lastPhasorVolume = volume;
			inhaling = (flow > 0.002f);
			exhaling = (flow < -0.002f);
			phase = exhaling ? 1.f : 0.f;
		} else if (mode == CLOSED) {
			// CLOSED mode (valved air rationing / mid-breath hold):
			// - TRIG rising edge catches a breath (inhales to depth and holds at top).
			// - When GATE is patched, GATE acts as the airway/glottal release valve:
			//   GATE high = exhale from remaining volume; GATE low = close airway and
			//   hold remaining breath mid-lung (VOL flat, FLOW = 0, PRESS = 0).
			// - When GATE is unpatched, TRIG inhales to depth when empty, and
			//   subsequent TRIGs release stepped ~35% phrase bursts with mid-breath
			//   holds in between until empty.
			bool gateConnected = inputs[GATE_INPUT].isConnected();
			bool trigConnected = inputs[TRIGGER_INPUT].isConnected();

			if (gateConnected) {
				bool startInhale = trigRising
					|| (!trigConnected && gateRising && volume <= 0.01f);
				if (startInhale) {
					inhaling = true;
					exhaling = false;
					phase = 0.f;
				}
				if (inhaling) {
					float it = std::fmax(inhaleTime, 0.05f);
					float f = std::fmax((depth - volume) / it, 0.08f);
					flow = f * (0.3f + 0.7f * maxFlow);
					volume += flow * dt;
					phase = 0.f;
					if (volume >= depth) {
						volume = depth;
						inhaling = false;
						// Top hold unless GATE valve is currently held open
						if (gate && (!trigRising || !trigConnected)) {
							exhaling = true;
						} else {
							exhaling = false;
							flow = 0.f;
						}
					}
				} else {
					// Not inhaling: GATE controls the expiratory valve
					if (gate && volume > 0.001f) {
						exhaling = true;
					} else {
						// Top, mid, or bottom breath-hold
						exhaling = false;
						flow = 0.f;
						phase = 0.f;
					}
				}
			} else {
				// Unpatched GATE fallback: TRIG refills when empty, or releases
				// a rationed ~35% phrase burst and holds mid-breath between triggers.
				if (trigRising) {
					if (volume <= 0.02f) {
						inhaling = true;
						exhaling = false;
						phase = 0.f;
					} else {
						inhaling = false;
						exhaling = true;
						closedBurstTarget = std::fmax(0.f, volume - 0.35f * depth);
					}
				}
				if (inhaling) {
					float it = std::fmax(inhaleTime, 0.05f);
					float f = std::fmax((depth - volume) / it, 0.08f);
					flow = f * (0.3f + 0.7f * maxFlow);
					volume += flow * dt;
					phase = 0.f;
					if (volume >= depth) {
						volume = depth;
						inhaling = false;
						exhaling = false;
						flow = 0.f; // Hold at top until next trigger releases a burst
					}
				}
			}

			if (exhaling) {
				float recoil = volume;
				float drive = (1.f - effort) * recoil + effort * depth;
				float k = 0.4f + 2.6f * (1.f - effort);
				volume -= drive * k * dt;
				if (volume < 0.f) volume = 0.f;
				flow = -drive;
				phase = 1.f;
				if (!gateConnected && volume <= closedBurstTarget) {
					// End of rationed phrase burst -> mid-breath hold
					flow = 0.f;
					phase = 0.f;
					exhaling = false;
				}
				if (volume <= 0.001f) {
					volume = 0.f;
					flow = 0.f;
					phase = 0.f;
					exhaling = false;
					inhaling = false;
				}
			}
		} else {
			// CYCLE and TRIG modes:
			// Start a breath cycle (CYCLE auto, TRIG on trigger, or GATE rising edge)
			bool start = false;
			if (mode == CYCLE) {
				float rate = params[RATE_PARAM].getValue();
				cycleTimer += dt;
				if (!inhaling && !exhaling && cycleTimer >= 60.f / rate
						&& volume < depth * 0.999f) {
					start = true;
				}
				inhaleTime = std::fmin(inhaleTime, 60.f / rate * 0.5f);
			} else if (trigRising) {
				start = true;
			}
			if (gateRising) start = true;

			if (start) {
				inhaling = true;
				exhaling = false;
				float it = std::fmax(inhaleTime, 0.05f);
				float f = std::fmax((depth - volume) / it, 0.05f);
				flow = f * (0.3f + 0.7f * maxFlow);
				if (flow < 0.01f) flow = 0.01f;
				phase = 0.f;
				cycleTimer = 0.f;
			}

			// Gate pedal: held high = inhale toward depth, then HOLD the breath
			// at the top (flow = 0, press = 0); also cuts an in-progress exhale.
			if (gate) {
				exhaling = false;
				phase = 0.f;
				if (volume < depth) {
					inhaling = true;
					float f = std::fmax((depth - volume) / std::fmax(inhaleTime, 0.05f), 0.05f);
					flow = f * (0.3f + 0.7f * maxFlow);
				} else {
					volume = depth;
					inhaling = false;
					flow = 0.f;
				}
			}

			// Gate release: let the held breath go
			if (gateFalling && !start && !exhaling && volume > 0.001f) {
				inhaling = false;
				exhaling = true;
			}

			// Integrate volume
			if (inhaling) {
				volume += flow * dt;
				if (volume >= depth) {
					volume = depth;
					inhaling = false;
					if (!gate) {
						exhaling = true;
					} else {
						// Top Breath-Hold: lungs full, gate held high -> zero flow immediately
						exhaling = false;
						flow = 0.f;
					}
				}
			} else if (exhaling) {
				float recoil = volume;
				float drive = (1.f - effort) * recoil + effort * depth;
				float k = 0.4f + 2.6f * (1.f - effort);
				volume -= drive * k * dt;
				if (volume < 0.f) volume = 0.f;
				flow = -drive;
				phase = 1.f;
				if (volume <= 0.001f) {
					volume = 0.f;
					flow = 0.f;
					phase = 0.f;
					exhaling = false;
					inhaling = false;
				}
			} else {
				// Top Hold or Bottom Rest (!inhaling && !exhaling)
				flow = 0.f;
				phase = 0.f;
			}
		}

		// Subglottal pressure output:
		// - Exhaling: positive outward phonation drive (recoil + effort, 0..+10 V)
		// - Inhaling (flow > 0): negative inspiratory suction (-5..0 V) in GASP
		//   mode, or 0 V when Inhale Mute is active
		// - Breath-Hold (Top, Mid, or Bottom, flow == 0): 0 V
		if (exhaling && volume > 0.001f) {
			float recoil = volume;
			float regulated = effort * depth * (volume > 0.02f ? 1.f : volume / 0.02f);
			pressure = rack::math::clamp((1.f - effort) * recoil + regulated, 0.f, 1.f);
		} else if (!exhaling && flow > 0.002f && !muteInhale) {
			pressure = -rack::math::clamp(flow * 0.35f, 0.f, 0.5f);
		} else {
			pressure = 0.f;
		}

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
		rack::math::clamp(outPressure * 10.f, -5.f, 10.f));
	outputs[VOLUME_OUTPUT].setVoltage(
		rack::math::clamp(outVolume * 10.f, 0.f, 10.f));
	outputs[FLOW_OUTPUT].setVoltage(
		rack::math::clamp(outFlow * 5.f, -5.f, 5.f));   // signed, +-5 V
	outputs[PHASE_OUTPUT].setVoltage(outPhase > 0.5f ? 5.f : 0.f);
	lights[PHASE_LIGHT].setSmoothBrightness(outPhase > 0.5f ? 1.f : 0.f,
		args.sampleTime);
}

} // namespace cyberline
