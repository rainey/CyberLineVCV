// CyberlineBrocaSqWidget — programmatic 20 HP hollow neon vector panel for
// "BROCA²" (Broca-Squared dual phoneme Turing machine). Panel styling follows
// the Broca/Lungs family: flat dark plate, violet neon top bar, chamfered
// wireframe input brackets, neon port rings, and a 4x4 register grid HUD
// showing each step's vowel/consonant pair with per-lane heads.

#include "CyberlineBrocaSq.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>
#include <cstdio>

namespace cyberline {

struct CyberlineBrocaSqWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 20 * 15.f; // 300 px (20 HP)
	static constexpr float KNOB_Y = 200.f;
	static constexpr float kX[7] = {30.f, 70.f, 110.f, 150.f, 190.f, 230.f, 270.f};

	static void strokeNeon(NVGcontext* vg, NVGcolor col, float coreW = 1.2f, float glowW = 3.4f, int glowAlpha = 50) {
		nvgStrokeWidth(vg, glowW);
		nvgStrokeColor(vg, nvgRGBA(col.r * 255, col.g * 255, col.b * 255, glowAlpha));
		nvgStroke(vg);
		nvgStrokeWidth(vg, coreW);
		nvgStrokeColor(vg, col);
		nvgStroke(vg);
	}

	// 3-position snap switch for clocking mode (Lungs VerticalSnapSwitch style).
	struct VerticalSnapSwitch : rack::app::ParamWidget {
		static constexpr int NumSlots = 3;

		VerticalSnapSwitch() {
			// Without a size the widget renders at 0x0: invisible and
			// unclickable (createParamCentered uses box.size for centering
			// and the hitbox). Same geometry as Lungs ModeSwitch.
			box.size = rack::Vec(18.f, 34.f);
		}

		float pressY = 0.f;
		float oldValue = 0.f;
		float dragDeltaY = 0.f;
		bool dragged = false;

		float slotY(int i) const {
			return 8.f + i * (box.size.y - 16.f) / (float) (NumSlots - 1);
		}

		void initParamQuantity() override {
			ParamWidget::initParamQuantity();
			if (auto* pq = getParamQuantity()) {
				pq->snapEnabled = true;
				pq->smoothEnabled = false;
			}
		}

		void onButton(const rack::event::Button& e) override {
			if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT)
				pressY = e.pos.y;
			ParamWidget::onButton(e);
		}

		void onDragStart(const rack::event::DragStart& e) override {
			ParamWidget::onDragStart(e);
			if (e.button != GLFW_MOUSE_BUTTON_LEFT)
				return;
			if (auto* pq = getParamQuantity())
				oldValue = std::round(pq->getValue());
			dragDeltaY = 0.f;
			dragged = false;
		}

		void onDragMove(const rack::event::DragMove& e) override {
			ParamWidget::onDragMove(e);
			if (e.button != GLFW_MOUSE_BUTTON_LEFT)
				return;
			auto* pq = getParamQuantity();
			if (!pq)
				return;
			dragDeltaY += e.mouseDelta.y;
			if (std::fabs(dragDeltaY) >= 2.5f)
				dragged = true;
			if (dragged) {
				float span = std::max(8.f, slotY(NumSlots - 1) - slotY(0));
				float stepPx = std::max(6.f, span / (float) (NumSlots - 1));
				float target = std::round(oldValue + dragDeltaY / stepPx);
				pq->setValue(rack::math::clamp(target, pq->getMinValue(), pq->getMaxValue()));
			}
		}

		void onDragEnd(const rack::event::DragEnd& e) override {
			ParamWidget::onDragEnd(e);
			if (e.button != GLFW_MOUSE_BUTTON_LEFT)
				return;
			auto* pq = getParamQuantity();
			if (!pq)
				return;
			if (!dragged) {
				int curSlot = (int) std::round(oldValue);
				int nearest = 0;
				float bestDist = std::fabs(pressY - slotY(0));
				for (int i = 1; i < NumSlots; i++) {
					float d = std::fabs(pressY - slotY(i));
					if (d < bestDist) {
						bestDist = d;
						nearest = i;
					}
				}
				int nextSlot = (nearest != curSlot)
					? nearest
					: ((curSlot + 1) % NumSlots);
				pq->setValue((float) nextSlot);
			}
			float newValue = std::round(pq->getValue());
			if (module && oldValue != newValue) {
				auto* h = new rack::history::ParamChange;
				h->name = "move switch";
				h->moduleId = module->id;
				h->paramId = paramId;
				h->oldValue = oldValue;
				h->newValue = newValue;
				APP->history->push(h);
			}
		}

