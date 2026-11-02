// CyberlineOralChamberWidget — programmatic panel (no SVG assets). 16 HP.
// Three-column grid; label 16 px above its widget.

#include "CyberlineOralChamber.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineOralChamberWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 16 * 15.f;

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
		auto font = getLabelFont();
		if (font && font->handle >= 0)
			nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
		nvgFillColor(args.vg, label);
		auto text = [&](float x, float y, const char* s) {
			nvgText(args.vg, x, y, s, NULL);
		};

		float c1 = panelW * 0.2f, c2 = panelW * 0.5f, c3 = panelW * 0.8f;
		// label baseline = widget center - 16 (ports) / - 16 (knobs);
		// rows spaced so each label top clears the widget above by >= 5 px
		// (real component boxes: big knob r14.2, small r11.3, port r11.9)
		text(c1, 26, "VTL cm");    text(c3, 26, "MUSCLE");
		text(c1, 68, "TNG X");     text(c2, 68, "TNG Y");   text(c3, 68, "VELUM");
		text(c1, 106, "C.POS");    text(c2, 106, "C.WIDTH"); text(c3, 106, "FRIC");
		text(c1, 144, "WALL");     text(c2, 144, "INJ POS");

		text(c1, 184, "SRC");      text(c3, 184, "INJECT");
		text(panelW * 0.09f, 224, "TNGX");  text(panelW * 0.245f, 224, "TNGY");
		text(panelW * 0.40f, 224, "VEL");   text(panelW * 0.56f, 224, "CPOS");
		text(panelW * 0.71f, 224, "CWID");  text(panelW * 0.87f, 224, "FRIC");
		text(c2, 264, "INJPOS CV");

		text(c1, 304, "LIPS");     text(c2, 304, "NOSE");   text(c3, 304, "SHAPE");
		text(c1, 344, "PROBE");

		drawPanelTitle(args, panelW, "ORAL CHAMBER");
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

		float c1 = PANEL_W * 0.2f, c2 = PANEL_W * 0.5f, c3 = PANEL_W * 0.8f;

		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(c1, 42), module, CyberlineOralChamber::VTL_PARAM));
		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(c3, 42), module, CyberlineOralChamber::SPEED_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, 84), module, CyberlineOralChamber::TONGUEX_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 84), module, CyberlineOralChamber::TONGUEY_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c3, 84), module, CyberlineOralChamber::VELUM_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, 122), module, CyberlineOralChamber::CONSTRPOS_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 122), module, CyberlineOralChamber::CONSTRW_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c3, 122), module, CyberlineOralChamber::FRIC_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, 160), module, CyberlineOralChamber::WALL_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 160), module, CyberlineOralChamber::INJPOS_PARAM));

		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c1, 200), module, CyberlineOralChamber::SOURCE_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c3, 200), module, CyberlineOralChamber::INJECT_INPUT));

		float cvRow = 240.f;
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.09f, cvRow), module, CyberlineOralChamber::TONGUEX_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.245f, cvRow), module, CyberlineOralChamber::TONGUEY_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.40f, cvRow), module, CyberlineOralChamber::VELUM_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.56f, cvRow), module, CyberlineOralChamber::CONSTRPOS_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.71f, cvRow), module, CyberlineOralChamber::CONSTRW_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.87f, cvRow), module, CyberlineOralChamber::FRIC_INPUT));

		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c2, 280), module, CyberlineOralChamber::INJPOS_INPUT));

		float outRow = 316.f;
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c1, outRow), module, CyberlineOralChamber::LIPS_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c2, outRow), module, CyberlineOralChamber::NOSE_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c3, outRow), module, CyberlineOralChamber::SHAPE_OUTPUT));
		// bottom-left, clear of the centered title block and corner screws
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c1, 358), module, CyberlineOralChamber::PROBE_OUTPUT));
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
