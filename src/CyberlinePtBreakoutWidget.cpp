// CyberlinePtBreakoutWidget — programmatic 14 HP hollow neon vector panel
// for "PT BREAKOUT" (Pink Trombone adapter). House styling: flat dark plate,
// violet neon top bar, jack boxes, neon port rings, PT-destination labels.

#include "CyberlinePtBreakout.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlinePtBreakoutWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 14 * 15.f; // 210 px (14 HP)

	static void strokeNeon(NVGcontext* vg, NVGcolor col, float coreW = 1.2f, float glowW = 3.4f, int glowAlpha = 50) {
		nvgStrokeWidth(vg, glowW);
		nvgStrokeColor(vg, nvgRGBA(col.r * 255, col.g * 255, col.b * 255, glowAlpha));
		nvgStroke(vg);
		nvgStrokeWidth(vg, coreW);
		nvgStrokeColor(vg, col);
		nvgStroke(vg);
	}

	static void drawPanel(const rack::widget::Widget::DrawArgs& args, float panelW) {
		NVGcontext* vg = args.vg;
		NVGcolor bg = nvgRGB(0x13, 0x14, 0x1a);
		NVGcolor edge = nvgRGB(0x2a, 0x2e, 0x38);
		NVGcolor label = nvgRGB(0xb8, 0xc2, 0xcc);
		NVGcolor neonViolet = nvgRGB(0x9b, 0x6d, 0xff);
		NVGcolor neonCyan   = nvgRGB(0x48, 0xd8, 0xf8);
		NVGcolor neonCoral  = nvgRGB(0xf0, 0x5c, 0x64);
		NVGcolor neonAmber  = nvgRGB(0xf4, 0xb0, 0x48);
		NVGcolor neonMint   = nvgRGB(0x58, 0xe0, 0xa8);

		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, panelW, rack::app::RACK_GRID_HEIGHT);
		nvgFillColor(vg, bg);
		nvgFill(vg);
		nvgStrokeWidth(vg, 1.f);
		nvgStrokeColor(vg, edge);
		nvgStroke(vg);

		// Top neon bar + hollow cyberline sub-trace
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, panelW, 5);
		nvgFillColor(vg, neonCoral);
		nvgFill(vg);
		nvgBeginPath(vg);
		nvgMoveTo(vg, 0, 7.5f);
		nvgLineTo(vg, panelW, 7.5f);
		nvgStrokeWidth(vg, 0.8f);
		nvgStrokeColor(vg, nvgRGBA(0xf0, 0x5c, 0x64, 130));
		nvgStroke(vg);

		float rIn = 92.f, rOut1 = 158.f, rOut2 = 216.f, rOut3 = 274.f;
		float outX[3] = {42.f, 105.f, 168.f};

		// Jack boxes
		auto drawJackBox = [&](float by0, float by1, NVGcolor strokeCol) {
			float bx0 = 8.f, bx1 = panelW - 8.f;
			float bch = 4.5f;
			nvgBeginPath(vg);
			nvgMoveTo(vg, bx0 + bch, by0);
			nvgLineTo(vg, bx1 - bch, by0);
			nvgLineTo(vg, bx1, by0 + bch);
			nvgLineTo(vg, bx1, by1 - bch);
			nvgLineTo(vg, bx1 - bch, by1);
			nvgLineTo(vg, bx0 + bch, by1);
			nvgLineTo(vg, bx0, by1 - bch);
			nvgLineTo(vg, bx0, by0 + bch);
			nvgClosePath(vg);
			nvgFillColor(vg, nvgRGBA(0x0b, 0x0d, 0x13, 115));
			nvgFill(vg);
			nvgStrokeWidth(vg, 0.9f);
			nvgStrokeColor(vg, strokeCol);
			nvgStroke(vg);
		};
		drawJackBox(rIn - 18.f, rIn + 16.5f, nvgRGBA(0x8e, 0x62, 0xf8, 95));
		drawJackBox(rOut1 - 18.f, rOut3 + 16.5f, nvgRGBA(0x8e, 0x62, 0xf8, 95));

		auto drawPortRing = [&](float x, float y, NVGcolor col, bool isOutput) {
			nvgBeginPath(vg);
			nvgCircle(vg, x, y, isOutput ? 13.8f : 12.8f);
			strokeNeon(vg, col, 1.0f, 2.6f, 38);
		};

		// Inputs: BUS (violet), V/OCT (mint)
		drawPortRing(60.f, rIn, neonViolet, false);
		drawPortRing(150.f, rIn, neonMint, false);

		// Outputs row 1: VOCT (mint), TNG X (cyan), TNG Y (cyan)
		drawPortRing(outX[0], rOut1, neonMint, true);
		drawPortRing(outX[1], rOut1, neonCyan, true);
		drawPortRing(outX[2], rOut1, neonCyan, true);
		// Row 2: THROAT X (amber), FRIC FC (amber), FRIC LVL (amber)
		drawPortRing(outX[0], rOut2, neonAmber, true);
		drawPortRing(outX[1], rOut2, neonAmber, true);
		drawPortRing(outX[2], rOut2, neonAmber, true);
		// Row 3: TENSE (coral), VEL (dim monitor)
		drawPortRing(outX[1], rOut3, neonCoral, true);
		drawPortRing(outX[2], rOut3, nvgRGB(0x6a, 0x74, 0x80), true);

		auto font = getLabelFont();
		if (font && font->handle >= 0)
			nvgFontFaceId(vg, font->handle);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);

		auto text = [&](float x, float y, const char* s) {
			nvgText(vg, x, y, s, NULL);
		};

		// Knobs: FOLD mm + TENSE + SRC switch (top row)
		nvgFontSize(vg, 8.f);
		nvgFillColor(vg, label);
		text(40.f, 48.f, "FOLD mm");
		text(105.f, 48.f, "TENSE");
		nvgFontSize(vg, 6.2f);
		text(170.f, 26.f, "SRC");
		text(170.f, 36.f, "LAW");
		text(170.f, 66.f, "VOCT");

		// Input labels
		nvgFontSize(vg, 6.2f);
		nvgFillColor(vg, nvgRGB(0xd0, 0xb0, 0xff));
		text(60.f, rIn - 15.f, "PT BUS");
		nvgFillColor(vg, nvgRGB(0x80, 0xe8, 0xb8));
		text(150.f, rIn - 15.f, "V/OCT");

		// Output labels (with PT destination hint below)
		nvgFontSize(vg, 7.5f);
		nvgFillColor(vg, nvgRGB(0x80, 0xe8, 0xb8));
		text(outX[0], rOut1 - 15.f, "VOCT");
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(outX[1], rOut1 - 15.f, "TNG X");
		text(outX[2], rOut1 - 15.f, "TNG Y");
		nvgFillColor(vg, nvgRGB(0xf8, 0xc0, 0x78));
		text(outX[0], rOut2 - 15.f, "THROT X");
		text(outX[1], rOut2 - 15.f, "FR FC");
		text(outX[2], rOut2 - 15.f, "FR LVL");
		nvgFillColor(vg, nvgRGB(0xf8, 0x90, 0x98));
		text(outX[1], rOut3 - 15.f, "TENSE");
		nvgFillColor(vg, nvgRGB(0x9a, 0xa4, 0xae));
		text(outX[2], rOut3 - 15.f, "VEL");

		// Destination hints (tiny, under each row)
		nvgFontSize(vg, 5.6f);
		nvgFillColor(vg, nvgRGB(0x6a, 0x74, 0x80));
		text(outX[0], rOut1 + 20.f, "VOCT");
		text(outX[1], rOut1 + 20.f, "TONGUE XI");
		text(outX[2], rOut1 + 20.f, "TONGUE YI");
		text(outX[0], rOut2 + 20.f, "THROAT XI");
		text(outX[1], rOut2 + 20.f, "FRICFCI");
		text(outX[2], rOut2 + 20.f, "FRICL");
		text(outX[1], rOut3 + 20.f, "TENSEI");
		text(outX[2], rOut3 + 20.f, "(no sink)");

		// Usage note (between TENSE knob and input row)
		nvgFontSize(vg, 5.6f);
		nvgFillColor(vg, nvgRGB(0x6a, 0x74, 0x80));
		text(78.f, 64.f, "Broca/BrocaSq menu: Pink Trombone poly mode ON.");
		text(78.f, 70.f, "Set PT tongue/throat knobs centered, attenver. 1.0.");

		drawPanelTitle(args, panelW, "PT BREAKOUT");
	}

	CyberlinePtBreakoutWidget(CyberlinePtBreakout* module) {
		setModule(module);
		box.size = rack::Vec(PANEL_W, rack::app::RACK_GRID_HEIGHT);

		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, rack::app::RACK_GRID_HEIGHT - 15)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, rack::app::RACK_GRID_HEIGHT - 15)));

		using rack::componentlibrary::RoundSmallBlackKnob;
		using rack::componentlibrary::PJ301MPort;

		// Top row: FOLD knob, TENSE knob, SRC switch
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(40.f, 30.f), module, CyberlinePtBreakout::FOLD_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(105.f, 30.f), module, CyberlinePtBreakout::TENSE_PARAM));
		{
			// Tiny 2-position vertical snap switch for SRC
			struct SrcSwitch : rack::app::ParamWidget {
				float slotY(int i) const { return 8.f + i * 14.f; }
				void initParamQuantity() override {
					ParamWidget::initParamQuantity();
					if (auto* pq = getParamQuantity()) {
						pq->snapEnabled = true;
						pq->smoothEnabled = false;
					}
				}
				void onDragEnd(const rack::event::DragEnd& e) override {
					ParamWidget::onDragEnd(e);
					if (auto* pq = getParamQuantity())
						pq->setValue(pq->getValue() < 0.5f ? 1.f : 0.f);
				}
				void draw(const DrawArgs& args) override {
					float w = box.size.x, h = box.size.y;
					nvgBeginPath(args.vg);
					nvgRoundedRect(args.vg, w * 0.5f - 3.f, 2.f, 6.f, h - 4.f, 3.f);
					nvgFillColor(args.vg, nvgRGB(0x0d, 0x0f, 0x12));
					nvgFill(args.vg);
					nvgStrokeWidth(args.vg, 1.f);
					nvgStrokeColor(args.vg, nvgRGB(0x2a, 0x2e, 0x35));
					nvgStroke(args.vg);
					float v = getParamQuantity() ? getParamQuantity()->getValue() : 0.f;
					int pos = (int) std::round(rack::math::clamp(v, 0.f, 1.f));
					nvgBeginPath(args.vg);
					nvgRoundedRect(args.vg, 1.f, slotY(pos) - 4.f, w - 2.f, 8.f, 3.f);
					nvgFillColor(args.vg, nvgRGB(0x5a, 0xc8, 0xe0));
					nvgFill(args.vg);
				}
			};
			auto* sw = rack::createParamCentered<SrcSwitch>(rack::Vec(170.f, 44.f), module, CyberlinePtBreakout::SRC_PARAM);
			sw->box.size = rack::Vec(14.f, 30.f);
			addParam(sw);
		}

		// Inputs
		float rIn = 92.f;
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(60.f, rIn), module, CyberlinePtBreakout::BUS_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(150.f, rIn), module, CyberlinePtBreakout::VOCT_INPUT));

		// Outputs
		float outX[3] = {42.f, 105.f, 168.f};
		float rOut1 = 158.f, rOut2 = 216.f, rOut3 = 274.f;
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[0], rOut1), module, CyberlinePtBreakout::VOCT_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[1], rOut1), module, CyberlinePtBreakout::TONGUEX_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[2], rOut1), module, CyberlinePtBreakout::TONGUEY_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[0], rOut2), module, CyberlinePtBreakout::THROATX_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[1], rOut2), module, CyberlinePtBreakout::FRICFC_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[2], rOut2), module, CyberlinePtBreakout::FRICLVL_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[1], rOut3), module, CyberlinePtBreakout::TENSE_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[2], rOut3), module, CyberlinePtBreakout::VEL_OUTPUT));
	}

	void appendContextMenu(rack::ui::Menu* menu) override {
		ModuleWidget::appendContextMenu(menu);
		// No quality-dependent DSP; expose the plugin-wide global.
		appendQualityMenu(menu, nullptr, -1);
	}

	void draw(const DrawArgs& args) override {
		drawPanel(args, PANEL_W);
		ModuleWidget::draw(args);
		drawCpuMeterText(args, static_cast<CyberlinePtBreakout*>(module));
	}
};

} // namespace cyberline

namespace cyberline {
rack::plugin::Model* modelCyberlinePtBreakout =
	rack::createModel<CyberlinePtBreakout, CyberlinePtBreakoutWidget>("cyberline-pt-breakout");
} // namespace cyberline
