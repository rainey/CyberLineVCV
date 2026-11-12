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

// =====================================================================================
// BROCA² LAYOUT — all widget positions live here. Edit these freely; the
// constructor and drawPanel only reference these constants. Panel is 300 px
// wide (20 HP) x 380 px tall (RACK_GRID_HEIGHT); the bottom half is split into
// left INPUTS and right OUTPUTS boxes sharing two jack rows (y=292, y=338),
// freeing vertical space for the knob row (y=236) and upper-right editor.
// =====================================================================================
namespace Layout {
	// 4x4 register grid HUD (left side)
	constexpr float GRID_X = 16.f,  GRID_Y = 24.f,  GRID_S = 142.f;

	// Register editor (upper-right quadrant, x: 164..288, y: 24..204)
	constexpr float PREVIEW_X = 168.f, PREVIEW_Y = 28.f, PREVIEW_S = 28.f;
	constexpr float ENC_V_X = 219.f,  ENC_V_Y = 44.f;    // V SEL (12 detents)
	constexpr float ENC_C_X = 251.f,  ENC_C_Y = 44.f;    // C SEL (21 detents)
	constexpr float MODE_X = 281.f,   MODE_Y = 44.f;
	constexpr float BTN_EDIT_X = 182.f, BTN_EDIT_Y = 84.f;
	constexpr float BTN_LKV_X = 219.f, BTN_LKV_Y = 84.f; // lock vowel
	constexpr float BTN_LKC_X = 251.f, BTN_LKC_Y = 84.f; // lock consonant
	constexpr float ENC_CELL_X = 182.f, ENC_CELL_Y = 128.f;
	constexpr float BTN_COPYL_X = 219.f, BTN_FILL_X = 249.f, BTN_COPYR_X = 279.f;
	constexpr float UTIL_Y = 128.f;                      // CELL + copy/fill row
	constexpr float FAVOR_X = 204.f, FAVOR_Y = 178.f;
	constexpr float ONSET_X = 256.f, ONSET_Y = 178.f;

	// Main knob row (y = KNOB_Y, labels 18 px above)
	constexpr float KNOB_Y = 236.f;
	constexpr float kX[7] = {30.f, 70.f, 110.f, 150.f, 190.f, 230.f, 270.f};

	// Shared bottom jack rows: left box = INPUTS (4x2), right box = OUTPUTS (3+4)
	constexpr float IN1_Y = 292.f, IN2_Y = 338.f;
	constexpr float OUT1_Y = 292.f, OUT2_Y = 338.f;
	constexpr float inX[4] = {25.f, 59.f, 93.f, 127.f};
	constexpr float outX1[3] = {180.f, 224.f, 268.f};        // TRACT, LARYNX, GATE
	constexpr float outX2[4] = {173.f, 207.f, 241.f, 275.f}; // ENV, BURST, VOW, CONS

	// Outlined box bounds for INPUTS (left) and OUTPUTS (right)
	constexpr float IN_BOX_X0 = 7.f,   IN_BOX_X1 = 145.f;
	constexpr float OUT_BOX_X0 = 155.f, OUT_BOX_X1 = 293.f;
	constexpr float JACK_BOX_Y0 = 264.f, JACK_BOX_Y1 = 356.f;

	// Label anchors (text draws are at these exact coords, matching the
	// widgets they name)
	constexpr float EDITOR_LABEL_Y1 = 26.f;   // V SEL / C SEL / MODE
	constexpr float EDITOR_LABEL_Y2 = 70.f;   // EDIT / LK-V / LK-C
	constexpr float UTIL_LABEL_Y = 112.f;     // CELL / CPY arrows / FILL
	constexpr float CHAR_LABEL_Y = 160.f;     // FAVOR / ONSET s
	constexpr float KNOB_LABEL_Y = 218.f;
}

