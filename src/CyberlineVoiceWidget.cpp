// CyberlineVoiceWidget — programmatic panel (no SVG assets). 16 HP.

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
		nvgFontFace(args.vg, "bold");
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
		nvgFillColor(args.vg, label);
		auto text = [&](float x, float y, const char* s) {
			nvgText(args.vg, x, y, s, NULL);
		};

		float c1 = panelW * 0.25f, c2 = panelW * 0.75f;
		text(c1, 22, "TENSION");
		text(c2, 22, "PRESSURE");
		text(c1, 50, "FOLD mm");
		text(c2, 50, "OCT");
		text(c1, 78, "DRIFT");
		text(c2, 78, "FB GAIN");
		text(c1, 106, "P-PITCH");
		text(c2, 106, "VTL cm");
		text(c1, 134, "TNG X");
		text(c2, 134, "TNG Y");
		text(c1, 162, "VELUM");
		text(c2, 162, "MUSCLE");

		text(c1, 188, "PITCH");
		text(c2, 188, "TENS");
		text(panelW * 0.5f, 188, "PRESS");
		text(c1, 250, "TNG X CV");
		text(c2, 250, "TNG Y CV");
		text(panelW * 0.5f, 250, "VELUM CV");
		text(c1, 306, "LIPS");
		text(panelW * 0.5f, 306, "NOSE");
		text(c2, 306, "SOURCE");
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

		float c1 = PANEL_W * 0.25f, c2 = PANEL_W * 0.75f;

		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(c1, 40), module, CyberlineVoice::V_TENSION_PARAM));
		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(c2, 40), module, CyberlineVoice::V_PRESSURE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, 66), module, CyberlineVoice::V_FOLDLEN_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 66), module, CyberlineVoice::V_PITCHOCT_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, 92), module, CyberlineVoice::V_DRIFT_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 92), module, CyberlineVoice::V_FB_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, 118), module, CyberlineVoice::V_PCOUPLE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 118), module, CyberlineVoice::V_VTL_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, 144), module, CyberlineVoice::V_TONGUEX_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 144), module, CyberlineVoice::V_TONGUEY_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, 170), module, CyberlineVoice::V_VELUM_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, 170), module, CyberlineVoice::V_SPEED_PARAM));

		// inputs
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c1, 206), module, CyberlineVoice::V_PITCH_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.5f, 206), module, CyberlineVoice::V_PRESSURE_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c2, 206), module, CyberlineVoice::V_TENSION_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c1, 266), module, CyberlineVoice::V_TONGUEX_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.5f, 266), module, CyberlineVoice::V_VELUM_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c2, 266), module, CyberlineVoice::V_TONGUEY_INPUT));

		// outputs
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c1, 320), module, CyberlineVoice::V_LIPS_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.5f, 320), module, CyberlineVoice::V_NOSE_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c2, 320), module, CyberlineVoice::V_VOICED_OUTPUT));

		addChild(rack::createLightCentered<rack::componentlibrary::SmallLight<rack::componentlibrary::RedLight>>(
			rack::Vec(PANEL_W * 0.5f, 288), module, CyberlineVoice::V_PHASE_LIGHT));
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