		void draw(const DrawArgs& args) override {
			float w = box.size.x, h = box.size.y;
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, w * 0.5f - 3.5f, 2.f, 7.f, h - 4.f, 3.5f);
			nvgFillColor(args.vg, nvgRGB(0x0d, 0x0f, 0x12));
			nvgFill(args.vg);
			nvgStrokeWidth(args.vg, 1.f);
			nvgStrokeColor(args.vg, nvgRGB(0x2a, 0x2e, 0x35));
			nvgStroke(args.vg);
			for (int i = 0; i < NumSlots; i++) {
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, w * 0.5f, slotY(i), 1.2f);
				nvgFillColor(args.vg, nvgRGB(0x44, 0x4c, 0x56));
				nvgFill(args.vg);
			}
			float v = getParamQuantity() ? getParamQuantity()->getValue() : 0.f;
			int pos = (int) std::round(rack::math::clamp(v, 0.f, (float) (NumSlots - 1)));
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, 1.f, slotY(pos) - 5.f, w - 2.f, 10.f, 4.f);
			nvgFillColor(args.vg, pos == CyberlineBrocaSq::MODE_PHASOR
				? nvgRGB(0x48, 0xd8, 0xf8)
				: nvgRGB(0x5a, 0xc8, 0xe0));
			nvgFill(args.vg);
		}
	};

	// 4x4 register grid: each cell shows the vowel code (top, cyan lane) and
	// consonant code (bottom, amber lane) of one slot; per-lane dimming past
	// that register's length, cyan/amber head markers at the active slots.
	struct RegisterGrid : rack::widget::Widget {
		CyberlineBrocaSq* module;
		RegisterGrid(CyberlineBrocaSq* m) : module(m) {}

		void draw(const DrawArgs& args) override {
			float w = box.size.x, h = box.size.y;
			NVGcontext* vg = args.vg;
			auto font = getLabelFont();
			if (font && font->handle >= 0)
				nvgFontFaceId(vg, font->handle);

			int vLen = module ? module->telVLen : 8;
			int cLen = module ? module->telCLen : 8;
			int vPos = module ? module->telVPos : 0;
			int cPos = module ? module->telCPos : 0;
			bool edOn = module && module->telEditActive;
			int edPos = module ? module->telEditPos : -1;
			uint32_t vLocks = module ? module->telVLocks : 0;
			uint32_t cLocks = module ? module->telCLocks : 0;

			float gap = 3.f;
			float cw = (w - gap * 3.f) / 4.f;
			float chh = (h - gap * 3.f) / 4.f;

			for (int k = 0; k < 16; k++) {
				int row = k / 4, col = k % 4;
				float x = col * (cw + gap);
				float y = row * (chh + gap);
				bool vOn = k < vLen;
				bool cOn = k < cLen;
				bool bothOff = (!vOn && !cOn);

				// Cell plate
				nvgBeginPath(vg);
				nvgRoundedRect(vg, x, y, cw, chh, 3.f);
				nvgFillColor(vg, bothOff
					? nvgRGB(0x0a, 0x0c, 0x10)
					: nvgRGB(0x11, 0x14, 0x1c));
				nvgFill(vg);
				nvgStrokeWidth(vg, 0.8f);
				nvgStrokeColor(vg, bothOff
					? nvgRGB(0x1c, 0x20, 0x28)
					: nvgRGB(0x2c, 0x32, 0x40));
				nvgStroke(vg);

				uint8_t vIdx = module ? module->vowelReg[k] : 0;
				uint8_t cIdx = module ? module->consReg[k] : 0;
				const char* vCode = module
					? CyberlineBroca::VOWELS[vIdx].code : "EE";
				const char* cCode = module
					? CyberlineBroca::CONSONANTS[cIdx].code : "-";

				// Vowel code (top, cyan lane)
				float vAlpha = vOn ? 1.f : 0.22f;
				nvgFontSize(vg, 9.5f);
				nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
				nvgFillColor(vg, nvgRGBA(0x8c, 0xe4, 0xfc,
					(int) (230 * vAlpha)));
				nvgText(vg, x + cw * 0.5f, y + 3.5f, vCode, NULL);

				// Consonant code (bottom, amber lane); NONE reads as a dim dash
				float cAlpha = cOn ? 1.f : 0.22f;
				nvgFontSize(vg, cIdx == 0 ? 8.f : 9.5f);
				nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
				nvgFillColor(vg, cIdx == 0
					? nvgRGBA(0x70, 0x7c, 0x88, (int) (200 * cAlpha))
					: nvgRGBA(0xf4, 0xb0, 0x48, (int) (230 * cAlpha)));
				nvgText(vg, x + cw * 0.5f, y + chh - 3.f, cCode, NULL);

				// Head markers: cyan dot for the vowel head, amber for the
				// consonant head (left edge); overlap = both drawn
				float cy = y + chh * 0.5f;
				if (vPos == k && vOn) {
					nvgBeginPath(vg);
					nvgCircle(vg, x + 3.5f, cy - 3.5f, 1.6f);
					nvgFillColor(vg, nvgRGB(0x48, 0xd8, 0xf8));
					nvgFill(vg);
				}
				if (cPos == k && cOn) {
					nvgBeginPath(vg);
					nvgCircle(vg, x + 3.5f, cy + 3.5f, 1.6f);
					nvgFillColor(vg, nvgRGB(0xf4, 0xb0, 0x48));
					nvgFill(vg);
				}

				// Mutation-lock markers (violet, right edge; upper half =
				// vowel lock, lower half = consonant lock)
				if (vLocks & (1u << k)) {
					nvgBeginPath(vg);
					nvgCircle(vg, x + cw - 4.f, cy - 3.5f, 1.8f);
					nvgFillColor(vg, nvgRGB(0x9b, 0x6d, 0xff));
					nvgFill(vg);
				}
				if (cLocks & (1u << k)) {
					nvgBeginPath(vg);
					nvgCircle(vg, x + cw - 4.f, cy + 3.5f, 1.8f);
					nvgFillColor(vg, nvgRGB(0x9b, 0x6d, 0xff));
					nvgFill(vg);
				}

				// Editor highlight: neon mint outline around the cell
				// being edited (armed state)
				if (edOn && k == edPos) {
					nvgBeginPath(vg);
					nvgRoundedRect(vg, x - 1.f, y - 1.f, cw + 2.f, chh + 2.f, 4.f);
					nvgStrokeWidth(vg, 1.5f);
					nvgStrokeColor(vg, nvgRGB(0x58, 0xe0, 0xa8));
					nvgStroke(vg);
				}
			}
		}
	};

	static void drawPanel(const rack::widget::Widget::DrawArgs& args, float panelW, CyberlineBrocaSq* module) {
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
		nvgFillColor(vg, neonViolet);
		nvgFill(vg);
		nvgBeginPath(vg);
		nvgMoveTo(vg, 0, 7.5f);
		nvgLineTo(vg, panelW, 7.5f);
		nvgStrokeWidth(vg, 0.8f);
		nvgStrokeColor(vg, nvgRGBA(0x9b, 0x6d, 0xff, 130));
		nvgStroke(vg);

		float rIn1 = 252.f, rIn2 = 294.f, rOut = 338.f;

		// Chamfered wireframe brackets around both input rows
		for (float rIn : {rIn1, rIn2}) {
			float bx0 = 8.f, bx1 = panelW - 8.f;
			float by0 = rIn - 18.f, by1 = rIn + 16.5f;
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
			nvgStrokeColor(vg, nvgRGBA(0x8e, 0x62, 0xf8, 95));
			nvgStroke(vg);
		}

		auto drawPortRing = [&](float x, float y, NVGcolor col, bool isOutput, bool isPoly) {
			nvgBeginPath(vg);
			nvgCircle(vg, x, y, isOutput ? 14.8f : 13.4f);
			if (isOutput) {
				nvgFillColor(vg, nvgRGB(0x09, 0x0b, 0x10));
				nvgFill(vg);
				strokeNeon(vg, col, isPoly ? 1.35f : 1.05f, isPoly ? 3.6f : 2.8f, isPoly ? 60 : 40);
				if (isPoly) {
					nvgBeginPath(vg);
					nvgCircle(vg, x, y, 16.6f);
					nvgStrokeWidth(vg, 0.8f);
					nvgStrokeColor(vg, nvgRGBA(col.r * 255, col.g * 255, col.b * 255, 130));
					nvgStroke(vg);
				}
			} else {
				strokeNeon(vg, col, 0.9f, 2.3f, 30);
			}
		};

		// Port rings (color-coded like Broca: cyan = vowel lane, amber =
		// consonant lane, mint = clock/reset, violet = poly buses)
		// Inputs row 1: CLOCK, RESET, V-PH, C-PH
		drawPortRing(30.f,  rIn1, neonMint,  false, false);
		drawPortRing(68.f,  rIn1, neonMint,  false, false);
		drawPortRing(106.f, rIn1, neonCyan,  false, false);
		drawPortRing(144.f, rIn1, neonAmber, false, false);
		// Inputs row 2: P-V, P-C, MORPH, LEN
		drawPortRing(30.f,  rIn2, neonCyan,  false, false);
		drawPortRing(68.f,  rIn2, neonAmber, false, false);
		drawPortRing(106.f, rIn2, neonViolet, false, false);
		drawPortRing(144.f, rIn2, neonMint,  false, false);
		// Outputs: TRACT POLY (violet, poly), LARYNX POLY (coral, poly),
		// CVENV (cyan), BURST (amber), GATE (mint), VOW CV (cyan), CONS CV (amber)
		float outX[7] = {26.f, 64.f, 102.f, 140.f, 178.f, 220.f, 262.f};
		drawPortRing(outX[0], rOut, neonViolet, true, true);
		drawPortRing(outX[1], rOut, neonCoral,  true, true);
		drawPortRing(outX[2], rOut, neonCyan,   true, false);
		drawPortRing(outX[3], rOut, neonAmber,  true, false);
		drawPortRing(outX[4], rOut, neonMint,   true, false);
		drawPortRing(outX[5], rOut, neonCyan,   true, false);
		drawPortRing(outX[6], rOut, neonAmber,  true, false);

		auto font = getLabelFont();
		if (font && font->handle >= 0)
			nvgFontFaceId(vg, font->handle);
		nvgFontSize(vg, 8.f);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);

		auto text = [&](float x, float y, const char* s) {
			nvgText(vg, x, y, s, NULL);
		};

		// Grid HUD title; lane legend moved under the grid
		nvgFontSize(vg, 7.5f);
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(90.f, 20.f, "REGISTER GRID");

		// Right column: register editor + character knobs + MODE.
		// Columns x: 178 (preview/EDIT), 218 & 250 (encoders), 276 (MODE)
		nvgFontSize(vg, 6.4f);
		nvgFillColor(vg, label);
		text(218.f, 26.f, "V SEL");
		text(250.f, 26.f, "C SEL");
		text(276.f, 26.f, "MODE");
		text(218.f, 62.f, "LK-V");
		text(250.f, 62.f, "LK-C");
		text(178.f, 92.f, "EDIT");
		text(218.f, 92.f, "CELL");
		text(206.f, 140.f, "FAVOR");
		text(252.f, 140.f, "ONSET s");

		// Register edit preview: the pair currently under the editor,
		// drawn like an enlarged grid cell at (178, 42) size 28
		{
			int ep = module ? module->editPos : 0;
			bool ea = module && module->editActive;
			uint8_t pv = module ? module->vowelReg[ep] : 0;
			uint8_t pc = module ? module->consReg[ep] : 0;
			float px = 164.f, py = 28.f, ps = 28.f;
			nvgBeginPath(vg);
			nvgRoundedRect(vg, px, py, ps, ps, 3.f);
			nvgFillColor(vg, nvgRGB(0x11, 0x14, 0x1c));
			nvgFill(vg);
			nvgStrokeWidth(vg, ea ? 1.4f : 0.9f);
			nvgStrokeColor(vg, ea ? nvgRGB(0x58, 0xe0, 0xa8) : nvgRGB(0x2c, 0x32, 0x40));
			nvgStroke(vg);
			auto pfont = getLabelFont();
			if (pfont && pfont->handle >= 0) {
				nvgFontFaceId(vg, pfont->handle);
				nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
				nvgFontSize(vg, 8.5f);
				nvgFillColor(vg, nvgRGBA(0x8c, 0xe4, 0xfc, ea ? 240 : 150));
				nvgText(vg, px + ps * 0.5f, py + 3.f,
					CyberlineBroca::VOWELS[pv].code, NULL);
				nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
				nvgFontSize(vg, pc == 0 ? 7.5f : 8.5f);
				nvgFillColor(vg, pc == 0
					? nvgRGBA(0x70, 0x7c, 0x88, 170)
					: nvgRGBA(0xf4, 0xb0, 0x48, ea ? 240 : 150));
				nvgText(vg, px + ps * 0.5f, py + ps - 2.5f,
					CyberlineBroca::CONSONANTS[pc].code, NULL);
			}
		}

		// Knob row labels (knobs at KNOB_Y=200)
		nvgFontSize(vg, 8.f);
		nvgFillColor(vg, label);
		text(kX[0], 182.f, "RATE");
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(kX[1], 182.f, "MUT-V");
		text(kX[2], 182.f, "V LEN");
		nvgFillColor(vg, nvgRGB(0xf8, 0xc0, 0x78));
		text(kX[3], 182.f, "C LEN");
		text(kX[5], 182.f, "MUT-C");
		nvgFillColor(vg, label);
		text(kX[4], 182.f, "MORPH");
		text(kX[6], 182.f, "STEP");

		// Lane legend (moved under the grid to free the editor area)
		nvgFontSize(vg, 6.2f);
		nvgFillColor(vg, label);
		text(90.f, 172.f, "TOP: VOWEL \xC2\xB7 BOTTOM: CONSONANT");

		nvgFontSize(vg, 6.2f);
		// Input row 1 labels
		nvgFillColor(vg, nvgRGB(0x80, 0xe8, 0xb8));
		text(30.f,  rIn1 - 17.f, "CLK");
		text(68.f,  rIn1 - 17.f, "RST");
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(106.f, rIn1 - 17.f, "V-PH");
		nvgFillColor(vg, nvgRGB(0xf8, 0xc0, 0x78));
		text(144.f, rIn1 - 17.f, "C-PH");
		// Input row 2 labels
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(30.f,  rIn2 - 17.f, "P-V");
		nvgFillColor(vg, nvgRGB(0xf8, 0xc0, 0x78));
		text(68.f,  rIn2 - 17.f, "P-C");
		nvgFillColor(vg, nvgRGB(0xd0, 0xb0, 0xff));
		text(106.f, rIn2 - 17.f, "MORPH");
		nvgFillColor(vg, nvgRGB(0x80, 0xe8, 0xb8));
		text(144.f, rIn2 - 17.f, "LEN");
		// Output labels
		nvgFontSize(vg, 6.2f);
		nvgFillColor(vg, nvgRGB(0xd0, 0xb0, 0xff));
		text(outX[0], rOut - 17.f, "TRACT");
		nvgFillColor(vg, nvgRGB(0xf8, 0x90, 0x98));
		text(outX[1], rOut - 17.f, "LARYNX");
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(outX[2], rOut - 17.f, "ENV");
		nvgFillColor(vg, nvgRGB(0xf8, 0xc0, 0x78));
		text(outX[3], rOut - 17.f, "BURST");
		nvgFillColor(vg, nvgRGB(0x80, 0xe8, 0xb8));
		text(outX[4], rOut - 17.f, "GATE");
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(outX[5], rOut - 17.f, "VOW");
		nvgFillColor(vg, nvgRGB(0xf8, 0xc0, 0x78));
		text(outX[6], rOut - 17.f, "CONS");

		drawPanelTitle(args, panelW, "BROCA\xC2\xB2");
	}

	CyberlineBrocaSqWidget(CyberlineBrocaSq* module) {
		setModule(module);
		box.size = rack::Vec(PANEL_W, rack::app::RACK_GRID_HEIGHT);

		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, rack::app::RACK_GRID_HEIGHT - 15)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, rack::app::RACK_GRID_HEIGHT - 15)));

		using rack::componentlibrary::RoundBlackKnob;
		using rack::componentlibrary::RoundSmallBlackKnob;
		using rack::componentlibrary::PJ301MPort;

		// 4x4 register grid HUD (22..156)
		{
			auto grid = new RegisterGrid(module);
			grid->box.pos = rack::Vec(22.f, 24.f);
			grid->box.size = rack::Vec(136.f, 136.f);
			addChild(grid);
		}

		// Right of the grid: register editor (see layout comment in
		// drawPanel) + the character column, then STEP lands in the free
		// knob-row slot and MODE goes to the top-right corner.
		// NOTE: ModuleWidget::addParam() is just addChild() — unlike
		// createParamCentered it does NOT set module/paramId, so the
		// hand-constructed encoder widgets must set them explicitly
		// (unset module made drags no-ops and right-click segfault the
		// context menu's unchecked pq->getString()).
		auto* editVEnc = new WrapEncoderKnob(12);
		editVEnc->module = module;
		editVEnc->paramId = CyberlineBrocaSq::EDIT_V_PARAM;
		editVEnc->autoArmFn = [m = module]() { if (m) m->editActive = true; };
		editVEnc->box.pos = rack::Vec(218.f, 44.f) - editVEnc->box.size * 0.5f;
		addParam(editVEnc);

		auto* editCEnc = new WrapEncoderKnob(21);
		editCEnc->module = module;
		editCEnc->paramId = CyberlineBrocaSq::EDIT_C_PARAM;
		editCEnc->autoArmFn = [m = module]() { if (m) m->editActive = true; };
		editCEnc->box.pos = rack::Vec(250.f, 44.f) - editCEnc->box.size * 0.5f;
		addParam(editCEnc);

		addParam(rack::createParamCentered<VerticalSnapSwitch>(rack::Vec(276.f, 44.f), module, CyberlineBrocaSq::MODE_PARAM));
		addParam(rack::createParamCentered<rack::componentlibrary::VCVButton>(rack::Vec(218.f, 76.f), module, CyberlineBrocaSq::LKV_BUT_PARAM));
		addParam(rack::createParamCentered<rack::componentlibrary::VCVButton>(rack::Vec(250.f, 76.f), module, CyberlineBrocaSq::LKC_BUT_PARAM));
		addParam(rack::createParamCentered<rack::componentlibrary::VCVButton>(rack::Vec(178.f, 108.f), module, CyberlineBrocaSq::EDIT_BUT_PARAM));
		// Lock/edit indicator lights (drawn after the buttons so the dot
		// sits on top): green = that lane's cell is locked, yellow =
		// editor armed
		addChild(rack::createLightCentered<rack::componentlibrary::SmallLight<rack::componentlibrary::GreenLight>>(
			rack::Vec(218.f, 76.f), module, CyberlineBrocaSq::LKV_LIGHT));
		addChild(rack::createLightCentered<rack::componentlibrary::SmallLight<rack::componentlibrary::GreenLight>>(
			rack::Vec(250.f, 76.f), module, CyberlineBrocaSq::LKC_LIGHT));
		addChild(rack::createLightCentered<rack::componentlibrary::SmallLight<rack::componentlibrary::YellowLight>>(
			rack::Vec(178.f, 108.f), module, CyberlineBrocaSq::EDIT_LIGHT));

		auto* editPosEnc = new WrapEncoderKnob(16);
		editPosEnc->module = module;
		editPosEnc->paramId = CyberlineBrocaSq::EDIT_POS_PARAM;
		editPosEnc->box.pos = rack::Vec(218.f, 108.f) - editPosEnc->box.size * 0.5f;
		addParam(editPosEnc);

		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(206.f, 158.f), module, CyberlineBrocaSq::FAVOR_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(252.f, 158.f), module, CyberlineBrocaSq::ONSET_PARAM));

		// Knob row (labels at y 182, knobs at KNOB_Y=200):
		// RATE, MUT-V, VLEN, CLEN, MORPH, MUT-C  (ONSET moved to the
		// right column beside FAVOR)
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(kX[0], KNOB_Y), module, CyberlineBrocaSq::RATE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(kX[1], KNOB_Y), module, CyberlineBrocaSq::MUTV_PARAM));
		addParam(rack::createParamCentered<LenEncoderKnob>(rack::Vec(kX[2], KNOB_Y), module, CyberlineBrocaSq::VLEN_PARAM));
		addParam(rack::createParamCentered<LenEncoderKnob>(rack::Vec(kX[3], KNOB_Y), module, CyberlineBrocaSq::CLEN_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(kX[4], KNOB_Y), module, CyberlineBrocaSq::MORPH_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(kX[5], KNOB_Y), module, CyberlineBrocaSq::MUTC_PARAM));
		addParam(rack::createParamCentered<rack::componentlibrary::VCVButton>(rack::Vec(kX[6], KNOB_Y), module, CyberlineBrocaSq::STEP_PARAM));

		// Inputs row 1: CLOCK, RESET, V-PH, C-PH
		float rIn1 = 252.f;
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(30.f,  rIn1), module, CyberlineBrocaSq::CLOCK_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(68.f,  rIn1), module, CyberlineBrocaSq::RESET_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(106.f, rIn1), module, CyberlineBrocaSq::VPH_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(144.f, rIn1), module, CyberlineBrocaSq::CPH_INPUT));
		// Inputs row 2: P-V, P-C, MORPH, LEN
		float rIn2 = 294.f;
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(30.f,  rIn2), module, CyberlineBrocaSq::PV_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(68.f,  rIn2), module, CyberlineBrocaSq::PC_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(106.f, rIn2), module, CyberlineBrocaSq::MORPH_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(144.f, rIn2), module, CyberlineBrocaSq::LEN_INPUT));

		// Outputs
		float outX[7] = {26.f, 64.f, 102.f, 140.f, 178.f, 220.f, 262.f};
		float rOut = 338.f;
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[0], rOut), module, CyberlineBrocaSq::TRACT_POLY_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[1], rOut), module, CyberlineBrocaSq::LARYNX_POLY_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[2], rOut), module, CyberlineBrocaSq::CVENV_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[3], rOut), module, CyberlineBrocaSq::BURST_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[4], rOut), module, CyberlineBrocaSq::GATE_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[5], rOut), module, CyberlineBrocaSq::VOWCV_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[6], rOut), module, CyberlineBrocaSq::CONSCV_OUTPUT));
	}

	void appendContextMenu(rack::ui::Menu* menu) override {
		ModuleWidget::appendContextMenu(menu);
		auto* mod = dynamic_cast<CyberlineBrocaSq*>(module);
		menu->addChild(new rack::ui::MenuSeparator);
		menu->addChild(rack::createMenuItem("Initialize registers", "", [=]() {
			if (mod)
				mod->initRegisters();
		}));
		menu->addChild(rack::createMenuItem("Randomize registers", "", [=]() {
			if (mod)
				mod->randomizeRegisters();
		}));
		menu->addChild(new rack::ui::MenuSeparator);
		// BrocaSq has no quality-dependent DSP; expose the plugin-wide global.
		appendQualityMenu(menu, nullptr, -1);
	}

	void draw(const DrawArgs& args) override {
		drawPanel(args, PANEL_W, static_cast<CyberlineBrocaSq*>(module));
		ModuleWidget::draw(args);
		drawCpuMeterText(args, static_cast<CyberlineBrocaSq*>(module));
	}
};

} // namespace cyberline

namespace cyberline {
rack::plugin::Model* modelCyberlineBrocaSq =
	rack::createModel<CyberlineBrocaSq, CyberlineBrocaSqWidget>("cyberline-broca-sq");
} // namespace cyberline
