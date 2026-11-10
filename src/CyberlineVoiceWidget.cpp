// CyberlineVoiceWidget — programmatic panel (no SVG assets). 16 HP.
// Three-column grid; label 16 px above its widget.

#include "CyberlineVoice.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineVoiceWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 16 * 15.f;

	// Helper: neon double-stroke (outer soft bloom + crisp inner vector core)
	static void strokeNeon(NVGcontext* vg, NVGcolor col, float coreW = 1.2f, float glowW = 3.6f, int glowAlpha = 50) {
		nvgStrokeWidth(vg, glowW);
		nvgStrokeColor(vg, nvgRGBA(col.r * 255, col.g * 255, col.b * 255, glowAlpha));
		nvgStroke(vg);
		nvgStrokeWidth(vg, coreW);
		nvgStrokeColor(vg, col);
		nvgStroke(vg);
	}

	// Cyberline fused Glottis -> Tract hollow-wireframe telemetry HUD:
	// - Left: hollow wireframe vocal folds & LF excitation pulse (neon magenta/coral)
	// - Right: hollow wireframe Kelly-Lochbaum oral + nasal waveguide (neon violet/cyan)
	// - Top arc: semi-coupled acoustic feedback loop (FB GAIN) from tract back to glottis
	struct VoiceWireframeGauge : rack::widget::Widget {
		CyberlineVoice* module;
		VoiceWireframeGauge(CyberlineVoice* m) : module(m) {}

		void draw(const DrawArgs& args) override {
			float w = box.size.x, h = box.size.y;
			NVGcontext* vg = args.vg;

			NVGcolor neonMag    = nvgRGB(0xff, 0x44, 0xa8);
			NVGcolor neonCyan   = nvgRGB(0x40, 0xe0, 0xff);
			NVGcolor neonViolet = nvgRGB(0x9b, 0x6d, 0xff);
			NVGcolor neonAmber  = nvgRGB(0xff, 0xb0, 0x40);

			// Hollow chamfered cyber-HUD frame
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
			nvgFillColor(vg, nvgRGB(0x0d, 0x0b, 0x10));
			nvgFill(vg);
			strokeNeon(vg, nvgRGBA(0xe0, 0x54, 0x9a, 200), 1.0f, 2.8f, 35);

			// Corner HUD accent brackets
			nvgBeginPath(vg);
			nvgMoveTo(vg, 0.f, ch + 8.f); nvgLineTo(vg, 0.f, ch); nvgLineTo(vg, ch, 0.f); nvgLineTo(vg, ch + 8.f, 0.f);
			nvgMoveTo(vg, w - ch - 8.f, 0.f); nvgLineTo(vg, w - ch, 0.f); nvgLineTo(vg, w, ch); nvgLineTo(vg, w, ch + 8.f);
			nvgMoveTo(vg, 0.f, h - ch - 8.f); nvgLineTo(vg, 0.f, h - ch); nvgLineTo(vg, ch, h); nvgLineTo(vg, ch + 8.f, h);
			nvgMoveTo(vg, w - ch - 8.f, h); nvgLineTo(vg, w - ch, h); nvgLineTo(vg, w, h - ch); nvgLineTo(vg, w, h - ch - 8.f);
			nvgStrokeWidth(vg, 1.4f);
			nvgStrokeColor(vg, neonCyan);
			nvgStroke(vg);

			// Subtle vector grid
			nvgBeginPath(vg);
			for (float gx = 26.f; gx < w - 10.f; gx += 26.f) {
				nvgMoveTo(vg, gx, 3.f);
				nvgLineTo(vg, gx, h - 3.f);
			}
			for (float gy = 16.f; gy < h - 6.f; gy += 15.f) {
				nvgMoveTo(vg, 4.f, gy);
				nvgLineTo(vg, w - 4.f, gy);
			}
			nvgStrokeWidth(vg, 0.5f);
			nvgStrokeColor(vg, nvgRGBA(0x68, 0x38, 0x58, 55));
			nvgStroke(vg);

			float tens = 0.6f, amp = 0.8f, te = 0.52f;
			float velum = 0.15f, tngX = 0.5f, tngY = 0.5f, fb = 0.25f;
			int n = 44, noseStart = 17;
			if (module && std::isfinite(module->glottis.curTenseness)) {
				tens = rack::math::clamp(module->glottis.curTenseness, 0.f, 1.f);
				amp = rack::math::clamp(module->glottis.intensity, 0.f, 1.f);
				if (module->glottis.Te > 0.05f && module->glottis.Te < 0.98f)
					te = module->glottis.Te;
				velum = rack::math::clamp(module->tract.velum, 0.f, 1.f);
				if (module->tract.initialized && module->tract.n >= 12) {
					n = module->tract.n;
					noseStart = module->tract.noseStart;
					float span = std::fmax(1.f, (float)(module->tract.tipStart - module->tract.bladeStart));
					tngX = rack::math::clamp((module->tract.tongueIndex - module->tract.bladeStart) / span, 0.f, 1.f);
					tngY = rack::math::clamp((module->tract.tongueDiameter - 1.5f) / 3.5f, 0.f, 1.f);
				} else {
					tngX = rack::math::clamp(module->params[CyberlineVoice::V_TONGUEX_PARAM].getValue() * 0.1f, 0.f, 1.f);
					tngY = rack::math::clamp(module->params[CyberlineVoice::V_TONGUEY_PARAM].getValue() * 0.1f, 0.f, 1.f);
				}
				fb = rack::math::clamp(module->params[CyberlineVoice::V_FB_PARAM].getValue(), 0.f, 1.f);
			}

			// ---- 1. Left Side: Curved Hollow Wireframe Vocal Folds + LF Pulse ----
			float gcx = 23.f, gcy = h * 0.56f;
			float halfLen = 11.5f;
			float chinkW = (1.3f + 4.2f * (1.f - tens)) * (0.4f + 0.6f * amp);

			// Curved epiglottis crescent at anterior (top) commissure
			nvgBeginPath(vg);
			nvgMoveTo(vg, gcx - 9.f, gcy - halfLen - 1.f);
			nvgBezierTo(vg, gcx - 5.f, gcy - halfLen - 5.f,
				gcx + 5.f, gcy - halfLen - 5.f, gcx + 9.f, gcy - halfLen - 1.f);
			strokeNeon(vg, nvgRGBA(0xff, 0x44, 0xa8, 175), 1.0f, 2.5f, 35);

			// Outer curved ventricular / laryngeal ring & vocal fold bodies (hollow bezier curves)
			nvgBeginPath(vg);
			nvgMoveTo(vg, gcx, gcy - halfLen);
			nvgBezierTo(vg, gcx - 13.f, gcy - halfLen * 0.55f,
				gcx - 13.f, gcy + halfLen * 0.55f, gcx, gcy + halfLen);
			nvgBezierTo(vg, gcx + 13.f, gcy + halfLen * 0.55f,
				gcx + 13.f, gcy - halfLen * 0.55f, gcx, gcy - halfLen);
			strokeNeon(vg, nvgRGBA(0xff, 0x44, 0xa8, 200), 1.0f, 2.8f, 40);

			// Inner curved rima glottidis (glottal aperture between vocal folds)
			nvgBeginPath(vg);
			nvgMoveTo(vg, gcx, gcy - halfLen + 0.5f);
			nvgBezierTo(vg, gcx - chinkW * 1.3f, gcy - halfLen * 0.35f,
				gcx - chinkW * 1.45f, gcy + halfLen * 0.4f,
				gcx, gcy + halfLen - 0.5f);
			nvgBezierTo(vg, gcx + chinkW * 1.45f, gcy + halfLen * 0.4f,
				gcx + chinkW * 1.3f, gcy - halfLen * 0.35f,
				gcx, gcy - halfLen + 0.5f);
			nvgClosePath(vg);
			strokeNeon(vg, neonCyan, 1.15f, 3.0f, 55);

			// Mini LF pulse wave coupling from glottis (x=40..78) into tract entrance (x=82)
			float wx0 = 40.f, wx1 = 78.f, ww = wx1 - wx0;
			float wyMid = h * 0.56f;
			nvgBeginPath(vg);
			int N = 20;
			float sharpness = 2.5f + 8.5f * tens;
			for (int i = 0; i <= N; i++) {
				float p = (float) i / (float) N;
				float yVal = 0.f;
				if (p <= te) {
					float u = p / std::fmax(te, 0.1f);
					yVal = std::sin(u * (float) M_PI) * std::exp(1.1f * (u - 0.5f));
					if (u > 0.72f) {
						float d = (u - 0.72f) / 0.28f;
						yVal -= d * d * (1.2f + 1.0f * tens);
					}
				} else {
					float r = (p - te) / std::fmax(1.f - te, 0.05f);
					yVal = -(0.55f + 0.9f * tens) * (std::exp(-sharpness * r) - std::exp(-sharpness))
						/ (1.f - std::exp(-sharpness));
				}
				float px = wx0 + p * ww;
				float py = rack::math::clamp(wyMid - yVal * (0.3f + 0.7f * amp) * 10.f, 10.f, h - 8.f);
				if (i == 0) nvgMoveTo(vg, px, py);
				else nvgLineTo(vg, px, py);
			}
			strokeNeon(vg, neonMag, 1.2f, 3.2f, 50);

			// ---- 2. Right Side: Hollow Wireframe Oral + Nasal Waveguide ----
			auto getOralDiam = [&](float normPos) -> float {
				if (module && module->tract.initialized && module->tract.n >= 12) {
					int idx = rack::math::clamp((int)(normPos * (n - 1)), 0, n - 1);
					float d = module->tract.diameter[idx];
					return std::isfinite(d) ? rack::math::clamp(d, 0.f, 3.5f) : 1.5f;
				}
				float base = (normPos < 0.16f) ? 0.8f : (normPos < 0.28f ? 1.3f : 1.85f);
				if (normPos > 0.88f) base = 1.35f;
				float tCenter = 0.28f + 0.45f * tngX;
				float dist = (normPos - tCenter) / 0.22f;
				if (std::fabs(dist) < 1.f) {
					float arch = 0.5f * (1.f + std::cos(dist * (float) M_PI));
					base -= arch * (0.4f + 0.8f * (1.f - tngY));
				}
				return rack::math::clamp(base, 0.15f, 3.2f);
			};

			float tx0 = 84.f, tx1 = w - 16.f;
			float tractW = tx1 - tx0;
			float oralCenterY = h * 0.64f;
			float scaleD = 5.0f;

			// Nasal branch hollow wireframe
			float noseStartNorm = (float) noseStart / (float) std::max(n, 1);
			float nx0 = tx0 + noseStartNorm * tractW;
			float nx1 = tx1 - 3.f;
			float nw = nx1 - nx0;
			float noseBaseY = h * 0.26f;
			int nRibs = 9;
			nvgBeginPath(vg);
			for (int i = 0; i <= nRibs; i++) {
				float u = (float) i / (float) nRibs;
				float arch = std::sin(u * (float) M_PI);
				float nd = (0.35f + 0.85f * arch);
				float rx = nx0 + u * nw;
				nvgMoveTo(vg, rx, noseBaseY - nd * 3.5f);
				nvgLineTo(vg, rx, noseBaseY + nd * 2.0f);
			}
			nvgStrokeWidth(vg, 0.65f);
			nvgStrokeColor(vg, nvgRGBA(0x40, 0xe0, 0xff, (uint8_t)(65 + 110 * velum)));
			nvgStroke(vg);

			nvgBeginPath(vg);
			for (int i = 0; i <= nRibs; i++) {
				float u = (float) i / (float) nRibs;
				float nd = 0.35f + 0.85f * std::sin(u * (float) M_PI);
				float rx = nx0 + u * nw;
				if (i == 0) nvgMoveTo(vg, rx, noseBaseY - nd * 3.5f);
				else nvgLineTo(vg, rx, noseBaseY - nd * 3.5f);
			}
			for (int i = 0; i <= nRibs; i++) {
				float u = (float) i / (float) nRibs;
				float nd = 0.35f + 0.85f * std::sin(u * (float) M_PI);
				float rx = nx0 + u * nw;
				if (i == 0) nvgMoveTo(vg, rx, noseBaseY + nd * 2.0f);
				else nvgLineTo(vg, rx, noseBaseY + nd * 2.0f);
			}
			strokeNeon(vg, nvgRGBA(0x40, 0xe0, 0xff, (uint8_t)(125 + 115 * velum)), 0.95f, 2.5f, 35);

			// Velopharyngeal junction branch lines connecting oral roof up to nasal floor
			float jTopOralY = oralCenterY - std::sin(noseStartNorm * (float) M_PI) * 3.0f
				- getOralDiam(noseStartNorm) * scaleD * 0.58f;
			nvgBeginPath(vg);
			nvgMoveTo(vg, nx0 - 3.5f, jTopOralY);
			nvgLineTo(vg, nx0, noseBaseY + 0.8f);
			nvgMoveTo(vg, nx0 + 4.5f, jTopOralY + 0.5f);
			nvgLineTo(vg, nx0 + 3.5f, noseBaseY + 1.1f);
			strokeNeon(vg, nvgRGBA(0x40, 0xe0, 0xff, (uint8_t)(110 + 130 * velum)), 0.95f, 2.4f, 40);

			// Oral tract hollow wireframe ribs & contours
			int oRibs = 20;
			nvgBeginPath(vg);
			for (int i = 0; i <= oRibs; i++) {
				float u = (float) i / (float) oRibs;
				float d = getOralDiam(u);
				float rx = tx0 + u * tractW;
				float roofBias = std::sin(u * (float) M_PI) * 3.0f;
				nvgMoveTo(vg, rx, oralCenterY - roofBias - d * scaleD * 0.58f);
				nvgLineTo(vg, rx, oralCenterY - roofBias + d * scaleD * 0.42f);
			}
			nvgStrokeWidth(vg, 0.7f);
			nvgStrokeColor(vg, nvgRGBA(0x9b, 0x6d, 0xff, 90));
			nvgStroke(vg);

			// Palate contour (neon violet)
			nvgBeginPath(vg);
			for (int i = 0; i <= oRibs; i++) {
				float u = (float) i / (float) oRibs;
				float d = getOralDiam(u);
				float rx = tx0 + u * tractW;
				float topY = oralCenterY - std::sin(u * (float) M_PI) * 3.0f - d * scaleD * 0.58f;
				if (i == 0) nvgMoveTo(vg, rx, topY);
				else nvgLineTo(vg, rx, topY);
			}
			strokeNeon(vg, neonViolet, 1.2f, 3.2f, 50);

			// Tongue contour (neon magenta)
			nvgBeginPath(vg);
			for (int i = 0; i <= oRibs; i++) {
				float u = (float) i / (float) oRibs;
				float d = getOralDiam(u);
				float rx = tx0 + u * tractW;
				float botY = oralCenterY - std::sin(u * (float) M_PI) * 3.0f + d * scaleD * 0.42f;
				if (i == 0) nvgMoveTo(vg, rx, botY);
				else nvgLineTo(vg, rx, botY);
			}
			strokeNeon(vg, neonMag, 1.2f, 3.2f, 50);

			// Internal feedback loop arc (tract -> glottis) along top left
			nvgBeginPath(vg);
			nvgMoveTo(vg, tx0 + 18.f, 11.f);
			nvgBezierTo(vg, tx0, 3.5f, gcx + 18.f, 3.5f, gcx + 6.f, 11.f);
			strokeNeon(vg, nvgRGBA(0xff, 0xb0, 0x40, (uint8_t)(85 + 150 * fb)), 0.9f, 2.4f, 35);

			// HUD micro-labels
			auto font = getLabelFont();
			if (font && font->handle >= 0) {
				nvgFontFaceId(vg, font->handle);
				nvgFontSize(vg, 5.5f);
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
				nvgFillColor(vg, neonMag);
				nvgText(vg, 7.f, 4.f, "LARYNX", NULL);

				nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
				nvgFillColor(vg, neonAmber);
				nvgText(vg, 64.f, 9.5f, "FB", NULL);

				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
				nvgFillColor(vg, neonCyan);
				nvgText(vg, w - 6.f, 4.f, "NOSE", NULL);

				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
				nvgFillColor(vg, neonViolet);
				nvgText(vg, w - 6.f, h - 4.f, "LIPS", NULL);
			}
		}
	};

	static void drawPanel(const rack::widget::Widget::DrawArgs& args, float panelW) {
		NVGcontext* vg = args.vg;
		NVGcolor bg = nvgRGB(0x14, 0x12, 0x16);
		NVGcolor edge = nvgRGB(0x2a, 0x2e, 0x35);
		NVGcolor label = nvgRGB(0xb8, 0xc2, 0xcc);
		NVGcolor neonMag    = nvgRGB(0xe8, 0x4c, 0x9c);
		NVGcolor neonCyan   = nvgRGB(0x48, 0xd8, 0xf8);
		NVGcolor neonCoral  = nvgRGB(0xf0, 0x5c, 0x64);

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
		nvgFillColor(vg, neonMag);
		nvgFill(vg);
		nvgBeginPath(vg);
		nvgMoveTo(vg, 0, 7.5f);
		nvgLineTo(vg, panelW, 7.5f);
		nvgStrokeWidth(vg, 0.8f);
		nvgStrokeColor(vg, nvgRGBA(0xe8, 0x4c, 0x9c, 120));
		nvgStroke(vg);

		float c1 = 44.f, c2 = 120.f, c3 = 196.f;
		float k4[4] = {34.f, 91.f, 149.f, 206.f};
		float rIn1 = 246.f, rIn2 = 292.f, rOut = 338.f;

		// ---- Cyberline Hollow Wireframe Section Divider & Bus Brackets ----
		// 1. Subtle horizontal cyber-divider between Larynx/Coupling (rows 1-2) and Tract Articulators (row 3)
		nvgBeginPath(vg);
		nvgMoveTo(vg, 14.f, 175.f);
		nvgLineTo(vg, 20.f, 171.f);
		nvgLineTo(vg, panelW - 20.f, 171.f);
		nvgLineTo(vg, panelW - 14.f, 175.f);
		nvgStrokeWidth(vg, 0.8f);
		nvgStrokeColor(vg, nvgRGBA(0x98, 0x6c, 0xf8, 80));
		nvgStroke(vg);

		// 2. Hollow chamfered bus bracket around the 3 Tract CV inputs & labels (TNGX CV, TNGY CV, VELUM CV at rIn2)
		nvgBeginPath(vg);
		nvgMoveTo(vg, 14.f, rIn2 - 20.f);
		nvgLineTo(vg, 19.f, rIn2 - 25.f);
		nvgLineTo(vg, panelW - 19.f, rIn2 - 25.f);
		nvgLineTo(vg, panelW - 14.f, rIn2 - 20.f);
		nvgLineTo(vg, panelW - 14.f, rIn2 + 12.f);
		nvgLineTo(vg, panelW - 19.f, rIn2 + 16.f);
		nvgLineTo(vg, 19.f, rIn2 + 16.f);
		nvgLineTo(vg, 14.f, rIn2 + 12.f);
		nvgClosePath(vg);
		nvgStrokeWidth(vg, 0.9f);
		nvgStrokeColor(vg, nvgRGBA(0x98, 0x6c, 0xf8, 90));
		nvgStroke(vg);

		// ---- Hollow Concentric Neon Rings on Inputs & Outputs ----
		auto drawHollowPortRing = [&](float x, float y, NVGcolor col, bool isOutput) {
			if (isOutput) {
				nvgBeginPath(vg);
				nvgCircle(vg, x, y, 14.8f);
				nvgFillColor(vg, nvgRGB(0x0b, 0x0c, 0x10));
				nvgFill(vg);
				strokeNeon(vg, col, 1.1f, 3.0f, 45);
			} else {
				nvgBeginPath(vg);
				nvgCircle(vg, x, y, 14.0f);
				strokeNeon(vg, col, 1.0f, 2.6f, 35);
			}
		};

		// Row 4 Inputs: PITCH (magenta), PRESS (cyan -> matches BREATH PRESS), TENS/POLY (coral)
		drawHollowPortRing(c1, rIn1, neonMag, false);
		drawHollowPortRing(c2, rIn1, neonCyan, false);
		drawHollowPortRing(c3, rIn1, neonCoral, false);
		// Row 5 TNGX/POLY bus entry ring (neon violet)
		drawHollowPortRing(c1, rIn2, nvgRGB(0x9b, 0x6d, 0xff), false);

		// Row 6 Outputs: LIPS (magenta), NOSE (cyan), SOURCE (coral)
		drawHollowPortRing(c1, rOut, neonMag, true);
		drawHollowPortRing(c2, rOut, neonCyan, true);
		drawHollowPortRing(c3, rOut, neonCoral, true);

		nvgFontSize(vg, 8);
		auto font = getLabelFont();
		if (font && font->handle >= 0)
			nvgFontFaceId(vg, font->handle);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
		nvgFillColor(vg, label);
		auto text = [&](float x, float y, const char* s) {
			nvgText(vg, x, y, s, NULL);
		};

		// Row 1 (Glottis Core): TENSION, PRESSURE, FOLD mm, OCT
		text(k4[0], 93, "TENSION");  text(k4[1], 93, "PRESSURE");
		text(k4[2], 93, "FOLD mm");  text(k4[3], 93, "OCT");

		// Row 2 (Coupling & Tract Scale): DRIFT, P-PITCH, FB GAIN, VTL cm
		text(k4[0], 141, "DRIFT");   text(k4[1], 141, "P-PITCH");
		nvgFillColor(vg, nvgRGB(0xf4, 0xb8, 0x68));
		text(k4[2], 141, "FB GAIN");
		nvgFillColor(vg, nvgRGB(0xd8, 0xb8, 0xff));
		text(k4[3], 141, "VTL cm");

		// Row 3 (Tract Articulators): TNG X, TNG Y, VELUM, MUSCLE
		nvgFillColor(vg, nvgRGB(0xd8, 0xb8, 0xff));
		text(k4[0], 185, "TNG X");   text(k4[1], 185, "TNG Y");
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(k4[2], 185, "VELUM");
		nvgFillColor(vg, label);
		text(k4[3], 185, "MUSCLE");

		// Row 4 (Glottis CV Inputs): PITCH, PRESS, TENS/POLY (+ VOX LED at x=158)
		nvgFillColor(vg, nvgRGB(0xff, 0x88, 0xcc));
		text(c1, 230, "PITCH");
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(c2, 230, "PRESS");
		nvgFillColor(vg, nvgRGB(0xf8, 0x82, 0x88));
		text(c3, 230, "TENS/POLY");
		nvgFontSize(vg, 6.f);
		nvgFillColor(vg, nvgRGB(0xf8, 0x82, 0x88));
		text(158.f, 240, "VOX");
		nvgFontSize(vg, 8.f);

		// Row 5 (Articulator CV Inputs): TNGX/POLY, TNGY CV, VELUM CV
		nvgFillColor(vg, nvgRGB(0xd8, 0xb8, 0xff));
		text(c1, 276, "TNGX/POLY");
		text(c2, 276, "TNGY CV");
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(c3, 276, "VELUM CV");

		// Row 6 (Outputs): LIPS, NOSE, SOURCE
		nvgFillColor(vg, nvgRGB(0xff, 0x88, 0xcc));
		text(c1, 321, "LIPS");
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(c2, 321, "NOSE");
		nvgFillColor(vg, nvgRGB(0xf8, 0x82, 0x88));
		text(c3, 321, "SOURCE");

		drawPanelTitle(args, panelW, "VOICE");
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

		// Voice anatomy knobs: a right-click Size range submenu drives BOTH
		// FOLD mm and VTL cm with the same band (one animal, one band).
		struct VFoldKnob : rack::componentlibrary::RoundSmallBlackKnob {
			void appendContextMenu(rack::ui::Menu* menu) override {
				RoundSmallBlackKnob::appendContextMenu(menu);
				menu->addChild(new rack::ui::MenuSeparator);
				appendSizeBandMenu(menu, module, CyberlineVoice::V_FOLDLEN_PARAM, 0,
					deriveBandIndex(module, CyberlineVoice::V_FOLDLEN_PARAM, 0),
					CyberlineVoice::V_VTL_PARAM, 1);
			}
		};
		struct VVtlKnob : rack::componentlibrary::RoundSmallBlackKnob {
			void appendContextMenu(rack::ui::Menu* menu) override {
				RoundSmallBlackKnob::appendContextMenu(menu);
				menu->addChild(new rack::ui::MenuSeparator);
				appendSizeBandMenu(menu, module, CyberlineVoice::V_FOLDLEN_PARAM, 0,
					deriveBandIndex(module, CyberlineVoice::V_FOLDLEN_PARAM, 0),
					CyberlineVoice::V_VTL_PARAM, 1);
			}
		};

		// Top hollow-wireframe fused Glottis + Tract HUD
		{
			auto gauge = new VoiceWireframeGauge(module);
			gauge->box.pos = rack::Vec(16.f, 14.f);
			gauge->box.size = rack::Vec(208.f, 60.f);
			addChild(gauge);
		}

		float c1 = 44.f, c2 = 120.f, c3 = 196.f;
		float k4[4] = {34.f, 91.f, 149.f, 206.f};
		float rKnob1 = 110.f, rKnob2 = 156.f, rKnob3 = 200.f;
		float rIn1 = 246.f, rIn2 = 292.f, rOut = 338.f;

		// Row 1: TENSION, PRESSURE, FOLD mm, OCT
		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(k4[0], rKnob1), module, CyberlineVoice::V_TENSION_PARAM));
		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(k4[1], rKnob1), module, CyberlineVoice::V_PRESSURE_PARAM));
		addParam(rack::createParamCentered<VFoldKnob>(rack::Vec(k4[2], rKnob1), module, CyberlineVoice::V_FOLDLEN_PARAM));
		addParam(rack::createParamCentered<OctEncoderKnob>(rack::Vec(k4[3], rKnob1), module, CyberlineVoice::V_PITCHOCT_PARAM));

		// Row 2: DRIFT, P-PITCH, FB GAIN, VTL cm
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(k4[0], rKnob2), module, CyberlineVoice::V_DRIFT_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(k4[1], rKnob2), module, CyberlineVoice::V_PCOUPLE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(k4[2], rKnob2), module, CyberlineVoice::V_FB_PARAM));
		addParam(rack::createParamCentered<VVtlKnob>(rack::Vec(k4[3], rKnob2), module, CyberlineVoice::V_VTL_PARAM));

		// Row 3: TNG X, TNG Y, VELUM, MUSCLE
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(k4[0], rKnob3), module, CyberlineVoice::V_TONGUEX_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(k4[1], rKnob3), module, CyberlineVoice::V_TONGUEY_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(k4[2], rKnob3), module, CyberlineVoice::V_VELUM_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(k4[3], rKnob3), module, CyberlineVoice::V_SPEED_PARAM));

		// Voicing light cleanly positioned between PRESS and TENS inputs
		addChild(rack::createLightCentered<rack::componentlibrary::SmallLight<rack::componentlibrary::RedLight>>(
			rack::Vec(158.f, rIn1 + 2.f), module, CyberlineVoice::V_PHASE_LIGHT));

		// Row 4: PITCH, PRESS, TENS
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c1, rIn1), module, CyberlineVoice::V_PITCH_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c2, rIn1), module, CyberlineVoice::V_PRESSURE_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c3, rIn1), module, CyberlineVoice::V_TENSION_INPUT));

		// Row 5: TNGX CV, TNGY CV, VELUM CV
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c1, rIn2), module, CyberlineVoice::V_TONGUEX_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c2, rIn2), module, CyberlineVoice::V_TONGUEY_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c3, rIn2), module, CyberlineVoice::V_VELUM_INPUT));

		// Row 6: LIPS, NOSE, SOURCE
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c1, rOut), module, CyberlineVoice::V_LIPS_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c2, rOut), module, CyberlineVoice::V_NOSE_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c3, rOut), module, CyberlineVoice::V_VOICED_OUTPUT));
	}

	void appendContextMenu(rack::ui::Menu* menu) override {
		ModuleWidget::appendContextMenu(menu);
		// One Size range submenu drives BOTH anatomy knobs (FOLD mm + VTL cm)
		appendSizeBandMenu(menu, module, CyberlineVoice::V_FOLDLEN_PARAM, 0,
			deriveBandIndex(module, CyberlineVoice::V_FOLDLEN_PARAM, 0),
			CyberlineVoice::V_VTL_PARAM, 1);
		menu->addChild(new rack::ui::MenuSeparator);
		appendQualityMenu(menu, module, CyberlineVoice::V_QUALITY_PARAM);
	}

	void draw(const DrawArgs& args) override {
		drawPanel(args, PANEL_W);
		ModuleWidget::draw(args);
		drawCpuMeterText(args, static_cast<CyberlineVoice*>(module));
	}
};

} // namespace cyberline

namespace cyberline {
rack::plugin::Model* modelCyberlineVoice =
	rack::createModel<CyberlineVoice, CyberlineVoiceWidget>("cyberline-voice");
} // namespace cyberline
