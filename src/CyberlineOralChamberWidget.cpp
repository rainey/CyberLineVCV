// CyberlineOralChamberWidget — programmatic panel (no SVG assets). 16 HP.

#include "CyberlineOralChamber.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineOralChamberWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 16 * 15.f; // 16 HP in px

	static void drawPanel(const rack::widget::Widget::DrawArgs& args, float panelW) {
		NVGcolor bg = nvgRGB(0x17, 0x18, 0x1d);
		NVGcolor edge = nvgRGB(0x2a, 0x2e, 0x35);
		NVGcolor label = nvgRGB(0xb8, 0xc2, 0xcc);
		NVGcolor accent = nvgRGB(0x7a, 0x5a, 0xe0);

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

		float c1 = panelW * 0.25f, c2 = panelW * 0.75f;
		text(c1, 22, "VTL cm");
		text(c2, 22, "MUSCLE");
		text(c1, 50, "TONGUE X");
		text(c2, 50, "TONGUE Y");
		text(c1, 78, "VELUM");
		text(c2, 78, "WALL");
		text(c1, 106, "C.POS");
		text(c2, 106, "C.WIDTH");
		text(c1, 134, "FRIC");
		text(c2, 134, "INJ POS");

		text(c1, 160, "SRC");
		text(c2, 160, "INJECT");
		text(c1, 224, "LIPS");
		text(c2, 224, "NOSE");
	}

	CyberlineOralChamberWidget(CyberlineOralChamber* module) {
		setModule(module);
		box.size = rack::Vec(PANEL_W, rack::app::RACK_GRID_HEIGHT);

		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, rack::app::RACK_GRID_HEIGHT - 15)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, rack::app::RACK_GRID_HEIGHT - 15)));

		using rack::componentlibrary::RoundBlackKnob;
		using rack::componentlibrary::RoundSmallBlackKnob;
		using rack::componentlibrary::PJ301MPort;

		float c1 = PANEL_W * 0.25f, c2 = PANEL_W * 0.75f;

		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(c1, 40), module, CyberlineOralChamber::VTL_PARAM));
		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(c2, 40), module, CyberlineOralChamber::SPEED_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, 66), module, CyberlineOralChamber::TONGUEX_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 66), module, CyberlineOralChamber::TONGUEY_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, 92), module, CyberlineOralChamber::VELUM_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 92), module, CyberlineOralChamber::WALL_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, 118), module, CyberlineOralChamber::CONSTRPOS_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 118), module, CyberlineOralChamber::CONSTRW_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, 144), module, CyberlineOralChamber::FRIC_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 144), module, CyberlineOralChamber::INJPOS_PARAM));

		// Source inputs
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c1, 182), module, CyberlineOralChamber::SOURCE_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c2, 182), module, CyberlineOralChamber::INJECT_INPUT));

		// CV inputs row (compact, center band)
		float cvRow = 246.f;
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.11f, cvRow), module, CyberlineOralChamber::TONGUEX_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.26f, cvRow), module, CyberlineOralChamber::TONGUEY_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.41f, cvRow), module, CyberlineOralChamber::VELUM_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.56f, cvRow), module, CyberlineOralChamber::CONSTRPOS_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.71f, cvRow), module, CyberlineOralChamber::CONSTRW_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.86f, cvRow), module, CyberlineOralChamber::FRIC_INPUT));

		// Outputs
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c1, 300), module, CyberlineOralChamber::LIPS_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c2, 300), module, CyberlineOralChamber::NOSE_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.35f, 344), module, CyberlineOralChamber::SHAPE_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.65f, 344), module, CyberlineOralChamber::PROBE_OUTPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c2, 344), module, CyberlineOralChamber::INJPOS_INPUT));
	}

	void draw(const DrawArgs& args) override {
		drawPanel(args, PANEL_W);
		ModuleWidget::draw(args);
	}
};

} // namespace cyberline

namespace cyberline {
rack::plugin::Model* modelCyberlineOralChamber =
	rack::createModel<CyberlineOralChamber, CyberlineOralChamberWidget>("cyberline-oral-chamber");
} // namespace cyberline
