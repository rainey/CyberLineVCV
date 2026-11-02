// CyberlineLarynxWidget — programmatic panel (no SVG assets).
// 12 HP. Dark panel, two columns: params top, inputs, outputs bottom.

#include "CyberlineLarynx.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineLarynxWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 12 * 15.f; // 12 HP in px (RACK_GRID_WIDTH)

	static void drawPanel(const rack::widget::Widget::DrawArgs& args, float panelW) {
		NVGcolor bg = nvgRGB(0x18, 0x1a, 0x1e);
		NVGcolor edge = nvgRGB(0x2a, 0x2e, 0x35);
		NVGcolor label = nvgRGB(0xb8, 0xc2, 0xcc);
		NVGcolor accent = nvgRGB(0xe0, 0x54, 0x5a);

		// Background
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, panelW, rack::app::RACK_GRID_HEIGHT);
		nvgFillColor(args.vg, bg);
		nvgFill(args.vg);
		nvgStrokeWidth(args.vg, 1.f);
		nvgStrokeColor(args.vg, edge);
		nvgStroke(args.vg);

		// Accent strip
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, panelW, 6);
		nvgFillColor(args.vg, accent);
		nvgFill(args.vg);

		// Labels
		nvgFontSize(args.vg, 8);
		nvgFontFace(args.vg, "bold");
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
		nvgFillColor(args.vg, label);

		auto text = [&](float x, float y, const char* s) {
			nvgText(args.vg, x, y, s, NULL);
		};

		text(panelW * 0.25f, 22, "TENSION");
		text(panelW * 0.75f, 22, "PRESSURE");
		text(panelW * 0.25f, 52, "FOLD mm");
		text(panelW * 0.75f, 52, "OCT");
		text(panelW * 0.25f, 80, "DRIFT");
		text(panelW * 0.75f, 80, "VIB Hz");
		text(panelW * 0.25f, 108, "FB GAIN");
		text(panelW * 0.75f, 108, "P-PITCH");

		text(panelW * 0.25f, 132, "PITCH");
		text(panelW * 0.75f, 132, "TENS");
		text(panelW * 0.25f, 192, "PRESS");
		text(panelW * 0.75f, 192, "FB IN");
		text(panelW * 0.25f, 252, "VOICED");
		text(panelW * 0.75f, 252, "N.MOD");
	}

	CyberlineLarynxWidget(CyberlineLarynx* module) {
		setModule(module);
		box.size = rack::Vec(PANEL_W, rack::app::RACK_GRID_HEIGHT);

		// Screws
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, rack::app::RACK_GRID_HEIGHT - 15)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, rack::app::RACK_GRID_HEIGHT - 15)));

		using rack::componentlibrary::RoundBlackKnob;
		using rack::componentlibrary::RoundSmallBlackKnob;
		using rack::componentlibrary::PJ301MPort;

		float cx1 = PANEL_W * 0.25f;
		float cx2 = PANEL_W * 0.75f;

		// Params (row pairs)
		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(cx1, 44), module, CyberlineLarynx::TENSION_PARAM));
		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(cx2, 44), module, CyberlineLarynx::PRESSURE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(cx1, 70), module, CyberlineLarynx::FOLDLEN_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(cx2, 70), module, CyberlineLarynx::OCT_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(cx1, 98), module, CyberlineLarynx::DRIFT_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(cx2, 98), module, CyberlineLarynx::VIBRATE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(cx1, 126), module, CyberlineLarynx::FBGAIN_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(cx2, 126), module, CyberlineLarynx::PCOUPLE_PARAM));

		// Inputs
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cx1, 156), module, CyberlineLarynx::PITCH_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cx2, 156), module, CyberlineLarynx::TENSION_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cx1, 216), module, CyberlineLarynx::PRESSURE_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cx2, 216), module, CyberlineLarynx::FEEDBACK_INPUT));

		// Outputs
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(cx1, 276), module, CyberlineLarynx::VOICED_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(cx2, 276), module, CyberlineLarynx::NOISEMOD_OUTPUT));
	}

	void draw(const DrawArgs& args) override {
		drawPanel(args, PANEL_W);
		ModuleWidget::draw(args);
	}
};

} // namespace cyberline

namespace cyberline {
rack::plugin::Model* modelCyberlineLarynx =
	rack::createModel<cyberline::CyberlineLarynx, cyberline::CyberlineLarynxWidget>("cyberline-larynx");
} // namespace cyberline
