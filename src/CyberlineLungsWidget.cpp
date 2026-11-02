// CyberlineLungsWidget — programmatic panel (no SVG assets). 14 HP.
// Single column; label 16 px above its widget.

#include "CyberlineLungs.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineLungsWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 14 * 15.f;

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
		auto font = getLabelFont();
		if (font && font->handle >= 0)
			nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
		nvgFillColor(args.vg, label);
		auto text = [&](float x, float y, const char* s) {
			nvgText(args.vg, x, y, s, NULL);
		};

		float cx = panelW * 0.5f;
		text(cx, 26, "RATE");
		text(cx, 66, "DEPTH");
		text(cx, 104, "INHALE s");
		text(cx, 136, "FLOW MAX");
		text(cx, 168, "EFFORT");
		text(cx, 200, "MODE");

		// inputs
		text(panelW * 0.25f, 234, "TRIG");
		text(cx, 234, "GATE");
		text(panelW * 0.75f, 234, "DEPTH CV");
		// outputs
		text(panelW * 0.15f, 286, "PRESS");
		text(panelW * 0.38f, 286, "VOL");
		text(panelW * 0.62f, 286, "FLOW +-");
		text(panelW * 0.85f, 286, "PHASE");
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
		using rack::componentlibrary::Trimpot;
		using rack::componentlibrary::PJ301MPort;

		float cx = PANEL_W * 0.5f;

		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(cx, 44), module, CyberlineLungs::RATE_PARAM));
		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(cx, 84), module, CyberlineLungs::DEPTH_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(cx, 120), module, CyberlineLungs::INHALE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(cx, 152), module, CyberlineLungs::MAXFLOW_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(cx, 184), module, CyberlineLungs::EFFORT_PARAM));
		addParam(rack::createParamCentered<Trimpot>(rack::Vec(cx, 216), module, CyberlineLungs::MODE_PARAM));

		// Phase light beside the mode switch
		addChild(rack::createLightCentered<rack::componentlibrary::SmallLight<rack::componentlibrary::GreenLight>>(
			rack::Vec(cx + 22, 216), module, CyberlineLungs::PHASE_LIGHT));

		float row = 250.f;
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.25f, row), module, CyberlineLungs::TRIGGER_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cx, row), module, CyberlineLungs::GATE_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.75f, row), module, CyberlineLungs::DEPTHCV_INPUT));

		float row2 = 302.f;
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.15f, row2), module, CyberlineLungs::PRESSURE_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.38f, row2), module, CyberlineLungs::VOLUME_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.62f, row2), module, CyberlineLungs::FLOW_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.85f, row2), module, CyberlineLungs::PHASE_OUTPUT));
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
