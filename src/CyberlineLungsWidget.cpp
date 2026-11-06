// CyberlineLungsWidget — programmatic panel (no SVG assets). 14 HP.
// Single column; label 16 px above its widget.

#include "CyberlineLungs.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineLungsWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 14 * 15.f;

	// Helper: neon double-stroke (outer soft bloom + crisp inner vector core)
	static void strokeNeon(NVGcontext* vg, NVGcolor col, float coreW = 1.2f, float glowW = 3.6f, int glowAlpha = 50) {
		nvgStrokeWidth(vg, glowW);
		nvgStrokeColor(vg, nvgRGBA(col.r * 255, col.g * 255, col.b * 255, glowAlpha));
		nvgStroke(vg);
		nvgStrokeWidth(vg, coreW);
		nvgStrokeColor(vg, col);
		nvgStroke(vg);
	}

	// "Lung" gauge: a bilateral human lung + bronchial tree hollow neon wireframe
	// telemetry display in the top header. The lung lobes expand slightly with
	// tidal volume and visibly fill up from base to apex on inhale (neon cyan)
	// and drain down on exhale (neon amber-coral), with an animated diaphragm arc.
	struct BreathGauge : rack::widget::Widget {
		CyberlineLungs* module;
		BreathGauge(CyberlineLungs* m) : module(m) {}

		static void traceRightLung(NVGcontext* vg, float cx, float cy, float exp) {
			// Viewer's left = anatomical Right Lung (3 lobes, broad base)
			nvgMoveTo(vg, cx - 7.f, cy - 19.f);
			// Apical dome
			nvgBezierTo(vg, cx - 12.f * exp, cy - 23.f,
				cx - 22.f * exp, cy - 18.f, cx - 26.f * exp, cy - 7.f);
			// Costal lateral border
			nvgBezierTo(vg, cx - 30.f * exp, cy + 4.f,
				cx - 31.f * exp, cy + 15.f * exp, cx - 28.f * exp, cy + 21.f * exp);
			// Diaphragmatic concave base
			nvgBezierTo(vg, cx - 22.f * exp, cy + 19.f * exp,
				cx - 12.f * exp, cy + 18.f * exp, cx - 6.f, cy + 19.f * exp);
			// Mediastinal medial border back to hilum/apex
			nvgBezierTo(vg, cx - 5.f, cy + 8.f,
				cx - 5.f, cy - 8.f, cx - 7.f, cy - 19.f);
			nvgClosePath(vg);
		}

		static void traceLeftLung(NVGcontext* vg, float cx, float cy, float exp) {
			// Viewer's right = anatomical Left Lung (with medial cardiac notch)
			nvgMoveTo(vg, cx + 7.f, cy - 19.f);
			// Apical dome
			nvgBezierTo(vg, cx + 12.f * exp, cy - 23.f,
				cx + 22.f * exp, cy - 18.f, cx + 26.f * exp, cy - 7.f);
			// Costal lateral border
			nvgBezierTo(vg, cx + 30.f * exp, cy + 4.f,
				cx + 31.f * exp, cy + 15.f * exp, cx + 28.f * exp, cy + 21.f * exp);
			// Diaphragmatic base to lingula tip
			nvgBezierTo(vg, cx + 22.f * exp, cy + 19.f * exp,
				cx + 15.f * exp, cy + 18.5f * exp, cx + 11.f, cy + 18.f * exp);
			// Cardiac notch (indented for the heart on the left medial border)
			nvgBezierTo(vg, cx + 13.f, cy + 12.f,
				cx + 11.f, cy + 5.f, cx + 6.f, cy + 2.f);
			// Upper mediastinal border back to apex
			nvgBezierTo(vg, cx + 5.f, cy - 6.f,
				cx + 5.f, cy - 13.f, cx + 7.f, cy - 19.f);
			nvgClosePath(vg);
		}

		static void traceBronchi(NVGcontext* vg, float cx, float cy, float exp) {
			// Mainstem bronchi from carina (cx, cy - 10) into right & left lungs
			nvgMoveTo(vg, cx, cy - 10.f);
			nvgLineTo(vg, cx - 12.f * exp, cy - 2.f);
			// Right lobar branches (superior, middle, inferior)
			nvgMoveTo(vg, cx - 8.f * exp, cy - 5.f);
			nvgLineTo(vg, cx - 15.f * exp, cy - 12.f);
			nvgMoveTo(vg, cx - 12.f * exp, cy - 2.f);
			nvgLineTo(vg, cx - 21.f * exp, cy + 3.f);
			nvgMoveTo(vg, cx - 12.f * exp, cy - 2.f);
			nvgLineTo(vg, cx - 17.f * exp, cy + 12.f * exp);
			nvgMoveTo(vg, cx - 15.f * exp, cy + 6.f * exp);
			nvgLineTo(vg, cx - 23.f * exp, cy + 13.f * exp);

			// Left mainstem bronchus & lobar branches (superior, inferior)
			nvgMoveTo(vg, cx, cy - 10.f);
			nvgLineTo(vg, cx + 12.f * exp, cy - 1.f);
			nvgMoveTo(vg, cx + 8.f * exp, cy - 4.f);
			nvgLineTo(vg, cx + 16.f * exp, cy - 11.f);
			nvgMoveTo(vg, cx + 12.f * exp, cy - 1.f);
			nvgLineTo(vg, cx + 21.f * exp, cy + 4.f);
			nvgMoveTo(vg, cx + 12.f * exp, cy - 1.f);
			nvgLineTo(vg, cx + 18.f * exp, cy + 12.f * exp);
			nvgMoveTo(vg, cx + 16.f * exp, cy + 6.f * exp);
			nvgLineTo(vg, cx + 23.f * exp, cy + 13.f * exp);
		}

		void draw(const DrawArgs& args) override {
			// Default to ~68% inhaled in module browser (when module == nullptr)
			float vol = 0.68f, phase = 0.f;
			bool exhaling = false, inhaling = true, holding = false, resting = false;
			if (module && std::isfinite(module->volume)) {
				vol = rack::math::clamp(module->volume, 0.f, 1.f);
				phase = module->phase;
				exhaling = module->exhaling || (phase > 0.5f);
				inhaling = module->inhaling || (!exhaling && module->flow > 0.005f);
				holding = !exhaling && !inhaling && vol > 0.01f;
				resting = !exhaling && !inhaling && vol <= 0.01f;
			}
			float w = box.size.x, h = box.size.y;
			float cx = w * 0.5f, cy = h * 0.52f;
			NVGcontext* vg = args.vg;

			NVGcolor neonCyan  = nvgRGB(0x40, 0xe0, 0xff);
			NVGcolor neonAmber = nvgRGB(0xff, 0x88, 0x54);
			NVGcolor neonMint  = nvgRGB(0x58, 0xe8, 0xa8);

			// Hollow chamfered cyber-HUD bezel
			float ch = 6.f;
			nvgBeginPath(vg);
			nvgMoveTo(vg, ch, 0.f);
			nvgLineTo(vg, w - ch, 0.f);
			nvgLineTo(vg, w, ch);
			nvgLineTo(vg, w, h - ch);
			nvgLineTo(vg, w - ch, h);
			nvgLineTo(vg, ch, h);
			nvgLineTo(vg, 0.f, h - ch);
			nvgLineTo(vg, 0.f, ch);
			nvgClosePath(vg);
			nvgFillColor(vg, nvgRGB(0x0b, 0x0e, 0x12));
			nvgFill(vg);
			strokeNeon(vg, nvgRGBA(0x40, 0xc8, 0xe8, 185), 1.0f, 2.8f, 35);

			// Corner HUD accent brackets
			nvgBeginPath(vg);
			nvgMoveTo(vg, 0.f, ch + 8.f); nvgLineTo(vg, 0.f, ch); nvgLineTo(vg, ch, 0.f); nvgLineTo(vg, ch + 8.f, 0.f);
			nvgMoveTo(vg, w - ch - 8.f, 0.f); nvgLineTo(vg, w - ch, 0.f); nvgLineTo(vg, w, ch); nvgLineTo(vg, w, ch + 8.f);
			nvgMoveTo(vg, 0.f, h - ch - 8.f); nvgLineTo(vg, 0.f, h - ch); nvgLineTo(vg, ch, h); nvgLineTo(vg, ch + 8.f, h);
			nvgMoveTo(vg, w - ch - 8.f, h); nvgLineTo(vg, w - ch, h); nvgLineTo(vg, w, h - ch); nvgLineTo(vg, w, h - ch - 8.f);
			nvgStrokeWidth(vg, 1.4f);
			nvgStrokeColor(vg, exhaling ? neonAmber : neonCyan);
			nvgStroke(vg);

			// Subtle vector grid lines
			nvgBeginPath(vg);
			for (float gx = 20.f; gx < w - 10.f; gx += 22.f) {
				nvgMoveTo(vg, gx, 3.f);
				nvgLineTo(vg, gx, h - 3.f);
			}
			for (float gy = 18.f; gy < h - 6.f; gy += 18.f) {
				nvgMoveTo(vg, 4.f, gy);
				nvgLineTo(vg, w - 4.f, gy);
			}
			nvgStrokeWidth(vg, 0.5f);
			nvgStrokeColor(vg, nvgRGBA(0x32, 0x52, 0x64, 55));
			nvgStroke(vg);

			// Spirometry volume level gauge ticks on left & right inner margins
			float topY = cy - 21.f;
			float botY = cy + 20.f;
			nvgBeginPath(vg);
			for (int i = 0; i <= 4; i++) {
				float ty = botY - (botY - topY) * (i * 0.25f);
				float tickW = (i % 2 == 0) ? 5.f : 3.f;
				nvgMoveTo(vg, 6.f, ty);
				nvgLineTo(vg, 6.f + tickW, ty);
				nvgMoveTo(vg, w - 6.f, ty);
				nvgLineTo(vg, w - 6.f - tickW, ty);
			}
			nvgStrokeWidth(vg, 0.9f);
			nvgStrokeColor(vg, nvgRGBA(0x58, 0x98, 0xb0, 130));
			nvgStroke(vg);

			// Micro-labels for phase & volume telemetry
			auto font = getLabelFont();
			if (font && font->handle >= 0) {
				nvgFontFaceId(vg, font->handle);
				nvgFontSize(vg, 6.f);
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
				nvgFillColor(vg, inhaling ? neonCyan : nvgRGB(0x4a, 0x62, 0x70));
				nvgText(vg, 8.f, 5.f, "INHALE", NULL);

				nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
				nvgFillColor(vg, holding ? neonMint : nvgRGBA(0x4a, 0x62, 0x70, 120));
				nvgText(vg, w * 0.28f, 5.f, "HOLD", NULL);

				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
				nvgFillColor(vg, exhaling ? neonAmber : nvgRGB(0x6c, 0x54, 0x50));
				nvgText(vg, w - 8.f, 5.f, "EXHALE", NULL);

				nvgFontSize(vg, 5.5f);
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
				nvgFillColor(vg, nvgRGBA(0x68, 0xa8, 0xc0, 175));
				nvgText(vg, 8.f, h - 4.f, "TIDAL VOL", NULL);

				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
				nvgText(vg, w - 8.f, h - 4.f, "SUBGLOTTAL", NULL);
			}

			// Thoracic expansion factor with volume
			float exp = 0.92f + 0.12f * vol;

			// ---- 1. Hollow Wireframe Trachea & Carina ----
			nvgBeginPath(vg);
			nvgMoveTo(vg, cx - 2.3f, cy - 28.f);
			nvgLineTo(vg, cx - 2.3f, cy - 10.f);
			nvgMoveTo(vg, cx + 2.3f, cy - 28.f);
			nvgLineTo(vg, cx + 2.3f, cy - 10.f);
			strokeNeon(vg, holding ? neonMint : (exhaling ? neonAmber : neonCyan), 1.0f, 2.6f, 40);

			// Closed glottal/airway valve bar across top of trachea during Breath-Hold
			if (holding) {
				nvgBeginPath(vg);
				nvgMoveTo(vg, cx - 4.2f, cy - 28.5f);
				nvgLineTo(vg, cx + 4.2f, cy - 28.5f);
				strokeNeon(vg, neonMint, 1.4f, 3.4f, 65);
			}

			// Tracheal cartilaginous C-ring ribs
			nvgBeginPath(vg);
			for (float ry = cy - 25.f; ry <= cy - 13.f; ry += 3.5f) {
				nvgMoveTo(vg, cx - 2.3f, ry);
				nvgLineTo(vg, cx + 2.3f, ry);
			}
			nvgStrokeWidth(vg, 0.75f);
			nvgStrokeColor(vg, nvgRGBA(0x90, 0xd8, 0xf0, 140));
			nvgStroke(vg);

			// ---- 2. Unfilled Hollow Wireframe Lung Cavities (Base Layer) ----
			// Horizontal pleural wireframe ribs clipped inside the lungs
			nvgSave(vg);
			nvgBeginPath(vg);
			traceRightLung(vg, cx, cy, exp);
			traceLeftLung(vg, cx, cy, exp);
			nvgFillColor(vg, nvgRGBA(0x10, 0x18, 0x20, 180));
			nvgFill(vg);
			nvgRestore(vg);

			// Dim bronchial wireframe tree inside unfilled lungs
			nvgBeginPath(vg);
			traceBronchi(vg, cx, cy, exp);
			nvgStrokeWidth(vg, 1.1f);
			nvgStrokeColor(vg, nvgRGBA(0x38, 0x62, 0x78, 120));
			nvgLineCap(vg, NVG_ROUND);
			nvgStroke(vg);

			// ---- 3. Rising / Draining Holographic Volume Fill Layer ----
			float fillTop = cy - 22.f;
			float fillBot = cy + 21.5f * exp;
			float fillY = fillBot - vol * (fillBot - fillTop);

			if (vol > 0.005f) {
				NVGcolor topCol = exhaling
					? nvgRGBA(0xff, 0x84, 0x50, 135)   // translucent neon amber-coral
					: nvgRGBA(0x40, 0xe0, 0xff, 135);  // translucent neon cyan
				NVGcolor botCol = exhaling
					? nvgRGBA(0x98, 0x38, 0x24, 55)
					: nvgRGBA(0x18, 0x58, 0x78, 55);
				NVGpaint lungGrad = nvgLinearGradient(vg,
					cx, fillY, cx, fillBot, topCol, botCol);

				nvgSave(vg);
				nvgIntersectScissor(vg, 0.f, fillY, w, fillBot - fillY + 4.f);

				nvgBeginPath(vg);
				traceRightLung(vg, cx, cy, exp);
				traceLeftLung(vg, cx, cy, exp);
				nvgFillPaint(vg, lungGrad);
				nvgFill(vg);

				// Bright neon bronchial tree inside the filled region
				nvgBeginPath(vg);
				traceBronchi(vg, cx, cy, exp);
				nvgLineCap(vg, NVG_ROUND);
				strokeNeon(vg, exhaling ? neonAmber : neonCyan, 1.15f, 3.0f, 55);

				nvgRestore(vg);

				// Glowing neon meniscus horizon clipped strictly inside each lung
				if (vol < 0.97f) {
					nvgSave(vg);
					nvgIntersectScissor(vg, 0.f, fillY - 1.0f, w, 2.2f);
					nvgBeginPath(vg);
					traceRightLung(vg, cx, cy, exp);
					traceLeftLung(vg, cx, cy, exp);
					nvgFillColor(vg, exhaling
						? nvgRGB(0xff, 0xc4, 0x98)
						: nvgRGB(0xb4, 0xf6, 0xff));
					nvgFill(vg);
					nvgRestore(vg);
				}
			}

			// ---- 4. Hollow Neon Pleural Outlines & Lobar Fissure Wireframe ----
			nvgBeginPath(vg);
			// Right horizontal fissure
			nvgMoveTo(vg, cx - 6.f, cy - 1.f);
			nvgQuadTo(vg, cx - 16.f * exp, cy - 1.5f, cx - 27.f * exp, cy + 1.f);
			// Right oblique fissure
			nvgMoveTo(vg, cx - 14.f * exp, cy - 1.f);
			nvgQuadTo(vg, cx - 18.f * exp, cy + 9.f * exp, cx - 22.f * exp, cy + 19.f * exp);
			// Left oblique fissure
			nvgMoveTo(vg, cx + 7.f, cy - 3.f);
			nvgQuadTo(vg, cx + 17.f * exp, cy + 6.f * exp, cx + 23.f * exp, cy + 19.f * exp);
			nvgStrokeWidth(vg, 0.85f);
			nvgStrokeColor(vg, exhaling
				? nvgRGBA(0xff, 0x98, 0x70, 110)
				: nvgRGBA(0x60, 0xd8, 0xf8, 110));
			nvgStroke(vg);

			nvgBeginPath(vg);
			traceRightLung(vg, cx, cy, exp);
			traceLeftLung(vg, cx, cy, exp);
			strokeNeon(vg, resting ? nvgRGB(0x3c, 0x58, 0x6c) : (exhaling ? neonAmber : neonCyan), 1.2f, 3.4f, 55);

			// ---- 5. Animated Neon Diaphragm Dome below Lungs ----
			float diaphPeakY = cy + 18.f + 5.5f * vol;
			float diaphEdgeY = cy + 25.f + 2.0f * vol;
			nvgBeginPath(vg);
			nvgMoveTo(vg, cx - 31.f * exp, diaphEdgeY);
			nvgBezierTo(vg, cx - 18.f, diaphPeakY,
				cx + 18.f, diaphPeakY, cx + 31.f * exp, diaphEdgeY);
			nvgLineCap(vg, NVG_ROUND);
			strokeNeon(vg, exhaling ? neonAmber : neonCyan, 1.2f, 3.2f, 45);
		}
	};

	// Base vertical multi-position snap switch with natural top-to-bottom drag
	// direction (dragging down moves the handle down; dragging up moves it up)
	// and click-to-select / click-to-toggle behavior.
	template <int NumSlots>
	struct VerticalSnapSwitch : rack::app::ParamWidget {
		float pressY = 0.f;
		float oldValue = 0.f;
		float dragDeltaY = 0.f;
		bool dragged = false;

		virtual float slotY(int i) const = 0;

		void initParamQuantity() override {
			ParamWidget::initParamQuantity();
			if (auto* pq = getParamQuantity()) {
				pq->snapEnabled = true;
				pq->smoothEnabled = false;
			}
		}

		void onButton(const rack::event::Button& e) override {
			if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT) {
				pressY = e.pos.y;
			}
			ParamWidget::onButton(e);
		}

		void onDragStart(const rack::event::DragStart& e) override {
			ParamWidget::onDragStart(e);
			if (e.button != GLFW_MOUSE_BUTTON_LEFT)
				return;
			if (auto* pq = getParamQuantity()) {
				oldValue = std::round(pq->getValue());
			}
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

			// Positive mouseDelta.y is downward on screen; slot 0 is at the top
			// and slot (NumSlots - 1) is at the bottom.
			dragDeltaY += e.mouseDelta.y;
			if (std::fabs(dragDeltaY) >= 2.5f) {
				dragged = true;
			}
			if (dragged) {
				float span = std::max(8.f, slotY(NumSlots - 1) - slotY(0));
				float stepPx = std::max(6.f, span / (float) std::max(1, NumSlots - 1));
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
				if (NumSlots <= 2) {
					// 2-position switch: clicking anywhere toggles between 0 and 1
					pq->setValue(curSlot == 0 ? 1.f : 0.f);
				} else {
					// Multi-position switch: clicking toward another slot selects it;
					// clicking the active handle steps to the next slot.
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
	};

	// 4-position snap switch (Cycle/Trigger/Closed/Phasor).
	struct ModeSwitch : VerticalSnapSwitch<4> {
		ModeSwitch() {
			box.size = rack::Vec(18.f, 34.f);
		}

		float slotY(int i) const override {
			// 4 slots evenly spread, inset to keep the handle inside
			return 8.f + i * (box.size.y - 16.f) / 3.f;
		}

		void draw(const DrawArgs& args) override {
			float w = box.size.x, h = box.size.y;
			// track
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, w * 0.5f - 3.5f, 2.f, 7.f, h - 4.f, 3.5f);
			nvgFillColor(args.vg, nvgRGB(0x0d, 0x0f, 0x12));
			nvgFill(args.vg);
			nvgStrokeWidth(args.vg, 1.f);
			nvgStrokeColor(args.vg, nvgRGB(0x2a, 0x2e, 0x35));
			nvgStroke(args.vg);
			// slot ticks
			for (int i = 0; i < 4; i++) {
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, w * 0.5f, slotY(i), 1.2f);
				nvgFillColor(args.vg, nvgRGB(0x44, 0x4c, 0x56));
				nvgFill(args.vg);
			}
			// handle at the snapped position
			float v = getParamQuantity()
				? getParamQuantity()->getValue() : 0.f;
			int pos = (int) std::round(rack::math::clamp(v, 0.f, 3.f));
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, 1.f, slotY(pos) - 5.f, w - 2.f, 10.f, 4.f);
			nvgFillColor(args.vg, nvgRGB(0x5a, 0xc8, 0xe0));
			nvgFill(args.vg);
		}
	};

	// 2-position snap switch for Inhale Pressure mode: 0 = GASP (-P suction), 1 = MUTE (0 V)
	struct InhaleSwitch : VerticalSnapSwitch<2> {
		InhaleSwitch() {
			box.size = rack::Vec(16.f, 24.f);
		}

		float slotY(int i) const override {
			return 6.5f + i * (box.size.y - 13.f);
		}

		void draw(const DrawArgs& args) override {
			float w = box.size.x, h = box.size.y;
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, w * 0.5f - 3.2f, 2.f, 6.4f, h - 4.f, 3.2f);
			nvgFillColor(args.vg, nvgRGB(0x0d, 0x0f, 0x12));
			nvgFill(args.vg);
			nvgStrokeWidth(args.vg, 1.f);
			nvgStrokeColor(args.vg, nvgRGB(0x2a, 0x2e, 0x35));
			nvgStroke(args.vg);
			for (int i = 0; i < 2; i++) {
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, w * 0.5f, slotY(i), 1.1f);
				nvgFillColor(args.vg, nvgRGB(0x44, 0x4c, 0x56));
				nvgFill(args.vg);
			}
			float v = getParamQuantity() ? getParamQuantity()->getValue() : 0.f;
			int pos = (int) std::round(rack::math::clamp(v, 0.f, 1.f));
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, 1.f, slotY(pos) - 4.5f, w - 2.f, 9.f, 3.5f);
			nvgFillColor(args.vg, pos == 0
				? nvgRGB(0x48, 0xd8, 0xf8)
				: nvgRGB(0x64, 0x74, 0x84));
			nvgFill(args.vg);
		}
	};

	// 2-position snap switch for the cycle-start polarity:
	// 0 = Inhale first (physio, cyan), 1 = Exhale first (musical, amber)
	struct PolaritySwitch : VerticalSnapSwitch<2> {
		PolaritySwitch() {
			box.size = rack::Vec(16.f, 24.f);
		}

		float slotY(int i) const override {
			return 6.5f + i * (box.size.y - 13.f);
		}

		void draw(const DrawArgs& args) override {
			float w = box.size.x, h = box.size.y;
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, w * 0.5f - 3.2f, 2.f, 6.4f, h - 4.f, 3.2f);
			nvgFillColor(args.vg, nvgRGB(0x0d, 0x0f, 0x12));
			nvgFill(args.vg);
			nvgStrokeWidth(args.vg, 1.f);
			nvgStrokeColor(args.vg, nvgRGB(0x2a, 0x2e, 0x35));
			nvgStroke(args.vg);
			for (int i = 0; i < 2; i++) {
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, w * 0.5f, slotY(i), 1.1f);
				nvgFillColor(args.vg, nvgRGB(0x44, 0x4c, 0x56));
				nvgFill(args.vg);
			}
			float v = getParamQuantity() ? getParamQuantity()->getValue() : 0.f;
			int pos = (int) std::round(rack::math::clamp(v, 0.f, 1.f));
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, 1.f, slotY(pos) - 4.5f, w - 2.f, 9.f, 3.5f);
			// Cyan = inhale-first (fills before voicing), amber = exhale-first
			// (the beat fires the drain) — matches the gauge phase colors
			nvgFillColor(args.vg, pos == 0
				? nvgRGB(0x48, 0xd8, 0xf8)
				: nvgRGB(0xf0, 0x90, 0x54));
			nvgFill(args.vg);
		}
	};

	static void drawPanel(const rack::widget::Widget::DrawArgs& args, float panelW) {
		NVGcontext* vg = args.vg;
		NVGcolor bg = nvgRGB(0x13, 0x16, 0x19);
		NVGcolor edge = nvgRGB(0x2a, 0x2e, 0x35);
		NVGcolor label = nvgRGB(0xb8, 0xc2, 0xcc);
		NVGcolor accent = nvgRGB(0x48, 0xd8, 0xf8);

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
		nvgFillColor(vg, accent);
		nvgFill(vg);
		nvgBeginPath(vg);
		nvgMoveTo(vg, 0, 7.5f);
		nvgLineTo(vg, panelW, 7.5f);
		nvgStrokeWidth(vg, 0.8f);
		nvgStrokeColor(vg, nvgRGBA(0x48, 0xd8, 0xf8, 120));
		nvgStroke(vg);

		float colL = 58.f, colR = 148.f;
		float inY = 276.f;
		float outY = 328.f;

		// Subtle chamfered wireframe divider between primary cycle params and effort/mode
		nvgBeginPath(vg);
		nvgMoveTo(vg, 22.f, 194.f);
		nvgLineTo(vg, 26.f, 191.f);
		nvgLineTo(vg, panelW - 26.f, 191.f);
		nvgLineTo(vg, panelW - 22.f, 194.f);
		nvgStrokeWidth(vg, 0.8f);
		nvgStrokeColor(vg, nvgRGBA(0x48, 0xa8, 0xc8, 70));
		nvgStroke(vg);

		// Chamfered wireframe bus bracket framing the 3 CV/trigger inputs (TRIG, GATE, DEPTH CV)
		float bx0 = 18.f, bx1 = panelW - 18.f;
		float by0 = inY - 27.f, by1 = inY + 16.5f;
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
		nvgFillColor(vg, nvgRGBA(0x0c, 0x11, 0x16, 110));
		nvgFill(vg);
		nvgStrokeWidth(vg, 0.9f);
		nvgStrokeColor(vg, nvgRGBA(0x48, 0xb8, 0xd8, 105));
		nvgStroke(vg);

		// Hollow neon port rings (inputs & outputs)
		auto drawPortRing = [&](float x, float y, NVGcolor ringCol, bool isOutput) {
			nvgBeginPath(vg);
			nvgCircle(vg, x, y, isOutput ? 14.6f : 13.6f);
			if (isOutput) {
				nvgFillColor(vg, nvgRGB(0x09, 0x0c, 0x10));
				nvgFill(vg);
				strokeNeon(vg, ringCol, 1.05f, 2.8f, 42);
			} else {
				strokeNeon(vg, ringCol, 0.9f, 2.4f, 30);
			}
		};

		NVGcolor trigCol  = nvgRGB(0x58, 0xd8, 0xa0); // mint-green (mode/phase control)
		NVGcolor depthCol = nvgRGB(0x4c, 0xa8, 0xe8); // teal-blue (tidal volume / depth)
		NVGcolor flowCol  = nvgRGB(0xf0, 0x90, 0x54); // warm amber (exhale airflow)

		// Input rings inside the bus bracket (SYNC, TRIG, GATE, DEPTH CV)
		drawPortRing(panelW * 0.15f, inY, trigCol, false);  // SYNC
		drawPortRing(panelW * 0.38f, inY, trigCol, false);  // TRIG
		drawPortRing(panelW * 0.62f, inY, trigCol, false);  // GATE
		drawPortRing(panelW * 0.85f, inY, depthCol, false); // DEPTH CV

		// Output rings (color-coded by physiological signal)
		drawPortRing(panelW * 0.15f, outY, accent, true);   // PRESS (bright cyan -> LARYNX/VOICE PRESS)
		drawPortRing(panelW * 0.38f, outY, depthCol, true); // VOL (teal-blue tidal volume)
		drawPortRing(panelW * 0.62f, outY, flowCol, true);  // FLOW +- (warm amber airflow)
		drawPortRing(panelW * 0.85f, outY, trigCol, true);  // PHASE (mint-green cycle phase)

		nvgFontSize(args.vg, 8);
		auto font = getLabelFont();
		if (font && font->handle >= 0)
			nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
		nvgFillColor(args.vg, label);
		auto text = [&](float x, float y, const char* s) {
			nvgText(args.vg, x, y, s, NULL);
		};

		// params (2 columns below the top lung gauge; 3rd row has EFFORT, INH P,
		// START polarity, MODE, EXH)
		float effX = 40.f, inhSwX = 88.f, polX = 117.f, modeX = 150.f, exhX = 183.f;
		text(colL, 101, "RATE");
		nvgFillColor(args.vg, nvgRGB(0x88, 0xc8, 0xf0));
		text(colR, 101, "DEPTH");
		nvgFillColor(args.vg, label);
		text(colL, 155, "INHALE s"); text(colR, 155, "FLOW MAX");
		text(effX, 204, "EFFORT");
		nvgFillColor(args.vg, nvgRGB(0x7e, 0xd8, 0xf8));
		text(inhSwX + 6.f, 204, "INH P");
		nvgFillColor(args.vg, nvgRGB(0xf4, 0xa8, 0x74));
		text(polX, 204, "START");
		nvgFillColor(args.vg, nvgRGB(0x80, 0xe0, 0xb0));
		text(modeX, 204, "MODE");
		text(exhX, 214, "EXH");
		nvgFillColor(args.vg, label);

		// 2-position INH P tick labels (GASP / MUTE, left of the switch),
		// START polarity tick labels (IN / EX, right of the switch), and the
		// 4-position MODE tick labels
		if (font && font->handle >= 0) {
			nvgFontSize(args.vg, 5.5f);
			nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, nvgRGB(0x7c, 0x8a, 0x98));

			static const char* inhNames[2] = {"GASP", "MUTE"};
			float inhTop = 224.f - 12.f;
			for (int i = 0; i < 2; i++) {
				float iy = inhTop + 6.5f + i * (24.f - 13.f);
				nvgText(args.vg, inhSwX - 10.f, iy, inhNames[i], NULL);
			}

			nvgTextAlign(args.vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			static const char* polNames[2] = {"IN", "EX"};
			for (int i = 0; i < 2; i++) {
				float iy = inhTop + 6.5f + i * (24.f - 13.f);
				nvgText(args.vg, polX + 10.f, iy, polNames[i], NULL);
			}

			static const char* modeNames[4] = {"CYC", "TRG", "CLS", "PHS"};
			float swTop = 224.f - 17.f;
			for (int i = 0; i < 4; i++) {
				float sy = swTop + 8.f + i * (34.f - 16.f) / 3.f;
				nvgText(args.vg, modeX + 11.f, sy, modeNames[i], NULL);
			}
			nvgFontSize(args.vg, 8.f);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
			nvgFillColor(args.vg, label);
		}

		// inputs (color-coded to match their target parameters)
		nvgFillColor(args.vg, nvgRGB(0x80, 0xe0, 0xb0));
		text(panelW * 0.15f, 258, "SYNC");
		text(panelW * 0.38f, 258, "TRIG");
		text(panelW * 0.62f, 258, "GATE");
		nvgFillColor(args.vg, nvgRGB(0x88, 0xc8, 0xf0));
		text(panelW * 0.85f, 258, "DEPTH CV");

		// outputs (color-coded to match gauge & downstream modules)
		nvgFillColor(args.vg, nvgRGB(0x7e, 0xd8, 0xf8));
		text(panelW * 0.15f, 309, "PRESS");
		nvgFillColor(args.vg, nvgRGB(0x88, 0xc8, 0xf0));
		text(panelW * 0.38f, 309, "VOL");
		nvgFillColor(args.vg, nvgRGB(0xf4, 0xa8, 0x74));
		text(panelW * 0.62f, 309, "FLOW +-");
		nvgFillColor(args.vg, nvgRGB(0x80, 0xe0, 0xb0));
		text(panelW * 0.85f, 309, "PHASE");

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
		using rack::componentlibrary::PJ301MPort;

		float colL = 58.f, colR = 148.f;

		// Top-center bilateral human lung & bronchial gauge
		{
			auto gauge = new BreathGauge(module);
			gauge->box.pos = rack::Vec(18.f, 15.f);
			gauge->box.size = rack::Vec(174.f, 70.f);
			addChild(gauge);
		}

		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(colL, 122), module, CyberlineLungs::RATE_PARAM));
		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(colR, 122), module, CyberlineLungs::DEPTH_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(colL, 173), module, CyberlineLungs::INHALE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(colR, 173), module, CyberlineLungs::MAXFLOW_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(40.f, 224), module, CyberlineLungs::EFFORT_PARAM));
		addParam(rack::createParamCentered<InhaleSwitch>(rack::Vec(88.f, 224), module, CyberlineLungs::INHMUTE_PARAM));
		addParam(rack::createParamCentered<PolaritySwitch>(rack::Vec(117.f, 224), module, CyberlineLungs::POLARITY_PARAM));
		addParam(rack::createParamCentered<ModeSwitch>(rack::Vec(150.f, 224), module, CyberlineLungs::MODE_PARAM));

		// Phase light beside the mode switch
		addChild(rack::createLightCentered<rack::componentlibrary::SmallLight<rack::componentlibrary::GreenLight>>(
			rack::Vec(183.f, 224), module, CyberlineLungs::PHASE_LIGHT));

		float row = 276.f;
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.15f, row), module, CyberlineLungs::SYNC_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.38f, row), module, CyberlineLungs::TRIGGER_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.62f, row), module, CyberlineLungs::GATE_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.85f, row), module, CyberlineLungs::DEPTHCV_INPUT));

		float row2 = 328.f;
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.15f, row2), module, CyberlineLungs::PRESSURE_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.38f, row2), module, CyberlineLungs::VOLUME_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.62f, row2), module, CyberlineLungs::FLOW_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(PANEL_W * 0.85f, row2), module, CyberlineLungs::PHASE_OUTPUT));
	}

	void appendContextMenu(rack::ui::Menu* menu) override {
		ModuleWidget::appendContextMenu(menu);
		// Lungs has no quality-dependent DSP; expose the plugin-wide global.
		appendQualityMenu(menu, nullptr, -1);
	}

	void draw(const DrawArgs& args) override {
		drawPanel(args, PANEL_W);
		ModuleWidget::draw(args);
		drawCpuMeterText(args, static_cast<CyberlineLungs*>(module));
	}
};

} // namespace cyberline

namespace cyberline {
rack::plugin::Model* modelCyberlineLungs =
	rack::createModel<CyberlineLungs, CyberlineLungsWidget>("cyberline-lungs");
} // namespace cyberline
