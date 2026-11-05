// CyberlineVisual1 — audio→image visualizer. Spec: docs/spec.md §5e.
//
// Rack wrapper around CyberlineVisual1Core (see that header for the
// audio→DCT mapping). Inputs L/R feed a 2048-sample windowed FFT; the
// band envelopes drive the 2D DCT coefficients of a 256x192 image rendered
// into an RGBA buffer the widget blits as a NanoVG texture.
//
// PERTURB mode multiplies the DCT coefficients of a loaded image
// (bundled demo JPEG or user-picked file) band-by-band by the spectrum.
// Image decode happens once on load (stb_image, widget-side), never in
// the audio path — suite convention: no file I/O at DSP time.

#pragma once
#include <rack.hpp>
#include "CyberlineVisual1Core.hpp"

namespace cyberline {

struct CyberlineVisual1 : rack::engine::Module {
	enum ParamIds {
		GAIN_PARAM,
		BAND_PARAM,
		DECAY_PARAM,
		MODE_PARAM,     // 3-position: MOSAIC / PHASE / PERTURB
		STEREO_PARAM,   // 3-position: SUM / INTLV / SPLIT
		NUM_PARAMS
	};
	enum InputIds {
		L_INPUT,
		R_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		FRAME_OUTPUT,   // trigger pulse each rendered frame
		ACT_OUTPUT,     // CV 0..10 V of band-envelope mean (loudness)
		NUM_OUTPUTS
	};
	enum LightIds {
		ACT_LIGHT,
		NUM_LIGHTS
	};

	CyberlineVisual1Core core;

	// FRAME trigger: a short pulse when a new frame lands (frame cadence
	// ~11.6 ms at 44.1k; 2 ms pulse is safely inside it)
	float framePulse = 0.f;

	CyberlineVisual1() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		configParam(GAIN_PARAM, 0.f, 1.f, 0.5f, "Input gain");
		configParam(BAND_PARAM, 0.f, 1.f, 0.65f, "Spectral spread (zigzag reach)");
		configParam(DECAY_PARAM, 0.f, 1.f, 0.4f, "Band decay");
		configSwitch(MODE_PARAM, 0.f, 2.f, 0.f, "Mode",
			{"Mosaic", "Phase", "Perturb"});
		configSwitch(STEREO_PARAM, 0.f, 2.f, 0.f, "Stereo",
			{"Sum", "Interleave", "Split"});
		configInput(L_INPUT, "Audio L / mono");
		configInput(R_INPUT, "Audio R");
		configOutput(FRAME_OUTPUT, "Frame trigger (each rendered image)");
		configOutput(ACT_OUTPUT, "Activity (mean band envelope, 0-10 V)");
		configLight(ACT_LIGHT, "Activity");
	}

	void process(const ProcessArgs& args) override {
		// Core params are written per sample (cheap scalars) so knob moves
		// apply at the next hop boundary without a control-rate split.
		core.params.gain = params[GAIN_PARAM].getValue();
		core.params.band = params[BAND_PARAM].getValue();
		core.params.decay = params[DECAY_PARAM].getValue();
		core.params.mode = (int) std::round(params[MODE_PARAM].getValue());
		core.params.stereo = (int) std::round(params[STEREO_PARAM].getValue());

		float l = inputs[L_INPUT].getVoltage() * 0.1f;
		float r = inputs[R_INPUT].isConnected()
			? inputs[R_INPUT].getVoltage() * 0.1f
			: l; // unpatched R follows L (mono source renders full image)
		core.processSample(l, r);

		// Outputs
		float act = core.activity;
		if (!std::isfinite(act)) act = 0.f;
		lights[ACT_LIGHT].setBrightnessSmooth(act, args.sampleRate);
		outputs[ACT_OUTPUT].setVoltage(10.f * act);

		// FRAME trigger pulse (~2 ms)
		if (framePulse > 0.f) {
			framePulse -= 1.f / args.sampleRate;
			outputs[FRAME_OUTPUT].setVoltage(5.f);
		} else {
			outputs[FRAME_OUTPUT].setVoltage(0.f);
		}
		int seq = core.frameSeq.load(std::memory_order_relaxed);
		if (seq != frameSeqSeen) {
			frameSeqSeen = seq;
			framePulse = 0.002f;
		}
	}

	void onSampleRateChange(const SampleRateChangeEvent& e) override {
		core.setSampleRate(e.sampleRate);
	}

	void onReset() override {
		core.reset();
		framePulse = 0.f;
		frameSeqSeen = 0;
	}

	int frameSeqSeen = 0;
};

} // namespace cyberline
