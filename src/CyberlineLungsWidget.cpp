// CyberlineLungsWidget — programmatic panel (no SVG assets). 14 HP.

#include "CyberlineLungs.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineLungsWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 14 * 15.f; // 14 HP in px

	static void drawPanel(const rack::widget::Widget::DrawArgs& args, float panelW) {
		NVGcolor bg = nvgRGB(0x16, 0x19, 0x1a);
		NVGcolor edge = nvgRGB(0x2a, 0x2e, 0x35);
		NVGcolor label = nvgRGB(0xb8, 0xc2, 0xcc);
		NVGcolor accent = nvgRGB(0x5a, 0xc8, 0xe0);

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
		nvgFontFace(args.vg, "bold");
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
		nvgFillColor(args.vg, label);
		auto text = [&](float x, float y, const char* s) {
			nvgText(args.vg, x, y, s, NULL);
		};

		float cx = panelW * 0.5f;
		text(cx, 22, "RATE");
		text(cx, 50, "DEPTH");
		text(cx, 78, "INHALE");
		text(cx, 106, "FLOW MAX");
		text(cx, 134, "EFFORT");

		text(cx, 162, "TRIG   GATE   DEPTH");
		text(cx, 222, "PRESS  VOL  FLOW+-  PHASE");
	}

	CyberlineLungsWidget(CyberlineLungs* module) {
		setModule(module);
		box.size = rack::Vec(PANEL_W, rack::app::RACK_GRID_HEIGHT);

		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, rack::app::RACK_GRID_HEIGHT - 15)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, rack::app::RACK_GRID_HEIGHT - 15)));

		using rack::componentlibrary::RoundBlackKnob;
		using rack::componentlibrary::RoundSmallBlackKnob;
		using rack::componentlibrary::PJ301MPort;

		float cx = PANEL_W * 0.5f;

		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(cx, 40), module, CyberlineLungs::RATE_PARAM));
		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(cx, 68), module, CyberlineLungs::DEPTH_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(cx, 94), module, CyberlineLungs::INHALE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(cx, 120), module, CyberlineLungs::MAXFLOW_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(cx, 146), module, CyberlineLungs::EFFORT_PARAM));
		addParam(rack::createParamCentered<rack::componentlibrary::Trimpot>(rack::Vec(cx, 172), module, CyberlineLungs::MODE_PARAM));

		// Inputs row
		float row = 190.f;
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.25f, row), module, CyberlineLungs::TRIGGER_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cx, row), module, CyberlineLungs::GATE_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.75f, row), module, CyberlineLungs::DEPTHCV_INPUT));

		// Outputs row
		float row2 = 250.f;
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.15f, row2), module, CyberlineLungs::PRESSURE_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.38f, row2), module, CyberlineLungs::VOLUME_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.62f, row2), module, CyberlineLungs::FLOW_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.85f, row2), module, CyberlineLungs::PHASE_OUTPUT));

		// Phase light between rows
		addChild(rack::createLightCentered<rack::componentlibrary::SmallLight<rack::componentlibrary::GreenLight>>(
			rack::Vec(cx, 218), module, CyberlineLungs::PHASE_LIGHT));
	}

	void draw(const DrawArgs& args) override {
		drawPanel(args, PANEL_W);
		ModuleWidget::draw(args);
	}
};

} // namespace cyberline

namespace cyberline {
rack::plugin::Model* modelCyberlineLungs =
	rack::createModel<CyberlineLungs, CyberlineLungsWidget>("cyberline-lungs");
} // namespace cyberline
