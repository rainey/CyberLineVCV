// CyberlineLungs — module process.

#include "CyberlineLungs.hpp"

namespace cyberline {

void CyberlineLungs::process(const ProcessArgs& args) {
	Mode mode = (Mode) std::round(params[MODE_PARAM].getValue());
	if (mode < CYCLE) mode = CYCLE;
	if (mode > CLOSED) mode = CLOSED;

	float depth = rack::math::clamp(
		params[DEPTH_PARAM].getValue()
		+ inputs[DEPTHCV_INPUT].getVoltage() * 0.1f, 0.f, 1.f);
	float effort = params[EFFORT_PARAM].getValue();
	float inhaleTime = params[INHALE_PARAM].getValue();
	float maxFlow = params[MAXFLOW_PARAM].getValue();

	bool gate = inputs[GATE_INPUT].getVoltage() >= 1.f;
	bool gateRising = gate && !gateWasHigh;
	gateWasHigh = gate;

	bool trig = inputs[TRIGGER_INPUT].getVoltage() >= 1.f;
	bool trigRising = trig && !trigWasHigh;
	trigWasHigh = trig;

	// ---- physics at control rate ----
	if (++physCounter >= PHYS_INTERVAL) {
		physCounter = 0;
		float dt = PHYS_INTERVAL / args.sampleRate;

		// Start a breath cycle (CYCLE auto, TRIG/CLOSED on trigger)
		bool start = false;
		if (mode == CYCLE) {
			float rate = params[RATE_PARAM].getValue();
			// free-running breath loop: fill then drain
			if (!exhaling && volume >= depth * 0.999f) exhaling = true;
			if (exhaling && volume <= 0.02f) { exhaling = false; }
			// resync inhale duration roughly to cycle period
			inhaleTime = std::fmin(inhaleTime, 60.f / rate * 0.5f);
		} else if (trigRising) {
			start = true;
		}
		if (gateRising) start = true;   // gate edge also starts a cycle

		if (start) {
			// catch-breath: refill target from current volume
			exhaling = false;
			float fill = depth;
			if (mode == CLOSED) {
				// whale mode: air retained; a new "trigger" can only
				// inhale what the reservoir is missing
				fill = depth;
			}
			(void)fill; // inhale integrates toward depth below
			float it = std::fmax(inhaleTime, 0.05f);
			// inhale flow magnitude: reach depth within it, scaled by maxFlow
			float f = (depth - volume) / it;
			flow = fmaxf(f, 0.f) * (0.3f + 0.7f * maxFlow);
			if (flow < 0.01f) flow = 0.01f;
			phase = 0.f;
		}

		// Gate held high: inhale (all modes)
		if (gate && !exhaling) {
			float f = (depth - volume) / std::fmax(inhaleTime, 0.05f);
			flow = fmaxf(f, 0.f) * (0.3f + 0.7f * maxFlow);
			phase = 0.f;
		}

		// Integrate volume
		if (!exhaling) {
			volume += flow * dt;
			if (volume >= depth) {
				volume = depth;
				if (mode == CYCLE || trigWasHigh || gate) {
					// switch to exhale
					exhaling = true;
				}
			}
		} else {
			// Exhale: flow driven by recoil + effort regulation
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
				flow = 0.f;
				if (mode == CYCLE) { exhaling = false; phase = 0.f; }
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
	float a = 0.4f; // smoothing toward control-rate targets
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
