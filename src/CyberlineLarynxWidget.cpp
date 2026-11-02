// CyberlineLarynxWidget — programmatic panel (no SVG assets).
// 12 HP. Grid: label 16 px above its widget; rows spread over 380 px.

#include "CyberlineLarynx.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineLarynxWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 12 * 15.f; // 12 HP in px

	static void drawPanel(const rack::widget::Widget::DrawArgs& args, float panelW) {
		NVGcolor bg = nvgRGB(0x18, 0x1a, 0x1e);
		NVGcolor edge = nvgRGB(0x2a, 0x2e, 0x35);
		NVGcolor label = nvgRGB(0xb8, 0xc2, 0xcc);
		NVGcolor accent = nvgRGB(0xe0, 0x54, 0x5a);

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

		float c1 = panelW * 0.25f, c2 = panelW * 0.75f;
		// params
		text(c1, 24, "TENSION");   text(c2, 24, "PRESSURE");
		text(c1, 62, "FOLD mm");   text(c2, 62, "OCT");
		text(c1, 96, "DRIFT");     text(c2, 96, "VIB Hz");
		text(c1, 130, "FB GAIN");  text(c2, 130, "P-PITCH");
		// inputs
		text(c1, 168, "PITCH");    text(c2, 168, "TENS");
		text(c1, 208, "PRESS");    text(c2, 208, "FB IN");
		// outputs
		text(c1, 256, "VOICED");   text(c2, 256, "N.MOD");
	}

	CyberlineLarynxWidget(CyberlineLarynx* module) {
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

		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(c1, 40), module, CyberlineLarynx::TENSION_PARAM));
		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(c2, 40), module, CyberlineLarynx::PRESSURE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, 78), module, CyberlineLarynx::FOLDLEN_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 78), module, CyberlineLarynx::OCT_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, 112), module, CyberlineLarynx::DRIFT_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 112), module, CyberlineLarynx::VIBRATE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, 146), module, CyberlineLarynx::FBGAIN_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 146), module, CyberlineLarynx::PCOUPLE_PARAM));

		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c1, 184), module, CyberlineLarynx::PITCH_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c2, 184), module, CyberlineLarynx::TENSION_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c1, 224), module, CyberlineLarynx::PRESSURE_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c2, 224), module, CyberlineLarynx::FEEDBACK_INPUT));

		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c1, 272), module, CyberlineLarynx::VOICED_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c2, 272), module, CyberlineLarynx::NOISEMOD_OUTPUT));
	}

	void draw(const DrawArgs& args) override {
		drawPanel(args, PANEL_W);
		ModuleWidget::draw(args);
	}
};

} // namespace cyberline

namespace cyberline {
rack::plugin::Model* modelCyberlineLarynx =
	rack::createModel<CyberlineLarynx, CyberlineLarynxWidget>("cyberline-larynx");
} // namespace cyberline
