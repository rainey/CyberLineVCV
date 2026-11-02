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
		float colL = 60.f, colR = 150.f;
		// params (2 columns; label baseline ~14 px above its widget top)
		text(colL, 30, "RATE");      text(colR, 30, "DEPTH");
		text(colL, 84, "INHALE s");  text(colR, 84, "FLOW MAX");
		text(colL, 134, "EFFORT");   text(colR, 134, "MODE");

		// inputs
		text(panelW * 0.25f, 196, "TRIG");
		text(cx, 196, "GATE");
		text(panelW * 0.75f, 196, "DEPTH CV");
		// outputs
		text(panelW * 0.15f, 266, "PRESS");
		text(panelW * 0.38f, 266, "VOL");
		text(panelW * 0.62f, 266, "FLOW +-");
		text(panelW * 0.85f, 266, "PHASE");

		drawPanelTitle(args, panelW, "BREATH");
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
		float colL = 60.f, colR = 150.f;

		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(colL, 56), module, CyberlineLungs::RATE_PARAM));
		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(colR, 56), module, CyberlineLungs::DEPTH_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(colL, 106), module, CyberlineLungs::INHALE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(colR, 106), module, CyberlineLungs::MAXFLOW_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(colL, 156), module, CyberlineLungs::EFFORT_PARAM));
		addParam(rack::createParamCentered<Trimpot>(rack::Vec(colR, 156), module, CyberlineLungs::MODE_PARAM));

		// Phase light beside the mode switch
		addChild(rack::createLightCentered<rack::componentlibrary::SmallLight<rack::componentlibrary::GreenLight>>(
			rack::Vec(colR + 22, 156), module, CyberlineLungs::PHASE_LIGHT));

		float row = 220.f;
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.25f, row), module, CyberlineLungs::TRIGGER_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cx, row), module, CyberlineLungs::GATE_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.75f, row), module, CyberlineLungs::DEPTHCV_INPUT));

		float row2 = 290.f;
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
