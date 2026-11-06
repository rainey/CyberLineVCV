// CyberlineHeartWidget — programmatic panel (no SVG assets). 22 HP,
// four columns: anatomy knobs | drive knobs + phase lights | in/out | out.
// Heart gauge top-center: atrium over ventricle, plus the aortic reservoir,
// fill-coded and phase-colored (like the Lungs BreathGauge).

#include "CyberlineHeart.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineHeartWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 22 * 15.f;

	// Heart gauge: an anatomical two-chambered heart cross-section inside a
	// medical-telemetry header — vena cava inflow on the left, atrium with
	// bi-leaflet AV valve opening into the thick-walled ventricular cone
	// (apex pointing bottom-right in standard anterior view), pulmonary
	// trunk behind, and three-branched aortic arch rising to the right.
	// Live state: chambers swell with volume, myocardium squeezes on systole,
	// AV and aortic valve leaflets swing with valve states, and blood color
	// transitions from venous cyan (filling) to arterial coral (ejection).
	// Helper: neon double-stroke (outer soft bloom + crisp inner vector core)
	static void strokeNeon(NVGcontext* vg, NVGcolor col, float coreW = 1.2f, float glowW = 3.6f, int glowAlpha = 50) {
		nvgStrokeWidth(vg, glowW);
		nvgStrokeColor(vg, nvgRGBA(col.r * 255, col.g * 255, col.b * 255, glowAlpha));
		nvgStroke(vg);
		nvgStrokeWidth(vg, coreW);
		nvgStrokeColor(vg, col);
		nvgStroke(vg);
	}

	struct HeartGauge : rack::widget::Widget {
		CyberlineHeart* module;
		HeartGauge(CyberlineHeart* m) : module(m) {}

		void draw(const DrawArgs& args) override {
			NVGcontext* vg = args.vg;
			float av = 0.68f, vv = 0.72f, ar = 0.62f;
			bool filling = true, ejecting = false;
			if (module && std::isfinite(module->core.vvol)) {
				av = rack::math::clamp(module->core.avol, 0.f, 1.f);
				vv = rack::math::clamp(module->core.vvol, 0.f, 1.f);
				ar = rack::math::clamp(module->core.apart, 0.f, 1.2f);
				filling = module->core.inflowOpen;
				ejecting = module->core.outflowOpen;
			}
			float w = box.size.x, h = box.size.y;

			NVGcolor neonCyan  = nvgRGB(0x40, 0xe0, 0xff);
			NVGcolor neonCoral = nvgRGB(0xff, 0x58, 0x64);

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
			nvgFillColor(vg, nvgRGB(0x0b, 0x0d, 0x11));
			nvgFill(vg);
			strokeNeon(vg, nvgRGBA(0xe8, 0x5c, 0x64, 185), 1.0f, 2.8f, 35);

			// Corner HUD accent brackets (cyan on left/venous side, coral on right/arterial side)
			nvgBeginPath(vg);
			nvgMoveTo(vg, 0.f, ch + 8.f); nvgLineTo(vg, 0.f, ch); nvgLineTo(vg, ch, 0.f); nvgLineTo(vg, ch + 8.f, 0.f);
			nvgMoveTo(vg, 0.f, h - ch - 8.f); nvgLineTo(vg, 0.f, h - ch); nvgLineTo(vg, ch, h); nvgLineTo(vg, ch + 8.f, h);
			nvgStrokeWidth(vg, 1.4f);
			nvgStrokeColor(vg, neonCyan);
			nvgStroke(vg);

			nvgBeginPath(vg);
			nvgMoveTo(vg, w - ch - 8.f, 0.f); nvgLineTo(vg, w - ch, 0.f); nvgLineTo(vg, w, ch); nvgLineTo(vg, w, ch + 8.f);
			nvgMoveTo(vg, w - ch - 8.f, h); nvgLineTo(vg, w - ch, h); nvgLineTo(vg, w, h - ch); nvgLineTo(vg, w, h - ch - 8.f);
			nvgStrokeWidth(vg, 1.4f);
			nvgStrokeColor(vg, neonCoral);
			nvgStroke(vg);

			// Subtle medical-grid crosshairs in bezel background
			nvgBeginPath(vg);
			for (float gx = 22.f; gx < w - 10.f; gx += 22.f) {
				nvgMoveTo(vg, gx, 3.f);
				nvgLineTo(vg, gx, h - 3.f);
			}
			for (float gy = 18.f; gy < h - 6.f; gy += 18.f) {
				nvgMoveTo(vg, 4.f, gy);
				nvgLineTo(vg, w - 4.f, gy);
			}
			nvgStrokeWidth(vg, 0.5f);
			nvgStrokeColor(vg, nvgRGBA(0x4a, 0x38, 0x48, 50));
			nvgStroke(vg);

			// Left wing: clean neon ECG lead trace (uncluttered)
			nvgBeginPath(vg);
			float ecgY = h * 0.56f;
			nvgMoveTo(vg, 10.f, ecgY);
			nvgLineTo(vg, 22.f, ecgY);
			nvgQuadTo(vg, 25.5f, ecgY - 4.f, 29.f, ecgY);  // P wave
			nvgLineTo(vg, 34.f, ecgY);
			nvgLineTo(vg, 36.5f, ecgY + 3.f);              // Q
			nvgLineTo(vg, 40.f, ecgY - 14.f);              // R peak
			nvgLineTo(vg, 43.5f, ecgY + 5.5f);             // S
			nvgLineTo(vg, 46.f, ecgY);
			nvgLineTo(vg, 52.f, ecgY);
			nvgQuadTo(vg, 56.5f, ecgY - 5.5f, 61.f, ecgY); // T wave
			nvgLineTo(vg, 68.f, ecgY);
			strokeNeon(vg, nvgRGBA(0x40, 0xe0, 0xff, 185), 1.0f, 2.6f, 35);

			// Right wing: clean neon arterial pulse wave (dicrotic notch)
			nvgBeginPath(vg);
			nvgMoveTo(vg, w - 68.f, ecgY + 2.f);
			nvgLineTo(vg, w - 58.f, ecgY + 2.f);
			nvgBezierTo(vg, w - 53.f, ecgY - 13.f,
				w - 47.f, ecgY - 13.f, w - 42.f, ecgY - 3.5f); // systolic peak
			nvgLineTo(vg, w - 38.5f, ecgY - 6.5f);             // dicrotic notch
			nvgQuadTo(vg, w - 31.f, ecgY + 1.f, w - 19.f, ecgY + 2.f);
			nvgLineTo(vg, w - 10.f, ecgY + 2.f);
			strokeNeon(vg, nvgRGBA(0xff, 0x58, 0x64, 195), 1.0f, 2.6f, 35);

			// Monitor corner micro-labels
			auto font = getLabelFont();
			if (font && font->handle >= 0) {
				nvgFontFaceId(vg, font->handle);
				nvgFontSize(vg, 6.f);
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
				nvgFillColor(vg, filling ? neonCyan : nvgRGB(0x58, 0x90, 0xa4));
				nvgText(vg, 8.f, 7.f, "VENOUS / ECG", NULL);

				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
				nvgFillColor(vg, ejecting ? neonCoral : nvgRGB(0xc8, 0x68, 0x70));
				nvgText(vg, w - 8.f, 7.f, "ARTERIAL", NULL);

				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
				nvgFillColor(vg, nvgRGBA(0x58, 0x98, 0xb0, 150));
				nvgFontSize(vg, 5.5f);
				nvgText(vg, 8.f, h - 5.f, "RA / RV", NULL);

				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
				nvgFillColor(vg, nvgRGBA(0xd8, 0x68, 0x74, 150));
				nvgText(vg, w - 8.f, h - 5.f, "LA / LV", NULL);
			}

			// Contraction beat: systolic squeeze
			float ejectPulse = 0.f;
			if (module && ejecting && module->core.period > 0.01f) {
				ejectPulse = rack::math::clamp(
					1.f - module->core.tV / (0.22f * module->core.period),
					0.f, 1.f);
			} else if (!module) {
				ejectPulse = 0.35f;
			}

			float cx = w * 0.50f;
			float cy = h * 0.53f;
			float squeezeX = 1.f - 0.04f * ejectPulse;
			float squeezeY = 1.f + 0.03f * ejectPulse;
			float vt = 0.92f + 0.10f * vv;
			float at = 0.92f + 0.10f * av;
			float arClamped = rack::math::clamp(ar, 0.f, 1.f);

			nvgSave(vg);
			nvgTranslate(vg, cx, cy);
			nvgScale(vg, squeezeX, squeezeY);
			nvgTranslate(vg, -cx, -cy);

			// ---- 1. Clean Great Vessels at the Crown (Vena Cava + Aortic Arch) ----
			// Superior Vena Cava (left-top, cyan hollow tube entering Right Atrium)
			NVGcolor venaCol = filling ? neonCyan : nvgRGB(0x38, 0x98, 0xb8);
			nvgBeginPath(vg);
			nvgMoveTo(vg, cx - 15.f, cy - 22.f);
			nvgLineTo(vg, cx - 14.f, cy - 11.f);
			nvgMoveTo(vg, cx - 10.5f, cy - 22.f);
			nvgLineTo(vg, cx - 9.5f, cy - 12.f);
			strokeNeon(vg, venaCol, 1.05f, 2.8f, 40);

			// Anatomical Aortic Arch (emerging from center root, arching over with 3 short ostia)
			NVGcolor aortaCol = nvgRGB(
				(uint8_t)(0xe0 + 31.f * arClamped),
				(uint8_t)(0x4c + 36.f * (ejecting ? 1.f : 0.35f * arClamped)),
				(uint8_t)(0x5c + 20.f * arClamped));
			nvgBeginPath(vg);
			// Outer arch curve + 3 short arterial branch stubs
			nvgMoveTo(vg, cx - 4.5f, cy - 11.5f);
			nvgBezierTo(vg, cx - 6.f, cy - 20.5f,
				cx + 1.f, cy - 23.5f, cx + 8.5f, cy - 19.5f);
			nvgBezierTo(vg, cx + 11.5f, cy - 17.5f,
				cx + 12.f, cy - 14.f, cx + 10.5f, cy - 11.f);
			// Inner arch curve
			nvgMoveTo(vg, cx + 0.f, cy - 11.5f);
			nvgBezierTo(vg, cx - 0.5f, cy - 16.5f,
				cx + 3.f, cy - 18.f, cx + 6.f, cy - 16.f);
			nvgBezierTo(vg, cx + 7.5f, cy - 15.f,
				cx + 7.5f, cy - 13.f, cx + 6.8f, cy - 11.5f);
			// Three short branch stubs at crest of arch
			nvgMoveTo(vg, cx - 1.f, cy - 20.5f); nvgLineTo(vg, cx - 2.f, cy - 24.5f);
			nvgMoveTo(vg, cx + 2.8f, cy - 21.f); nvgLineTo(vg, cx + 2.8f, cy - 25.f);
			nvgMoveTo(vg, cx + 6.5f, cy - 20.f); nvgLineTo(vg, cx + 7.8f, cy - 23.8f);
			strokeNeon(vg, aortaCol, 1.1f, 2.8f, 45);

			// ---- 2. Single-Wall Biological Heart Halves (Right Heart & Left Heart) ----
			// Like the two lung lobes in BREATH, the Right Heart (venous, cyan) and
			// Left Heart (arterial, coral) are drawn as two clean, biological halves
			// separated by the interventricular septum — avoiding nested double outlines.

			auto traceRightHeart = [&]() {
				nvgBeginPath(vg);
				// Right Atrium upper border & lateral wall -> AV groove indentation
				nvgMoveTo(vg, cx - 4.f, cy - 11.5f);
				nvgBezierTo(vg, cx - 14.f * at, cy - 14.5f * at,
					cx - 22.f * at, cy - 8.5f * at, cx - 20.5f * at, cy - 0.5f);
				nvgBezierTo(vg, cx - 19.5f * at, cy + 3.5f,
					cx - 17.5f, cy + 5.5f, cx - 15.5f, cy + 6.5f);
				// Right Ventricle acute margin sweeping down toward the apex
				nvgBezierTo(vg, cx - 16.5f * vt, cy + 15.f * vt,
					cx - 4.f * vt, cy + 22.5f * vt, cx + 8.f * vt, cy + 24.f * vt);
				// Interventricular septum curving back up to the crux
				nvgBezierTo(vg, cx + 3.5f, cy + 14.f * vt,
					cx - 1.0f, cy + 2.f, cx - 4.f, cy - 11.5f);
				nvgClosePath(vg);
			};

			auto traceLeftHeart = [&]() {
				nvgBeginPath(vg);
				// Interventricular septum -> down to the muscular left ventricular apex
				nvgMoveTo(vg, cx - 0.5f, cy - 11.f);
				nvgBezierTo(vg, cx + 2.f, cy + 2.f,
					cx + 6.5f, cy + 14.f * vt, cx + 11.5f * vt, cy + 24.5f * vt);
				// Left Ventricle apex & broad obtuse lateral margin curving up to AV groove
				nvgBezierTo(vg, cx + 18.f * vt, cy + 24.5f * vt,
					cx + 23.5f * vt, cy + 15.5f * vt, cx + 21.f * vt, cy + 4.f * vt);
				nvgBezierTo(vg, cx + 20.f * vt, cy + 0.5f,
					cx + 18.f, cy - 1.5f, cx + 16.f, cy - 2.5f);
				// Left Atrium upper-right auricle curve back to base
				nvgBezierTo(vg, cx + 19.f * at, cy - 6.5f * at,
					cx + 16.f * at, cy - 13.5f * at, cx + 5.f, cy - 11.5f);
				nvgLineTo(vg, cx - 0.5f, cy - 11.f);
				nvgClosePath(vg);
			};

			// Translucent volume fills modulated by cardiac cycle (avol / vvol)
			float rFillAlpha = 0.20f + 0.45f * (0.4f * av + 0.6f * vv);
			float lFillAlpha = 0.22f + 0.50f * (0.35f * av + 0.65f * vv);

			traceRightHeart();
			nvgFillColor(vg, nvgRGBA(0x18, 0xa8, 0xd8, (uint8_t)(rFillAlpha * 110.f)));
			nvgFill(vg);

			traceLeftHeart();
			nvgFillColor(vg, nvgRGBA(0xf0, 0x42, 0x58, (uint8_t)(lFillAlpha * 115.f)));
			nvgFill(vg);

			// Subtle internal atrioventricular (AV) sulcus lines dividing Atria from Ventricles
			nvgBeginPath(vg);
			nvgMoveTo(vg, cx - 15.5f, cy + 6.5f);
			nvgQuadTo(vg, cx - 8.5f, cy + 5.f, cx - 2.2f, cy + 2.f);
			strokeNeon(vg, nvgRGBA(0x40, 0xe0, 0xff, 150), 0.85f, 2.2f, 30);

			nvgBeginPath(vg);
			nvgMoveTo(vg, cx + 1.8f, cy - 0.5f);
			nvgQuadTo(vg, cx + 9.f, cy + 0.5f, cx + 16.f, cy - 2.5f);
			strokeNeon(vg, nvgRGBA(0xff, 0x58, 0x64, 155), 0.85f, 2.2f, 30);

			// Crisp neon outer contours for Right Heart (cyan) and Left Heart (coral)
			traceRightHeart();
			strokeNeon(vg, venaCol, 1.2f, 3.4f, 55);

			traceLeftHeart();
			NVGcolor lvCol = ejecting ? nvgRGB(0xff, 0x68, 0x60) : neonCoral;
			strokeNeon(vg, lvCol, 1.25f, 3.6f, 60);

			nvgRestore(vg);
		}
	};

	static void drawPanel(const rack::widget::Widget::DrawArgs& args, float panelW) {
		NVGcolor bg = nvgRGB(0x12, 0x15, 0x1a);
		NVGcolor edge = nvgRGB(0x28, 0x30, 0x3a);
		NVGcolor label = nvgRGB(0xb8, 0xc2, 0xcc);
		NVGcolor accent = nvgRGB(0xf0, 0x4e, 0x62);

		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, panelW, rack::app::RACK_GRID_HEIGHT);
		nvgFillColor(args.vg, bg);
		nvgFill(args.vg);
		nvgStrokeWidth(args.vg, 1.f);
		nvgStrokeColor(args.vg, edge);
		nvgStroke(args.vg);

		// Top neon accent bar + sub-trace
		nvgBeginPath(args.vg);
		nvgRect(args.vg, 0, 0, panelW, 5);
		nvgFillColor(args.vg, accent);
		nvgFill(args.vg);
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, 0, 6.5f);
		nvgLineTo(args.vg, panelW, 6.5f);
		nvgStrokeWidth(args.vg, 0.8f);
		nvgStrokeColor(args.vg, nvgRGBA(0xf0, 0x4e, 0x62, 120));
		nvgStroke(args.vg);

		float c1 = 41.f, c2 = 123.f, c3 = 206.f, c4 = 288.f;
		float r[7] = {112.f, 150.f, 188.f, 226.f, 264.f, 302.f, 340.f};

		// ---- 1. Chamfered Wireframe CV Input Bus Bracket (Col 3, rows 0..3) ----
		// Separates the 4 CV inputs (PACING, FILL CV, DRIVE CV, AFT CV) from the
		// 3 chamber outputs (FILL LVL, PRESSURE, FLOW +-) at the bottom of Column 3.
		// Bottom edge opens around the FILL LVL label so the line never strikes text.
		{
			float bx0 = c3 - 26.f, bx1 = c3 + 26.f;
			float by0 = r[0] - 24.5f, by1 = r[3] + 15.f;
			float bch = 4.5f;
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, c3 - 19.f, by1);
			nvgLineTo(args.vg, bx0 + bch, by1);
			nvgLineTo(args.vg, bx0, by1 - bch);
			nvgLineTo(args.vg, bx0, by0 + bch);
			nvgLineTo(args.vg, bx0 + bch, by0);
			nvgLineTo(args.vg, bx1 - bch, by0);
			nvgLineTo(args.vg, bx1, by0 + bch);
			nvgLineTo(args.vg, bx1, by1 - bch);
			nvgLineTo(args.vg, bx1 - bch, by1);
			nvgLineTo(args.vg, c3 + 19.f, by1);
			nvgStrokeWidth(args.vg, 0.9f);
			nvgStrokeColor(args.vg, nvgRGBA(0x48, 0x98, 0xb8, 105));
			nvgStroke(args.vg);
		}

		// ---- 2. Color-Coded Hollow Neon Output & Input Jack Rings ----
		auto drawOutPad = [&](float x, float y, NVGcolor ringCol, float glowA = 40.f) {
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, x, y, 13.6f);
			nvgFillColor(args.vg, nvgRGB(0x09, 0x0c, 0x10));
			nvgFill(args.vg);
			strokeNeon(args.vg, ringCol, 1.0f, 2.7f, (uint8_t)glowA);
		};

		NVGcolor outLinkCol = nvgRGB(0xff, 0x4c, 0x60); // bright neon coral (OUTFLOW)
		NVGcolor artCol     = nvgRGB(0xe8, 0x64, 0x70); // arterial/pressure coral-rose
		NVGcolor flowCol    = nvgRGB(0xf0, 0x98, 0x54); // warm amber (FLOW +-)
		NVGcolor volCol     = nvgRGB(0x48, 0xc0, 0xe8); // venous/chamber cyan-blue (FILL LVL)
		NVGcolor ecgCol     = nvgRGB(0x58, 0xd8, 0x98); // bio-electric mint-green (ECG, BEAT, ATRIA)
		NVGcolor sndCol     = nvgRGB(0xb4, 0x7c, 0xf0); // neon violet (SOUND)
		NVGcolor fillLinkCol = nvgRGB(0x00, 0xd8, 0xf8); // bright neon cyan (FILL CV)

		// Column 3 outputs (rows 4..6): FILL LVL, PRESSURE, FLOW +-
		drawOutPad(c3, r[4], volCol);
		drawOutPad(c3, r[5], artCol);
		drawOutPad(c3, r[6], flowCol);

		// Column 4 outputs (rows 0..5): OUTFLOW, ARTERIAL, ECG, BEAT, ATRIA, SOUND
		drawOutPad(c4, r[0], outLinkCol, 52.f);
		drawOutPad(c4, r[1], artCol);
		drawOutPad(c4, r[2], ecgCol);
		drawOutPad(c4, r[3], ecgCol);
		drawOutPad(c4, r[4], ecgCol);
		drawOutPad(c4, r[5], sndCol);

		// Column 2 row 6: SYNC input ring (bio-electric mint-green)
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, c2, r[6], 13.2f);
		strokeNeon(args.vg, ecgCol, 0.95f, 2.5f, 32);

		// ---- 3. CV Input Rings & 1-to-1 Coupling Lines (Col 3 -> Col 2 Knobs) ----
		NVGcolor cvCols[4] = {
			ecgCol,                         // r[0]: PACING -> IRRIT
			fillLinkCol,                    // r[1]: FILL CV -> FILL
			nvgRGB(0xe8, 0x74, 0x80),       // r[2]: DRIVE CV -> DRIVE
			nvgRGB(0xd8, 0x90, 0x64)        // r[3]: AFT CV -> AFTERLOAD
		};
		for (int i = 0; i < 4; i++) {
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, c3, r[i], 13.2f);
			strokeNeon(args.vg, cvCols[i], (i == 1) ? 1.15f : 0.9f, (i == 1) ? 3.0f : 2.3f, (i == 1) ? 48 : 28);

			// Horizontal coupling trace from CV jack (c3) left to paired knob (c2)
			NVGcolor lc = cvCols[i];
			lc.a = (i == 1) ? 0.50f : 0.32f;
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, c3 - 15.f, r[i]);
			nvgLineTo(args.vg, c2 + 16.f, r[i]);
			nvgStrokeWidth(args.vg, (i == 1) ? 1.0f : 0.85f);
			nvgStrokeColor(args.vg, lc);
			nvgStroke(args.vg);

			// Arrowhead pointing into the Col 2 knob
			lc.a = (i == 1) ? 0.65f : 0.45f;
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, c2 + 14.5f, r[i]);
			nvgLineTo(args.vg, c2 + 18.2f, r[i] - 2.1f);
			nvgLineTo(args.vg, c2 + 18.2f, r[i] + 2.1f);
			nvgClosePath(args.vg);
			nvgFillColor(args.vg, lc);
			nvgFill(args.vg);
		}

		// ---- 4. Circulation Series-Link Diagram: OUTFLOW -> LOOP -> FILL CV ----
		float midX = (c3 + c4) * 0.5f;
		float midY = (r[0] + r[1]) * 0.5f;
		NVGpaint traceGrad = nvgLinearGradient(args.vg,
			c4 - 15.f, r[0], c3 + 16.f, r[1],
			nvgRGBA(0xe8, 0x74, 0x6e, 200),
			nvgRGBA(0x5a, 0xc8, 0xe0, 200));
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, c4 - 15.f, r[0]);
		nvgLineTo(args.vg, midX + 5.f, r[0]);
		nvgQuadTo(args.vg, midX, r[0], midX, r[0] + 5.f);
		nvgLineTo(args.vg, midX, r[1] - 5.f);
		nvgQuadTo(args.vg, midX, r[1], midX - 5.f, r[1]);
		nvgLineTo(args.vg, c3 + 16.f, r[1]);
		nvgStrokeWidth(args.vg, 1.4f);
		nvgStrokePaint(args.vg, traceGrad);
		nvgStroke(args.vg);

		// Arrowhead pointing into FILL CV (c3, r[1])
		nvgBeginPath(args.vg);
		nvgMoveTo(args.vg, c3 + 14.5f, r[1]);
		nvgLineTo(args.vg, c3 + 20.0f, r[1] - 3.2f);
		nvgLineTo(args.vg, c3 + 20.0f, r[1] + 3.2f);
		nvgClosePath(args.vg);
		nvgFillColor(args.vg, fillLinkCol);
		nvgFill(args.vg);

		nvgFontSize(args.vg, 8);
		auto font = getLabelFont();
		if (font && font->handle >= 0)
			nvgFontFaceId(args.vg, font->handle);

		// Dark pill + micro-label on the OUTFLOW -> FILL CV trace so line doesn't strike text
		if (font && font->handle >= 0) {
			nvgBeginPath(args.vg);
			nvgRoundedRect(args.vg, midX - 12.f, midY - 4.8f, 24.f, 9.6f, 3.f);
			nvgFillColor(args.vg, bg);
			nvgFill(args.vg);
			nvgStrokeWidth(args.vg, 0.8f);
			nvgStrokeColor(args.vg, nvgRGBA(0x46, 0x7c, 0x90, 140));
			nvgStroke(args.vg);

			nvgFontSize(args.vg, 5.8f);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, nvgRGB(0x7e, 0xd4, 0xe8));
			nvgText(args.vg, midX, midY + 0.3f, "LOOP", NULL);
			nvgFontSize(args.vg, 8.f);
		}
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);

		nvgFillColor(args.vg, label);
		auto text = [&](float x, float y, const char* s) {
			nvgText(args.vg, x, y, s, NULL);
		};

		// column 1: anatomy knobs
		text(c1, 96, "MASS");        text(c1, 134, "RATE");
		text(c1, 172, "AV DELAY");   text(c1, 210, "CONTRACT");
		text(c1, 248, "COMPLIANCE"); text(c1, 286, "VALVE -+");
		text(c1, 324, "HRV");

		// column 2: drive knobs + phase lights (color-matched to Col 3 CV bus)
		nvgFillColor(args.vg, nvgRGB(0x88, 0xe0, 0xb4));
		text(c2, 96, "IRRIT");
		nvgFillColor(args.vg, nvgRGB(0x7e, 0xd4, 0xe8));
		text(c2, 134, "FILL");
		nvgFillColor(args.vg, nvgRGB(0xf0, 0x94, 0x9c));
		text(c2, 172, "DRIVE");
		nvgFillColor(args.vg, nvgRGB(0xe8, 0xb0, 0x88));
		text(c2, 210, "AFTERLOAD");
		nvgFillColor(args.vg, nvgRGB(0xc8, 0xa0, 0xf8));
		text(c2, 248, "SOUND");
		nvgFillColor(args.vg, label);
		text(c2, 286, "PHASE");
		nvgFillColor(args.vg, nvgRGB(0x88, 0xe0, 0xb4));
		text(c2, 324, "SYNC");

		// column 3: CV inputs (rows 0..3) + chamber outputs (rows 4..6)
		nvgFillColor(args.vg, nvgRGB(0x88, 0xe0, 0xb4));
		text(c3, 96, "PACING");
		nvgFillColor(args.vg, nvgRGB(0x7e, 0xd8, 0xf8));
		text(c3, 134, "FILL CV");
		nvgFillColor(args.vg, nvgRGB(0xf0, 0x94, 0x9c));
		text(c3, 172, "DRIVE CV");
		nvgFillColor(args.vg, nvgRGB(0xe8, 0xb0, 0x88));
		text(c3, 210, "AFT CV");
		nvgFillColor(args.vg, nvgRGB(0x7e, 0xd0, 0xf0));
		text(c3, 248, "FILL LVL");
		nvgFillColor(args.vg, nvgRGB(0xf0, 0x88, 0x8e));
		text(c3, 286, "PRESSURE");
		nvgFillColor(args.vg, nvgRGB(0xf4, 0xb0, 0x74));
		text(c3, 324, "FLOW +-");

		// column 4: outputs (color-coded by physiological signal family)
		nvgFillColor(args.vg, nvgRGB(0xff, 0x84, 0x8e));
		text(c4, 96, "OUTFLOW");
		nvgFillColor(args.vg, nvgRGB(0xf0, 0x88, 0x8e));
		text(c4, 134, "ARTERIAL");
		nvgFillColor(args.vg, nvgRGB(0x88, 0xe0, 0xb4));
		text(c4, 172, "ECG");
		text(c4, 210, "BEAT");
		text(c4, 248, "ATRIA");
		nvgFillColor(args.vg, nvgRGB(0xc8, 0xa0, 0xf8));
		text(c4, 286, "SOUND");

		drawPanelTitle(args, panelW, "HEART");
	}

	CyberlineHeartWidget(CyberlineHeart* module) {
		setModule(module);
		box.size = rack::Vec(PANEL_W, rack::app::RACK_GRID_HEIGHT);

		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, rack::app::RACK_GRID_HEIGHT - 15)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, rack::app::RACK_GRID_HEIGHT - 15)));

		using rack::componentlibrary::RoundSmallBlackKnob;
		using rack::componentlibrary::PJ301MPort;

		float c1 = 41.f, c2 = 123.f, c3 = 206.f, c4 = 288.f;
		// 38 px row pitch: ~23 px widgets + clean space for each label
		float r[7] = {112.f, 150.f, 188.f, 226.f, 264.f, 302.f, 340.f};

		// column 1: anatomy
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[0]), module, CyberlineHeart::MASS_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[1]), module, CyberlineHeart::RATE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[2]), module, CyberlineHeart::AVDELAY_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[3]), module, CyberlineHeart::CONTRACT_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[4]), module, CyberlineHeart::COMPLIANCE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[5]), module, CyberlineHeart::VALVE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[6]), module, CyberlineHeart::HRV_PARAM));

		// column 2: drive
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, r[0]), module, CyberlineHeart::IRRIT_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, r[1]), module, CyberlineHeart::FILL_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, r[2]), module, CyberlineHeart::DRIVE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, r[3]), module, CyberlineHeart::AFTERLOAD_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, r[4]), module, CyberlineHeart::SOUNDLVL_PARAM));
		// phase lights (cyan filling / warm ejecting)
		addChild(rack::createLightCentered<rack::componentlibrary::SmallLight<rack::componentlibrary::GreenLight>>(
			rack::Vec(c2 - 12.f, r[5]), module, CyberlineHeart::FILL_LIGHT));
		addChild(rack::createLightCentered<rack::componentlibrary::SmallLight<rack::componentlibrary::RedLight>>(
			rack::Vec(c2 + 12.f, r[5]), module, CyberlineHeart::EJECT_LIGHT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c2, r[6]), module, CyberlineHeart::SYNC_INPUT));

		// column 3: inputs + outputs
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c3, r[0]), module, CyberlineHeart::PACING_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c3, r[1]), module, CyberlineHeart::FILLCV_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c3, r[2]), module, CyberlineHeart::DRIVECV_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c3, r[3]), module, CyberlineHeart::AFTERLOADCV_INPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c3, r[4]), module, CyberlineHeart::VOLUME_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c3, r[5]), module, CyberlineHeart::PRESSURE_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c3, r[6]), module, CyberlineHeart::FLOW_OUTPUT));

		// column 4: outputs
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c4, r[0]), module, CyberlineHeart::OUTFLOW_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c4, r[1]), module, CyberlineHeart::ARTERIAL_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c4, r[2]), module, CyberlineHeart::ECG_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c4, r[3]), module, CyberlineHeart::BEAT_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c4, r[4]), module, CyberlineHeart::ATRIA_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c4, r[5]), module, CyberlineHeart::SOUND_OUTPUT));

		// heart gauge, top-center between columns 2 and 3 (bottom edge
		// clears the first label row's text top at ~88 px)
		{
			auto gauge = new HeartGauge(module);
			gauge->box.pos = rack::Vec(60.f, 11.f);
			gauge->box.size = rack::Vec(210.f, 68.f);
			addChild(gauge);
		}
	}

	void appendContextMenu(rack::ui::Menu* menu) override {
		ModuleWidget::appendContextMenu(menu);
		// Heart has no quality-dependent DSP; expose the plugin-wide global.
		appendQualityMenu(menu, nullptr, -1);
	}

	void draw(const DrawArgs& args) override {
		drawPanel(args, PANEL_W);
		ModuleWidget::draw(args);
		drawCpuMeterText(args, static_cast<CyberlineHeart*>(module));
	}
};

} // namespace cyberline

namespace cyberline {
rack::plugin::Model* modelCyberlineHeart =
	rack::createModel<CyberlineHeart, CyberlineHeartWidget>("cyberline-heart");
} // namespace cyberline
