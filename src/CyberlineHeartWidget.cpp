// CyberlineHeartWidget — programmatic panel (no SVG assets). 22 HP,
// four columns: anatomy knobs | drive knobs + phase lights | in/out | out.
// Heart gauge top-center: atrium over ventricle, plus the aortic reservoir,
// fill-coded and phase-colored (like the Lungs BreathGauge).

#include "CyberlineHeart.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineHeartWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 22 * 15.f;

	// Heart gauge: an anatomical two-chambered heart silhouette — atrium
	// tapering into the ventricle, aortic arch rising to the right, great
	// vessels behind. Live state: chambers swell with volume, the muscle
	// "beats" (brief squeeze on contraction), the AV valve flap swings
	// with the inflow valve state, the arch tenses at high pressure.
	// Color = phase (cyan filling / warm ejecting / dim at rest).
	// Drawn from engine floats per spec §2.
	struct HeartGauge : rack::widget::Widget {
		CyberlineHeart* module;
		HeartGauge(CyberlineHeart* m) : module(m) {}

		void draw(const DrawArgs& args) override {
			float av = 0.6f, vv = 0.6f, ar = 0.4f;
			bool filling = false, ejecting = false;
			if (module && std::isfinite(module->core.vvol)) {
				av = rack::math::clamp(module->core.avol, 0.f, 1.f);
				vv = rack::math::clamp(module->core.vvol, 0.f, 1.f);
				ar = rack::math::clamp(module->core.apart, 0.f, 1.2f);
				filling = module->core.inflowOpen;
				ejecting = module->core.outflowOpen;
			}
			float w = box.size.x, h = box.size.y;
			float rest = !filling && !ejecting;
			NVGcolor fill = rest
				? nvgRGB(0x3a, 0x40, 0x48)
				: (ejecting
					? nvgRGB(0xe0, 0x7a, 0x5a)
					: nvgRGB(0x5a, 0xc8, 0xe0));
			NVGcolor outline = nvgRGB(0x4a, 0x54, 0x60);
			// great vessels warm/tense with arterial pressure
			NVGcolor vessel = nvgRGB(
				(uint8_t)(0x6a + 60.f * rack::math::clamp(ar, 0.f, 1.f)),
				0x56, 0x60);

			// contraction beat: brief squeeze while the aortic valve is
			// open, decaying as ejection proceeds (breathing muscle feel)
			float ejectPulse = (ejecting && module)
				? rack::math::clamp(
					1.f - module->core.tV / (0.22f * module->core.period),
					0.f, 1.f)
				: 0.f;
			float cx = w * 0.46f;
			float cy = h * 0.58f;
			float beat = 1.f + 0.05f * ejectPulse;    // muscle squeeze
			float vt = 0.85f + 0.15f * vv;            // ventricle fullness
			float at = 0.85f + 0.15f * av;            // atrium fullness

			nvgSave(args.vg);
			nvgTranslate(args.vg, cx, cy);
			nvgScale(args.vg, beat, beat);
			nvgTranslate(args.vg, -cx, -cy);

			// ---- great vessels (behind the muscle body) ----
			// aortic arch: rises from the top of the atrial mass, curves
			// over right, and continues down as the descending aorta
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, cx + 1.f, cy - 20.f);
			nvgBezierTo(args.vg, cx + 6.f, cy - 27.f, cx + 16.f, cy - 27.f,
				cx + 19.f, cy - 21.f);
			nvgBezierTo(args.vg, cx + 22.f, cy - 16.f, cx + 21.f, cy - 8.f,
				cx + 19.f, cy - 2.f);
			nvgStrokeWidth(args.vg, 4.5f);
			nvgStrokeColor(args.vg, vessel);
			nvgLineCap(args.vg, NVG_ROUND);
			nvgStroke(args.vg);
			// arch branch stubs (brachiocephalic ticks)
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, cx + 8.f, cy - 25.f);
			nvgLineTo(args.vg, cx + 8.f, cy - 30.f);
			nvgMoveTo(args.vg, cx + 13.f, cy - 26.f);
			nvgLineTo(args.vg, cx + 14.f, cy - 31.f);
			nvgStrokeWidth(args.vg, 1.8f);
			nvgStroke(args.vg);
			// pulmonary/venous stub entering the atrium side
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, cx - 15.f, cy - 17.f);
			nvgLineTo(args.vg, cx - 11.f, cy - 13.f);
			nvgStrokeWidth(args.vg, 3.f);
			nvgStroke(args.vg);

			// ---- muscle body: atrium tapering into the ventricle ----
			// One silhouette: rounded atrial mass up top, bulging
			// ventricular cone below, notched AV groove between, apex
			// pointing bottom-left (anatomical tilt).
			nvgBeginPath(args.vg);
			// start at the AV notch, right side
			nvgMoveTo(args.vg, cx + 10.f, cy - 5.f);
			// right ventricle wall, down toward the apex
			nvgBezierTo(args.vg, cx + 16.f * vt, cy + 7.f,
				cx + 10.f * vt, cy + 20.f * vt, cx - 1.f, cy + 26.f * vt);
			// apex tip (bottom-left, pointed)
			nvgBezierTo(args.vg, cx - 8.f, cy + 29.f * vt,
				cx - 16.f, cy + 24.f * vt, cx - 17.f * vt, cy + 14.f);
			// left ventricle wall, back up to the AV groove
			nvgBezierTo(args.vg, cx - 18.f * vt, cy + 3.f,
				cx - 15.f * vt, cy - 4.f, cx - 11.f, cy - 7.f);
			// AV groove notch (deep indent)
			nvgLineTo(args.vg, cx - 8.f, cy - 10.5f);
			// atrial mass, up and over (rounded)
			nvgBezierTo(args.vg, cx - 13.f, cy - 17.f * at,
				cx - 8.f, cy - 23.f * at, cx - 1.f, cy - 23.f * at);
			nvgBezierTo(args.vg, cx + 5.f, cy - 23.f * at,
				cx + 10.f, cy - 19.f * at, cx + 10.f, cy - 11.f);
			// down into the notch (atrial ear)
			nvgBezierTo(args.vg, cx + 10.f, cy - 8.f,
				cx + 10.f, cy - 7.f, cx + 10.f, cy - 5.f);
			nvgClosePath(args.vg);
			nvgStrokeWidth(args.vg, 1.2f);
			nvgStrokeColor(args.vg, outline);
			nvgStroke(args.vg);
			nvgFillColor(args.vg, fill);
			nvgFill(args.vg);

			// interventricular groove line: from the AV notch toward the
			// apex (the "crease" that makes it read as a heart)
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, cx + 4.f, cy - 7.f);
			nvgBezierTo(args.vg, cx + 4.f, cy + 2.f, cx + 1.f, cy + 10.f,
				cx - 4.f, cy + 17.f);
			nvgStrokeWidth(args.vg, 1.f);
			nvgStrokeColor(args.vg, nvgRGB(0x2a, 0x2e, 0x35));
			nvgStroke(args.vg);

			// ---- AV valve flap: the modeled valve, visible — swings
			// down into the ventricle while filling, snaps flat when
			// closed (S1 moment)
			nvgBeginPath(args.vg);
			float flapX = filling ? cx + 6.f : cx - 1.f;
			nvgMoveTo(args.vg, cx - 8.f, cy - 9.f);
			nvgLineTo(args.vg, flapX, cy - 1.5f);
			nvgStrokeWidth(args.vg, 1.6f);
			nvgStrokeColor(args.vg, nvgRGB(0xc8, 0xd2, 0xdc));
			nvgLineCap(args.vg, NVG_ROUND);
			nvgStroke(args.vg);

			// aortic valve tick at the arch root (visible while ejecting)
			if (ejecting) {
				nvgBeginPath(args.vg);
				nvgMoveTo(args.vg, cx + 8.5f, cy - 11.5f);
				nvgLineTo(args.vg, cx + 10.5f, cy - 14.5f);
				nvgStrokeWidth(args.vg, 1.4f);
				nvgStrokeColor(args.vg, nvgRGB(0xc8, 0xd2, 0xdc));
				nvgStroke(args.vg);
			}

			nvgRestore(args.vg);
		}
	};

	static void drawPanel(const rack::widget::Widget::DrawArgs& args, float panelW) {
		NVGcolor bg = nvgRGB(0x16, 0x19, 0x1a);
		NVGcolor edge = nvgRGB(0x2a, 0x2e, 0x35);
		NVGcolor label = nvgRGB(0xb8, 0xc2, 0xcc);
		NVGcolor accent = nvgRGB(0xd8, 0x6a, 0x6a);

		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, panelW, rack::app::RACK_GRID_HEIGHT);
		nvgFillColor(args.vg, bg);
		nvgFill(args.vg);
		nvgStrokeWidth(args.vg, 1.f);
		nvgStrokeColor(args.vg, edge);
		nvgStroke(args.vg);

		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, panelW, 6);
		nvgFillColor(args.vg, accent);
		nvgFill(args.vg);

		nvgFontSize(args.vg, 8);
		auto font = getLabelFont();
		if (font && font->handle >= 0)
			nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
		nvgFillColor(args.vg, label);
		auto text = [&](float x, float y, const char* s) {
			nvgText(args.vg, x, y, s, NULL);
		};

		float c1 = 41.f, c2 = 123.f, c3 = 206.f, c4 = 288.f;
		// column 1: anatomy knobs
		text(c1, 96, "MASS");        text(c1, 134, "RATE");
		text(c1, 172, "AV DELAY");   text(c1, 210, "CONTRACT");
		text(c1, 248, "COMPLIANCE"); text(c1, 286, "VALVE -+");
		text(c1, 324, "HRV");
		// column 2: drive knobs + phase lights
		text(c2, 96, "IRRIT");       text(c2, 134, "FILL");
		text(c2, 172, "DRIVE");      text(c2, 210, "AFTERLOAD");
		text(c2, 248, "SOUND");      text(c2, 286, "PHASE");
		// column 3: inputs, then outputs
		text(c3, 96, "PACING");      text(c3, 134, "FILL CV");
		text(c3, 172, "DRIVE CV");   text(c3, 210, "AFT CV");
		text(c3, 248, "VOLUME");     text(c3, 286, "PRESSURE");
		text(c3, 324, "FLOW +-");
		// column 4: outputs
		text(c4, 96, "OUTFLOW");     text(c4, 134, "ARTERIAL");
		text(c4, 172, "ECG");        text(c4, 210, "BEAT");
		text(c4, 248, "ATRIA");      text(c4, 286, "SOUND");

		drawPanelTitle(args, panelW, "HEART");
	}

	CyberlineHeartWidget(CyberlineHeart* module) {
		setModule(module);
		box.size = rack::Vec(PANEL_W, rack::app::RACK_GRID_HEIGHT);

		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, rack::app::RACK_GRID_HEIGHT - 15)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, rack::app::RACK_GRID_HEIGHT - 15)));

		using rack::componentlibrary::RoundSmallBlackKnob;
		using rack::componentlibrary::PJ301MPort;

		float c1 = 41.f, c2 = 123.f, c3 = 206.f, c4 = 288.f;
		// 38 px row pitch: ~23 px widgets + clean space for each label
		float r[7] = {112.f, 150.f, 188.f, 226.f, 264.f, 302.f, 340.f};

		// column 1: anatomy
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[0]), module, CyberlineHeart::MASS_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[1]), module, CyberlineHeart::RATE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[2]), module, CyberlineHeart::AVDELAY_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[3]), module, CyberlineHeart::CONTRACT_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[4]), module, CyberlineHeart::COMPLIANCE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[5]), module, CyberlineHeart::VALVE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[6]), module, CyberlineHeart::HRV_PARAM));

		// column 2: drive
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, r[0]), module, CyberlineHeart::IRRIT_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, r[1]), module, CyberlineHeart::FILL_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, r[2]), module, CyberlineHeart::DRIVE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, r[3]), module, CyberlineHeart::AFTERLOAD_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, r[4]), module, CyberlineHeart::SOUNDLVL_PARAM));
		// phase lights (cyan filling / warm ejecting)
		addChild(rack::createLightCentered<rack::componentlibrary::SmallLight<rack::componentlibrary::GreenLight>>(
			rack::Vec(c2 - 12.f, r[5]), module, CyberlineHeart::FILL_LIGHT));
		addChild(rack::createLightCentered<rack::componentlibrary::SmallLight<rack::componentlibrary::RedLight>>(
			rack::Vec(c2 + 12.f, r[5]), module, CyberlineHeart::EJECT_LIGHT));

		// column 3: inputs + outputs
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c3, r[0]), module, CyberlineHeart::PACING_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c3, r[1]), module, CyberlineHeart::FILLCV_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c3, r[2]), module, CyberlineHeart::DRIVECV_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c3, r[3]), module, CyberlineHeart::AFTERLOADCV_INPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c3, r[4]), module, CyberlineHeart::VOLUME_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c3, r[5]), module, CyberlineHeart::PRESSURE_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c3, r[6]), module, CyberlineHeart::FLOW_OUTPUT));

		// column 4: outputs
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c4, r[0]), module, CyberlineHeart::OUTFLOW_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c4, r[1]), module, CyberlineHeart::ARTERIAL_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c4, r[2]), module, CyberlineHeart::ECG_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c4, r[3]), module, CyberlineHeart::BEAT_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c4, r[4]), module, CyberlineHeart::ATRIA_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c4, r[5]), module, CyberlineHeart::SOUND_OUTPUT));

		// heart gauge, top-center between columns 2 and 3 (bottom edge
		// clears the first label row's text top at ~88 px)
		{
			auto gauge = new HeartGauge(module);
			gauge->box.pos = rack::Vec(128.f, 16.f);
			gauge->box.size = rack::Vec(80.f, 64.f);
			addChild(gauge);
		}
	}

	void draw(const DrawArgs& args) override {
		drawPanel(args, PANEL_W);
		ModuleWidget::draw(args);
	}
};

} // namespace cyberline

namespace cyberline {
rack::plugin::Model* modelCyberlineHeart =
	rack::createModel<CyberlineHeart, CyberlineHeartWidget>("cyberline-heart");
} // namespace cyberline