struct CyberlineBrocaSqWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 20 * 15.f; // 300 px (20 HP)
	static constexpr float KNOB_Y = Layout::KNOB_Y;
	static constexpr float kX[7] = {Layout::kX[0], Layout::kX[1], Layout::kX[2], Layout::kX[3], Layout::kX[4], Layout::kX[5], Layout::kX[6]};

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

	// Momentary utility button drawn as a small square key (grid-cell
	// visual language): dark plate, thin rim, cyan dot flash while held.
	// Momentary semantics are implemented here: ParamWidget has no
	// click-to-set behavior of its own (Switch does, via onDragStart/
	// onDragEnd); press sets the param to max, release back to min, and
	// the engine edge-detects in process().
	struct SqButton : rack::app::ParamWidget {
		SqButton() {
			box.size = rack::Vec(18.f, 18.f);
		}

		void onButton(const rack::event::Button& e) override {
			ParamWidget::onButton(e);
			if (e.button != GLFW_MOUSE_BUTTON_LEFT || !module)
				return;
			auto* pq = getParamQuantity();
			if (!pq)
				return;
			if (e.action == GLFW_PRESS && (e.mods & RACK_MOD_MASK) == 0) {
				pq->setValue(pq->getMaxValue());
				e.consume(this);
			}
		}

		void onDragEnd(const rack::event::DragEnd& e) override {
			ParamWidget::onDragEnd(e);
			if (e.button != GLFW_MOUSE_BUTTON_LEFT)
				return;
			if (auto* pq = getParamQuantity())
				pq->setValue(pq->getMinValue());
		}

		void draw(const DrawArgs& args) override {
			float s = box.size.x;
			bool on = getParamQuantity() && getParamQuantity()->getValue() > 0.f;
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, 0, 0, s, s, 3.f);
			nvgFillColor(args.vg, nvgRGB(0x11, 0x14, 0x1c));
			nvgFill(args.vg);
			nvgStrokeWidth(args.vg, on ? 1.2f : 0.8f);
			nvgStrokeColor(args.vg, on
				? nvgRGB(0x58, 0xe0, 0xa8)
				: nvgRGB(0x2c, 0x32, 0x40));
			nvgStroke(args.vg);
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, s * 0.5f, s * 0.5f, on ? 2.6f : 1.8f);
			nvgFillColor(args.vg, on
				? nvgRGB(0x58, 0xe0, 0xa8)
				: nvgRGB(0x44, 0x4c, 0x56));
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

				// Editor selection: neon mint outline around the selected
				// cell — bright while the editor is armed, dim otherwise
				if (k == edPos) {
					nvgBeginPath(vg);
					nvgRoundedRect(vg, x - 1.f, y - 1.f, cw + 2.f, chh + 2.f, 4.f);
					nvgStrokeWidth(vg, edOn ? 1.5f : 1.f);
					nvgStrokeColor(vg, nvgRGBA(0x58, 0xe0, 0xa8, edOn ? 255 : 70));
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

		float rIn1 = Layout::IN1_Y, rIn2 = Layout::IN2_Y;
		float rOut1 = Layout::OUT1_Y, rOut2 = Layout::OUT2_Y;

		// Chamfered wireframe boxes around INPUTS (left) and OUTPUTS (right)
		auto drawJackBox = [&](float bx0, float bx1, float by0, float by1, NVGcolor strokeCol) {
			float bch = 5.f;
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
			nvgFillColor(vg, nvgRGBA(0x0b, 0x0d, 0x13, 125));
			nvgFill(vg);
			nvgStrokeWidth(vg, 0.95f);
			nvgStrokeColor(vg, strokeCol);
			nvgStroke(vg);
		};
		drawJackBox(Layout::IN_BOX_X0, Layout::IN_BOX_X1, Layout::JACK_BOX_Y0, Layout::JACK_BOX_Y1,
			nvgRGBA(0x8e, 0x62, 0xf8, 105));
		drawJackBox(Layout::OUT_BOX_X0, Layout::OUT_BOX_X1, Layout::JACK_BOX_Y0, Layout::JACK_BOX_Y1,
			nvgRGBA(0x48, 0xd8, 0xf8, 105));

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
		drawPortRing(Layout::inX[0], rIn1, neonMint,   false, false);
		drawPortRing(Layout::inX[1], rIn1, neonMint,   false, false);
		drawPortRing(Layout::inX[2], rIn1, neonCyan,   false, false);
		drawPortRing(Layout::inX[3], rIn1, neonAmber,  false, false);
		// Inputs row 2: P-V, P-C, MORPH, LEN
		drawPortRing(Layout::inX[0], rIn2, neonCyan,   false, false);
		drawPortRing(Layout::inX[1], rIn2, neonAmber,  false, false);
		drawPortRing(Layout::inX[2], rIn2, neonViolet, false, false);
		drawPortRing(Layout::inX[3], rIn2, neonMint,   false, false);
		// Outputs row 1: TRACT POLY (violet, poly), LARYNX POLY (coral, poly), GATE (mint)
		drawPortRing(Layout::outX1[0], rOut1, neonViolet, true, true);
		drawPortRing(Layout::outX1[1], rOut1, neonCoral,  true, true);
		drawPortRing(Layout::outX1[2], rOut1, neonMint,   true, false);
		// Outputs row 2: CVENV (cyan), BURST (amber), VOW CV (cyan), CONS CV (amber)
		drawPortRing(Layout::outX2[0], rOut2, neonCyan,   true, false);
		drawPortRing(Layout::outX2[1], rOut2, neonAmber,  true, false);
		drawPortRing(Layout::outX2[2], rOut2, neonCyan,   true, false);
		drawPortRing(Layout::outX2[3], rOut2, neonAmber,  true, false);

		auto font = getLabelFont();
		if (font && font->handle >= 0)
			nvgFontFaceId(vg, font->handle);
		nvgFontSize(vg, 8.f);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);

		auto text = [&](float x, float y, const char* s) {
			nvgText(vg, x, y, s, NULL);
		};

		// Grid HUD title; lane legend under the grid
		float gridCenterX = Layout::GRID_X + Layout::GRID_S * 0.5f;
		nvgFontSize(vg, 7.5f);
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(gridCenterX, 20.f, "REGISTER GRID");

		// Upper-right quadrant: register editor + character knobs + MODE
		nvgFontSize(vg, 6.4f);
		nvgFillColor(vg, label);
		text(Layout::ENC_V_X, Layout::EDITOR_LABEL_Y1, "V SEL");
		text(Layout::ENC_C_X, Layout::EDITOR_LABEL_Y1, "C SEL");
		text(Layout::MODE_X, Layout::EDITOR_LABEL_Y1, "MODE");
		text(Layout::BTN_EDIT_X, Layout::EDITOR_LABEL_Y2, "EDIT");
		text(Layout::BTN_LKV_X, Layout::EDITOR_LABEL_Y2, "LK-V");
		text(Layout::BTN_LKC_X, Layout::EDITOR_LABEL_Y2, "LK-C");
		text(Layout::ENC_CELL_X, Layout::UTIL_LABEL_Y, "CELL");
		text(Layout::BTN_COPYL_X, Layout::UTIL_LABEL_Y, "\xE2\x86\x90 CPY");
		text(Layout::BTN_FILL_X, Layout::UTIL_LABEL_Y, "FILL");
		text(Layout::BTN_COPYR_X, Layout::UTIL_LABEL_Y, "CPY \xE2\x86\x92");
		text(Layout::FAVOR_X, Layout::CHAR_LABEL_Y, "FAVOR");
		text(Layout::ONSET_X, Layout::CHAR_LABEL_Y, "ONSET s");

		// Register edit preview: the pair currently under the editor,
		// drawn like an enlarged grid cell at (PREVIEW_X, PREVIEW_Y)
		{
			int ep = module ? module->editPos : 0;
			bool ea = module && module->editActive;
			uint8_t pv = module ? module->vowelReg[ep] : 0;
			uint8_t pc = module ? module->consReg[ep] : 0;
			float px = Layout::PREVIEW_X, py = Layout::PREVIEW_Y, ps = Layout::PREVIEW_S;
			nvgBeginPath(vg);
			nvgRoundedRect(vg, px, py, ps, ps, 3.f);
			nvgFillColor(vg, nvgRGB(0x11, 0x14, 0x1c));
			nvgFill(vg);
			nvgStrokeWidth(vg, ea ? 1.4f : 1.f);
			nvgStrokeColor(vg, ea ? nvgRGB(0x58, 0xe0, 0xa8)
				: nvgRGBA(0x58, 0xe0, 0xa8, 70));
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

		// Knob row labels
		nvgFontSize(vg, 8.f);
		nvgFillColor(vg, label);
		text(kX[0], Layout::KNOB_LABEL_Y, "RATE");
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(kX[1], Layout::KNOB_LABEL_Y, "MUT-V");
		text(kX[2], Layout::KNOB_LABEL_Y, "V LEN");
		nvgFillColor(vg, nvgRGB(0xf8, 0xc0, 0x78));
		text(kX[3], Layout::KNOB_LABEL_Y, "C LEN");
		text(kX[5], Layout::KNOB_LABEL_Y, "MUT-C");
		nvgFillColor(vg, label);
		text(kX[4], Layout::KNOB_LABEL_Y, "MORPH");
		text(kX[6], Layout::KNOB_LABEL_Y, "STEP");

		// Lane legend under the grid
		nvgFontSize(vg, 6.2f);
		nvgFillColor(vg, label);
		text(gridCenterX, Layout::GRID_Y + Layout::GRID_S + 13.f, "TOP: VOWEL \xC2\xB7 BOTTOM: CONSONANT");

		// Input / Output box section headers
		nvgFontSize(vg, 6.2f);
		nvgFillColor(vg, nvgRGBA(0xb8, 0x9c, 0xff, 195));
		text((Layout::IN_BOX_X0 + Layout::IN_BOX_X1) * 0.5f, Layout::JACK_BOX_Y0 - 2.5f, "INPUTS");
		nvgFillColor(vg, nvgRGBA(0x78, 0xe0, 0xfc, 195));
		text((Layout::OUT_BOX_X0 + Layout::OUT_BOX_X1) * 0.5f, Layout::JACK_BOX_Y0 - 2.5f, "OUTPUTS");

		nvgFontSize(vg, 6.2f);
		// Input row 1 labels
		nvgFillColor(vg, nvgRGB(0x80, 0xe8, 0xb8));
		text(Layout::inX[0], rIn1 - 16.5f, "CLK");
		text(Layout::inX[1], rIn1 - 16.5f, "RST");
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(Layout::inX[2], rIn1 - 16.5f, "V-PH");
		nvgFillColor(vg, nvgRGB(0xf8, 0xc0, 0x78));
		text(Layout::inX[3], rIn1 - 16.5f, "C-PH");
		// Input row 2 labels
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(Layout::inX[0], rIn2 - 16.5f, "P-V");
		nvgFillColor(vg, nvgRGB(0xf8, 0xc0, 0x78));
		text(Layout::inX[1], rIn2 - 16.5f, "P-C");
		nvgFillColor(vg, nvgRGB(0xd0, 0xb0, 0xff));
		text(Layout::inX[2], rIn2 - 16.5f, "MORPH");
		nvgFillColor(vg, nvgRGB(0x80, 0xe8, 0xb8));
		text(Layout::inX[3], rIn2 - 16.5f, "LEN");

		// Output row 1 labels: TRACT, LARYNX, GATE
		nvgFillColor(vg, nvgRGB(0xd0, 0xb0, 0xff));
		text(Layout::outX1[0], rOut1 - 17.5f, "TRACT");
		nvgFillColor(vg, nvgRGB(0xf8, 0x90, 0x98));
		text(Layout::outX1[1], rOut1 - 17.5f, "LARYNX");
		nvgFillColor(vg, nvgRGB(0x80, 0xe8, 0xb8));
		text(Layout::outX1[2], rOut1 - 16.5f, "GATE");
		// Output row 2 labels: ENV, BURST, VOW, CONS
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(Layout::outX2[0], rOut2 - 16.5f, "ENV");
		nvgFillColor(vg, nvgRGB(0xf8, 0xc0, 0x78));
		text(Layout::outX2[1], rOut2 - 16.5f, "BURST");
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(Layout::outX2[2], rOut2 - 16.5f, "VOW");
		nvgFillColor(vg, nvgRGB(0xf8, 0xc0, 0x78));
		text(Layout::outX2[3], rOut2 - 16.5f, "CONS");

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
			grid->box.pos = rack::Vec(Layout::GRID_X, Layout::GRID_Y);
			grid->box.size = rack::Vec(Layout::GRID_S, Layout::GRID_S);
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
		editVEnc->box.pos = rack::Vec(Layout::ENC_V_X, Layout::ENC_V_Y) - editVEnc->box.size * 0.5f;
		addParam(editVEnc);

		auto* editCEnc = new WrapEncoderKnob(21);
		editCEnc->module = module;
		editCEnc->paramId = CyberlineBrocaSq::EDIT_C_PARAM;
		editCEnc->autoArmFn = [m = module]() { if (m) m->editActive = true; };
		editCEnc->box.pos = rack::Vec(Layout::ENC_C_X, Layout::ENC_C_Y) - editCEnc->box.size * 0.5f;
		addParam(editCEnc);

		addParam(rack::createParamCentered<VerticalSnapSwitch>(rack::Vec(Layout::MODE_X, Layout::MODE_Y), module, CyberlineBrocaSq::MODE_PARAM));
		addParam(rack::createParamCentered<rack::componentlibrary::VCVButton>(rack::Vec(Layout::BTN_LKV_X, Layout::BTN_LKV_Y), module, CyberlineBrocaSq::LKV_BUT_PARAM));
		addParam(rack::createParamCentered<rack::componentlibrary::VCVButton>(rack::Vec(Layout::BTN_LKC_X, Layout::BTN_LKC_Y), module, CyberlineBrocaSq::LKC_BUT_PARAM));
		addParam(rack::createParamCentered<rack::componentlibrary::VCVButton>(rack::Vec(Layout::BTN_EDIT_X, Layout::BTN_EDIT_Y), module, CyberlineBrocaSq::EDIT_BUT_PARAM));
		// Lock/edit indicator lights (drawn after the buttons so the dot
		// sits on top): green = that lane's cell is locked, yellow =
		// editor armed
		addChild(rack::createLightCentered<rack::componentlibrary::SmallLight<rack::componentlibrary::GreenLight>>(
			rack::Vec(Layout::BTN_LKV_X, Layout::BTN_LKV_Y), module, CyberlineBrocaSq::LKV_LIGHT));
		addChild(rack::createLightCentered<rack::componentlibrary::SmallLight<rack::componentlibrary::GreenLight>>(
			rack::Vec(Layout::BTN_LKC_X, Layout::BTN_LKC_Y), module, CyberlineBrocaSq::LKC_LIGHT));
		addChild(rack::createLightCentered<rack::componentlibrary::SmallLight<rack::componentlibrary::YellowLight>>(
			rack::Vec(Layout::BTN_EDIT_X, Layout::BTN_EDIT_Y), module, CyberlineBrocaSq::EDIT_LIGHT));

		auto* editPosEnc = new WrapEncoderKnob(16);
		editPosEnc->module = module;
		editPosEnc->paramId = CyberlineBrocaSq::EDIT_POS_PARAM;
		editPosEnc->box.pos = rack::Vec(Layout::ENC_CELL_X, Layout::ENC_CELL_Y) - editPosEnc->box.size * 0.5f;
		addParam(editPosEnc);

		// Utility row: copy-left / fill / copy-right (square-key buttons)
		addParam(rack::createParamCentered<SqButton>(rack::Vec(Layout::BTN_COPYL_X, Layout::UTIL_Y), module, CyberlineBrocaSq::COPYL_BUT_PARAM));
		addParam(rack::createParamCentered<SqButton>(rack::Vec(Layout::BTN_FILL_X, Layout::UTIL_Y), module, CyberlineBrocaSq::FILL_BUT_PARAM));
		addParam(rack::createParamCentered<SqButton>(rack::Vec(Layout::BTN_COPYR_X, Layout::UTIL_Y), module, CyberlineBrocaSq::COPYR_BUT_PARAM));

		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(Layout::FAVOR_X, Layout::FAVOR_Y), module, CyberlineBrocaSq::FAVOR_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(Layout::ONSET_X, Layout::ONSET_Y), module, CyberlineBrocaSq::ONSET_PARAM));

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
		float rIn1 = Layout::IN1_Y;
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(Layout::inX[0],  rIn1), module, CyberlineBrocaSq::CLOCK_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(Layout::inX[1],  rIn1), module, CyberlineBrocaSq::RESET_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(Layout::inX[2], rIn1), module, CyberlineBrocaSq::VPH_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(Layout::inX[3], rIn1), module, CyberlineBrocaSq::CPH_INPUT));
		// Inputs row 2: P-V, P-C, MORPH, LEN
		float rIn2 = Layout::IN2_Y;
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(Layout::inX[0],  rIn2), module, CyberlineBrocaSq::PV_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(Layout::inX[1],  rIn2), module, CyberlineBrocaSq::PC_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(Layout::inX[2], rIn2), module, CyberlineBrocaSq::MORPH_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(Layout::inX[3], rIn2), module, CyberlineBrocaSq::LEN_INPUT));

		// Outputs row 1: TRACT, LARYNX, GATE
		float rOut1 = Layout::OUT1_Y, rOut2 = Layout::OUT2_Y;
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(Layout::outX1[0], rOut1), module, CyberlineBrocaSq::TRACT_POLY_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(Layout::outX1[1], rOut1), module, CyberlineBrocaSq::LARYNX_POLY_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(Layout::outX1[2], rOut1), module, CyberlineBrocaSq::GATE_OUTPUT));
		// Outputs row 2: ENV, BURST, VOW, CONS
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(Layout::outX2[0], rOut2), module, CyberlineBrocaSq::CVENV_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(Layout::outX2[1], rOut2), module, CyberlineBrocaSq::BURST_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(Layout::outX2[2], rOut2), module, CyberlineBrocaSq::VOWCV_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(Layout::outX2[3], rOut2), module, CyberlineBrocaSq::CONSCV_OUTPUT));
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
