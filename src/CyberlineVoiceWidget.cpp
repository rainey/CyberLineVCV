// CyberlineVoiceWidget — programmatic panel (no SVG assets). 16 HP.
// Three-column grid; label 16 px above its widget.

#include "CyberlineVoice.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineVoiceWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 16 * 15.f;

	static void drawPanel(const rack::widget::Widget::DrawArgs& args, float panelW) {
		NVGcolor bg = nvgRGB(0x1a, 0x17, 0x19);
		NVGcolor edge = nvgRGB(0x2a, 0x2e, 0x35);
		NVGcolor label = nvgRGB(0xb8, 0xc2, 0xcc);
		NVGcolor accent = nvgRGB(0xe0, 0x54, 0x9a);

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
		text(c1, 24, "TENSION");  text(c3, 24, "PRESSURE");
		text(c1, 60, "FOLD mm");  text(c2, 60, "OCT");     text(c3, 60, "DRIFT");
		text(c1, 94, "FB GAIN");  text(c2, 94, "P-PITCH"); text(c3, 94, "VTL cm");
		text(c1, 128, "TNG X");   text(c2, 128, "TNG Y");  text(c3, 128, "VELUM");
		text(c2, 162, "MUSCLE");

		text(c1, 186, "PITCH");   text(c2, 186, "PRESS");  text(c3, 186, "TENS");
		text(c1, 226, "TNGX CV"); text(c2, 226, "VELUM CV"); text(c3, 226, "TNGY CV");

		text(c1, 274, "LIPS");    text(c2, 274, "NOSE");   text(c3, 274, "SOURCE");
	}

	CyberlineVoiceWidget(CyberlineVoice* module) {
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

		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(c1, 40), module, CyberlineVoice::V_TENSION_PARAM));
		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(c3, 40), module, CyberlineVoice::V_PRESSURE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, 76), module, CyberlineVoice::V_FOLDLEN_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 76), module, CyberlineVoice::V_PITCHOCT_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c3, 76), module, CyberlineVoice::V_DRIFT_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, 110), module, CyberlineVoice::V_FB_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 110), module, CyberlineVoice::V_PCOUPLE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c3, 110), module, CyberlineVoice::V_VTL_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, 144), module, CyberlineVoice::V_TONGUEX_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 144), module, CyberlineVoice::V_TONGUEY_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c3, 144), module, CyberlineVoice::V_VELUM_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 178), module, CyberlineVoice::V_SPEED_PARAM));

		// voicing light beside MUSCLE knob
		addChild(rack::createLightCentered<rack::componentlibrary::SmallLight<rack::componentlibrary::RedLight>>(
			rack::Vec(c2 + 26, 178), module, CyberlineVoice::V_PHASE_LIGHT));

		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c1, 202), module, CyberlineVoice::V_PITCH_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c2, 202), module, CyberlineVoice::V_PRESSURE_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c3, 202), module, CyberlineVoice::V_TENSION_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c1, 242), module, CyberlineVoice::V_TONGUEX_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c2, 242), module, CyberlineVoice::V_VELUM_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c3, 242), module, CyberlineVoice::V_TONGUEY_INPUT));

		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c1, 290), module, CyberlineVoice::V_LIPS_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c2, 290), module, CyberlineVoice::V_NOSE_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c3, 290), module, CyberlineVoice::V_VOICED_OUTPUT));
	}

	void draw(const DrawArgs& args) override {
		drawPanel(args, PANEL_W);
		ModuleWidget::draw(args);
	}
};

} // namespace cyberline

namespace cyberline {
rack::plugin::Model* modelCyberlineVoice =
	rack::createModel<CyberlineVoice, CyberlineVoiceWidget>("cyberline-voice");
} // namespace cyberline
