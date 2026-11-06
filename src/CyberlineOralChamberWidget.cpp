// CyberlineOralChamberWidget — programmatic panel (no SVG assets). 16 HP.
// Three-column grid; label 16 px above its widget.

#include "CyberlineOralChamber.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineOralChamberWidget : rack::app::ModuleWidget {
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

	// Cyberline hollow-wireframe waveguide display:
	// Renders the Kelly-Lochbaum vocal tract (glottis -> pharynx -> palate -> lips)
	// and branching nasal cavity as neon vector line art with hollow cross-sectional
	// waveguide ribs, tongue reticle, velum gate, and injection/constriction markers.
	struct TractWireframeGauge : rack::widget::Widget {
		CyberlineOralChamber* module;
		TractWireframeGauge(CyberlineOralChamber* m) : module(m) {}

		void draw(const DrawArgs& args) override {
			float w = box.size.x, h = box.size.y;
			NVGcontext* vg = args.vg;

			NVGcolor neonViolet = nvgRGB(0x9b, 0x6d, 0xff);
			NVGcolor neonCyan   = nvgRGB(0x40, 0xe0, 0xff);
			NVGcolor neonMag    = nvgRGB(0xff, 0x48, 0xb0);
			NVGcolor neonAmber  = nvgRGB(0xff, 0xb0, 0x40);

			// Hollow chamfered cyber-HUD frame (dark void interior + neon chamfered border)
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
			nvgFillColor(vg, nvgRGB(0x0c, 0x0d, 0x12));
			nvgFill(vg);
			strokeNeon(vg, nvgRGBA(0x7a, 0x5a, 0xe0, 200), 1.0f, 2.8f, 35);

			// Corner HUD accent brackets
			nvgBeginPath(vg);
			nvgMoveTo(vg, 0.f, ch + 8.f); nvgLineTo(vg, 0.f, ch); nvgLineTo(vg, ch, 0.f); nvgLineTo(vg, ch + 8.f, 0.f);
			nvgMoveTo(vg, w - ch - 8.f, 0.f); nvgLineTo(vg, w - ch, 0.f); nvgLineTo(vg, w, ch); nvgLineTo(vg, w, ch + 8.f);
			nvgMoveTo(vg, 0.f, h - ch - 8.f); nvgLineTo(vg, 0.f, h - ch); nvgLineTo(vg, ch, h); nvgLineTo(vg, ch + 8.f, h);
			nvgMoveTo(vg, w - ch - 8.f, h); nvgLineTo(vg, w - ch, h); nvgLineTo(vg, w, h - ch); nvgLineTo(vg, w, h - ch - 8.f);
			nvgStrokeWidth(vg, 1.4f);
			nvgStrokeColor(vg, neonCyan);
			nvgStroke(vg);

			// Subtle vector perspective grid
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
			nvgStrokeColor(vg, nvgRGBA(0x4a, 0x3c, 0x80, 55));
			nvgStroke(vg);

			// Read live tract state (or synthesize default profile in module browser)
			int n = 44;
			int noseStart = 17;
			float velum = 0.15f;
			float tngX = 0.5f, tngY = 0.5f;
			float injPos = 0.f;
			if (module && module->tract.initialized && module->tract.n >= 12) {
				n = module->tract.n;
				noseStart = module->tract.noseStart;
				velum = rack::math::clamp(module->tract.velum, 0.f, 1.f);
				float span = std::fmax(1.f, (float)(module->tract.tipStart - module->tract.bladeStart));
				tngX = rack::math::clamp((module->tract.tongueIndex - module->tract.bladeStart) / span, 0.f, 1.f);
				tngY = rack::math::clamp((module->tract.tongueDiameter - 1.5f) / 3.5f, 0.f, 1.f);
				injPos = rack::math::clamp(
					module->params[CyberlineOralChamber::INJPOS_PARAM].getValue()
					+ module->inputs[CyberlineOralChamber::INJPOS_INPUT].getVoltage(),
					0.f, 10.f) * 0.1f;
			}

			auto getOralDiam = [&](float normPos) -> float {
				if (module && module->tract.initialized && module->tract.n >= 12) {
					int idx = rack::math::clamp((int)(normPos * (n - 1)), 0, n - 1);
					float d = module->tract.diameter[idx];
					return std::isfinite(d) ? rack::math::clamp(d, 0.f, 3.5f) : 1.5f;
				}
				// Default preview profile: narrow glottal entry -> pharynx -> tongue arch -> lips
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

			float x0 = 22.f, x1 = w - 22.f;
			float tractW = x1 - x0;
			float oralCenterY = h * 0.63f;
			float scaleD = 5.4f;

			// ---- 1. Nasal Cavity Branch (Upper Hollow Wireframe Waveguide) ----
			float noseStartNorm = (float) noseStart / (float) std::max(n, 1);
			float nx0 = x0 + noseStartNorm * tractW;
			float nx1 = x1 - 4.f;
			float nw = nx1 - nx0;
			float noseBaseY = h * 0.26f;

			// Nasal wireframe ribs
			int nRibs = 12;
			nvgBeginPath(vg);
			for (int i = 0; i <= nRibs; i++) {
				float u = (float) i / (float) nRibs;
				float arch = std::sin(u * (float) M_PI);
				float nd = (0.35f + 0.85f * arch) * (0.45f + 0.55f * (i == 0 ? velum * 2.f : 1.f));
				float rx = nx0 + u * nw;
				float topY = noseBaseY - nd * 3.8f;
				float botY = noseBaseY + nd * 2.2f;
				nvgMoveTo(vg, rx, topY);
				nvgLineTo(vg, rx, botY);
			}
			nvgStrokeWidth(vg, 0.7f);
			nvgStrokeColor(vg, nvgRGBA(0x40, 0xe0, 0xff, (uint8_t)(60 + 110 * velum)));
			nvgStroke(vg);

			// Nasal roof & floor hollow contour lines
			nvgBeginPath(vg);
			for (int i = 0; i <= nRibs; i++) {
				float u = (float) i / (float) nRibs;
				float arch = std::sin(u * (float) M_PI);
				float nd = (0.35f + 0.85f * arch) * (0.45f + 0.55f * (i == 0 ? velum * 2.f : 1.f));
				float rx = nx0 + u * nw;
				float topY = noseBaseY - nd * 3.8f;
				if (i == 0) nvgMoveTo(vg, rx, topY);
				else nvgLineTo(vg, rx, topY);
			}
			for (int i = 0; i <= nRibs; i++) {
				float u = (float) i / (float) nRibs;
				float arch = std::sin(u * (float) M_PI);
				float nd = (0.35f + 0.85f * arch) * (0.45f + 0.55f * (i == 0 ? velum * 2.f : 1.f));
				float rx = nx0 + u * nw;
				float botY = noseBaseY + nd * 2.2f;
				if (i == 0) nvgMoveTo(vg, rx, botY);
				else nvgLineTo(vg, rx, botY);
			}
			strokeNeon(vg, nvgRGBA(0x40, 0xe0, 0xff, (uint8_t)(120 + 120 * velum)), 1.0f, 2.8f, 35);

			// Velopharyngeal junction branch lines connecting oral roof up to nasal floor
			float jTopOralY = oralCenterY - std::sin(noseStartNorm * (float) M_PI) * 3.2f
				- getOralDiam(noseStartNorm) * scaleD * 0.58f;
			nvgBeginPath(vg);
			nvgMoveTo(vg, nx0 - 4.f, jTopOralY);
			nvgLineTo(vg, nx0, noseBaseY + 0.8f);
			nvgMoveTo(vg, nx0 + 5.f, jTopOralY + 0.5f);
			nvgLineTo(vg, nx0 + 4.f, noseBaseY + 1.2f);
			strokeNeon(vg, nvgRGBA(0x40, 0xe0, 0xff, (uint8_t)(110 + 130 * velum)), 1.0f, 2.5f, 40);

			// ---- 2. Main Oral Waveguide (Hollow Wireframe Ribs + Roof/Floor Contours) ----
			int oRibs = 28;
			nvgBeginPath(vg);
			for (int i = 0; i <= oRibs; i++) {
				float u = (float) i / (float) oRibs;
				float d = getOralDiam(u);
				float rx = x0 + u * tractW;
				float roofBias = std::sin(u * (float) M_PI) * 3.2f;
				float topY = oralCenterY - roofBias - d * scaleD * 0.58f;
				float botY = oralCenterY - roofBias + d * scaleD * 0.42f;
				nvgMoveTo(vg, rx, topY);
				nvgLineTo(vg, rx, botY);
			}
			nvgStrokeWidth(vg, 0.75f);
			nvgStrokeColor(vg, nvgRGBA(0x9b, 0x6d, 0xff, 95));
			nvgStroke(vg);

			// Palate / upper wall contour (neon violet)
			nvgBeginPath(vg);
			for (int i = 0; i <= oRibs; i++) {
				float u = (float) i / (float) oRibs;
				float d = getOralDiam(u);
				float rx = x0 + u * tractW;
				float roofBias = std::sin(u * (float) M_PI) * 3.2f;
				float topY = oralCenterY - roofBias - d * scaleD * 0.58f;
				if (i == 0) nvgMoveTo(vg, rx, topY);
				else nvgLineTo(vg, rx, topY);
			}
			strokeNeon(vg, neonViolet, 1.3f, 3.6f, 55);

			// Tongue / lower wall contour (neon magenta-cyan)
			nvgBeginPath(vg);
			for (int i = 0; i <= oRibs; i++) {
				float u = (float) i / (float) oRibs;
				float d = getOralDiam(u);
				float rx = x0 + u * tractW;
				float roofBias = std::sin(u * (float) M_PI) * 3.2f;
				float botY = oralCenterY - roofBias + d * scaleD * 0.42f;
				if (i == 0) nvgMoveTo(vg, rx, botY);
				else nvgLineTo(vg, rx, botY);
			}
			strokeNeon(vg, neonMag, 1.3f, 3.6f, 55);

			// ---- 3. Hollow Tongue Reticle & Injection Node ----
			float tngPosNorm = 0.28f + 0.45f * tngX;
			float tx = x0 + tngPosNorm * tractW;
			float td = getOralDiam(tngPosNorm);
			float ty = oralCenterY - std::sin(tngPosNorm * (float) M_PI) * 3.2f + td * scaleD * 0.42f + 2.5f;
			// Hollow diamond reticle at tongue control point
			nvgBeginPath(vg);
			nvgMoveTo(vg, tx, ty - 3.5f);
			nvgLineTo(vg, tx + 3.5f, ty);
			nvgLineTo(vg, tx, ty + 3.5f);
			nvgLineTo(vg, tx - 3.5f, ty);
			nvgClosePath(vg);
			strokeNeon(vg, neonCyan, 1.0f, 2.5f, 45);

			// Injection position vertical marker (if INJ POS > 0)
			if (injPos > 0.02f) {
				float ix = x0 + injPos * tractW;
				nvgBeginPath(vg);
				nvgMoveTo(vg, ix, oralCenterY - 14.f);
				nvgLineTo(vg, ix, oralCenterY + 10.f);
				strokeNeon(vg, neonAmber, 1.0f, 2.5f, 45);
			}

			// HUD Endpoint Callouts: GLOTTIS (left), NOSE (top-right), LIPS (bottom-right)
			auto font = getLabelFont();
			if (font && font->handle >= 0) {
				nvgFontFaceId(vg, font->handle);
				nvgFontSize(vg, 5.5f);
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
				nvgFillColor(vg, nvgRGB(0xff, 0x68, 0x78));
				nvgText(vg, 6.f, 5.f, "SRC", NULL);

				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
				nvgFillColor(vg, neonCyan);
				nvgText(vg, w - 6.f, 5.f, "NOSE", NULL);

				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
				nvgFillColor(vg, neonViolet);
				nvgText(vg, w - 6.f, h - 4.f, "LIPS", NULL);

				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
				nvgFillColor(vg, neonAmber);
				nvgText(vg, 6.f, h - 4.f, "PROBE", NULL);
			}
		}
	};

	static void drawPanel(const rack::widget::Widget::DrawArgs& args, float panelW) {
		NVGcontext* vg = args.vg;
		NVGcolor bg = nvgRGB(0x13, 0x14, 0x1a);
		NVGcolor edge = nvgRGB(0x2a, 0x2e, 0x38);
		NVGcolor label = nvgRGB(0xb8, 0xc2, 0xcc);
		NVGcolor neonViolet = nvgRGB(0x8e, 0x62, 0xf8);
		NVGcolor neonCyan   = nvgRGB(0x48, 0xd8, 0xf8);
		NVGcolor neonMag    = nvgRGB(0xf4, 0x52, 0xb0);
		NVGcolor neonCoral  = nvgRGB(0xf0, 0x5c, 0x64); // matches LARYNX VOICED
		NVGcolor neonAmber  = nvgRGB(0xec, 0xa4, 0x48); // matches LARYNX FB IN

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
		nvgStrokeColor(vg, nvgRGBA(0x8e, 0x62, 0xf8, 120));
		nvgStroke(vg);

		float c1 = 44.f, c2 = 120.f, c3 = 196.f;
		float k4[4] = {34.f, 91.f, 149.f, 206.f};
		float cvX[6] = {24.f, 62.4f, 100.8f, 139.2f, 177.6f, 216.f};
		float rKnob3 = 200.f;
		float rIn1 = 246.f, rIn2 = 292.f, rOut = 338.f;

		// ---- Cyberline Hollow Wireframe Section Brackets & Signal Traces ----
		// 1. Hollow chamfered bus bracket around the 6 articulation CV inputs & labels (rIn2)
		nvgBeginPath(vg);
		nvgMoveTo(vg, 7.f, rIn2 - 20.f);
		nvgLineTo(vg, 12.f, rIn2 - 25.f);
		nvgLineTo(vg, panelW - 12.f, rIn2 - 25.f);
		nvgLineTo(vg, panelW - 7.f, rIn2 - 20.f);
		nvgLineTo(vg, panelW - 7.f, rIn2 + 12.f);
		nvgLineTo(vg, panelW - 12.f, rIn2 + 16.f);
		nvgLineTo(vg, 12.f, rIn2 + 16.f);
		nvgLineTo(vg, 7.f, rIn2 + 12.f);
		nvgClosePath(vg);
		nvgStrokeWidth(vg, 0.9f);
		nvgStrokeColor(vg, nvgRGBA(0x8e, 0x62, 0xf8, 90));
		nvgStroke(vg);

		// 2. 45-degree chamfered trace linking INJ POS knob (k4[3], rKnob3) -> INJECT (k4[3], rIn1) -> INJPOS CV (k4[2], rIn1)
		// Routed around the right side of INJECT so it never crosses the "INJECT" label text
		nvgBeginPath(vg);
		nvgMoveTo(vg, k4[3] + 9.f, rKnob3 + 9.f);
		nvgLineTo(vg, k4[3] + 15.f, rKnob3 + 15.f);
		nvgLineTo(vg, k4[3] + 15.f, rIn1 - 5.f);
		nvgLineTo(vg, k4[3] + 14.0f, rIn1);
		nvgMoveTo(vg, k4[3] - 14.0f, rIn1);
		nvgLineTo(vg, k4[2] + 14.0f, rIn1);
		nvgStrokeWidth(vg, 1.0f);
		nvgStrokeColor(vg, nvgRGBA(0x48, 0xd8, 0xf8, 120));
		nvgStroke(vg);

		// ---- Hollow Concentric Neon Rings on Key Inputs & All Outputs ----
		auto drawHollowPortRing = [&](float x, float y, NVGcolor col, bool isOutput) {
			if (isOutput) {
				// Hollow double-ring neon output pad
				nvgBeginPath(vg);
				nvgCircle(vg, x, y, 14.8f);
				nvgFillColor(vg, nvgRGB(0x0b, 0x0c, 0x10));
				nvgFill(vg);
				strokeNeon(vg, col, 1.1f, 3.0f, 42);
			} else {
				nvgBeginPath(vg);
				nvgCircle(vg, x, y, 14.0f);
				strokeNeon(vg, col, 1.0f, 2.6f, 35);
			}
		};

		// SRC input (matches LARYNX VOICED crimson-coral)
		drawHollowPortRing(k4[0], rIn1, neonCoral, false);
		// VTL CV input (neon violet, matching VTL cm & tract geometry)
		drawHollowPortRing(k4[1], rIn1, neonViolet, false);
		// INJPOS CV & INJECT inputs (neon cyan)
		drawHollowPortRing(k4[2], rIn1, neonCyan, false);
		drawHollowPortRing(k4[3], rIn1, neonCyan, false);
		// TNGX/POLY bus entry input (neon violet ring inside the articulation CV bus)
		drawHollowPortRing(cvX[0], rIn2, neonViolet, false);

		// 4 Outputs: LIPS (violet), NOSE (cyan), SHAPE (magenta), PROBE (amber -> LARYNX FB IN)
		drawHollowPortRing(k4[0], rOut, neonViolet, true);
		drawHollowPortRing(k4[1], rOut, neonCyan, true);
		drawHollowPortRing(k4[2], rOut, neonMag, true);
		drawHollowPortRing(k4[3], rOut, neonAmber, true);

		nvgFontSize(vg, 8);
		auto font = getLabelFont();
		if (font && font->handle >= 0)
			nvgFontFaceId(vg, font->handle);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
		nvgFillColor(vg, label);
		auto text = [&](float x, float y, const char* s) {
			nvgText(vg, x, y, s, NULL);
		};

		// Row 1 knobs: VTL cm, WALL, MUSCLE
		nvgFillColor(vg, nvgRGB(0xb8, 0x98, 0xff));
		text(c1, 93, "VTL cm");
		nvgFillColor(vg, label);
		text(c2, 93, "WALL");     text(c3, 93, "MUSCLE");
		// Row 2 knobs: TNG X, TNG Y, VELUM
		nvgFillColor(vg, nvgRGB(0xd8, 0xb8, 0xff));
		text(c1, 141, "TNG X");   text(c2, 141, "TNG Y");
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(c3, 141, "VELUM");
		// Row 3 knobs: C.POS, C.WIDTH, FRIC, INJ POS
		nvgFillColor(vg, label);
		text(k4[0], 185, "C.POS"); text(k4[1], 185, "C.WIDTH"); text(k4[2], 185, "FRIC");
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(k4[3], 185, "INJ POS");

		// Row 4 inputs: SRC, VTL CV, INJPOS CV, INJECT
		nvgFillColor(vg, nvgRGB(0xf8, 0x82, 0x88));
		text(k4[0], 230, "SRC");
		nvgFillColor(vg, nvgRGB(0xb8, 0x98, 0xff));
		text(k4[1], 230, "VTL CV");
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(k4[2], 230, "INJPOS CV");
		text(k4[3], 230, "INJECT");

		// Row 5 articulation CV inputs: TNGX/POLY, TNGY, VEL, CPOS, CWID, FRIC
		nvgFontSize(vg, 6.2f);
		nvgFillColor(vg, nvgRGB(0xd8, 0xb8, 0xff));
		text(cvX[0] + 1.f, 277, "TNGX/POLY");
		nvgFontSize(vg, 7.2f);
		nvgFillColor(vg, nvgRGB(0xc8, 0xb4, 0xec));
		text(cvX[1], 277, "TNGY");
		text(cvX[2], 277, "VEL");  text(cvX[3], 277, "CPOS");
		text(cvX[4], 277, "CWID"); text(cvX[5], 277, "FRIC");
		nvgFontSize(vg, 8.f);

		// Row 6 outputs: LIPS, NOSE, SHAPE, PROBE
		nvgFillColor(vg, nvgRGB(0xb8, 0x98, 0xff));
		text(k4[0], 321, "LIPS");
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(k4[1], 321, "NOSE");
		nvgFillColor(vg, nvgRGB(0xff, 0x88, 0xcc));
		text(k4[2], 321, "SHAPE");
		nvgFillColor(vg, nvgRGB(0xf4, 0xb8, 0x68));
		text(k4[3], 321, "PROBE");

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

		// Top hollow-wireframe vocal-tract display
		{
			auto gauge = new TractWireframeGauge(module);
			gauge->box.pos = rack::Vec(16.f, 14.f);
			gauge->box.size = rack::Vec(208.f, 60.f);
			addChild(gauge);
		}

		float c1 = 44.f, c2 = 120.f, c3 = 196.f;
		float k4[4] = {34.f, 91.f, 149.f, 206.f};
		float cvX[6] = {24.f, 62.4f, 100.8f, 139.2f, 177.6f, 216.f};
		float rKnob1 = 110.f, rKnob2 = 156.f, rKnob3 = 200.f;
		float rIn1 = 246.f, rIn2 = 292.f, rOut = 338.f;

		// Row 1: VTL cm, WALL, MUSCLE
		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(c1, rKnob1), module, CyberlineOralChamber::VTL_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, rKnob1), module, CyberlineOralChamber::WALL_PARAM));
		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(c3, rKnob1), module, CyberlineOralChamber::SPEED_PARAM));

		// Row 2: TNG X, TNG Y, VELUM
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, rKnob2), module, CyberlineOralChamber::TONGUEX_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, rKnob2), module, CyberlineOralChamber::TONGUEY_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c3, rKnob2), module, CyberlineOralChamber::VELUM_PARAM));

		// Row 3: C.POS, C.WIDTH, FRIC, INJ POS
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(k4[0], rKnob3), module, CyberlineOralChamber::CONSTRPOS_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(k4[1], rKnob3), module, CyberlineOralChamber::CONSTRW_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(k4[2], rKnob3), module, CyberlineOralChamber::FRIC_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(k4[3], rKnob3), module, CyberlineOralChamber::INJPOS_PARAM));

		// Row 4: SRC, VTL CV, INJPOS CV, INJECT
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(k4[0], rIn1), module, CyberlineOralChamber::SOURCE_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(k4[1], rIn1), module, CyberlineOralChamber::VTL_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(k4[2], rIn1), module, CyberlineOralChamber::INJPOS_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(k4[3], rIn1), module, CyberlineOralChamber::INJECT_INPUT));

		// Row 5: 6 articulation CV inputs
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cvX[0], rIn2), module, CyberlineOralChamber::TONGUEX_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cvX[1], rIn2), module, CyberlineOralChamber::TONGUEY_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cvX[2], rIn2), module, CyberlineOralChamber::VELUM_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cvX[3], rIn2), module, CyberlineOralChamber::CONSTRPOS_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cvX[4], rIn2), module, CyberlineOralChamber::CONSTRW_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cvX[5], rIn2), module, CyberlineOralChamber::FRIC_INPUT));

		// Row 6: 4 outputs (LIPS, NOSE, SHAPE, PROBE)
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(k4[0], rOut), module, CyberlineOralChamber::LIPS_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(k4[1], rOut), module, CyberlineOralChamber::NOSE_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(k4[2], rOut), module, CyberlineOralChamber::SHAPE_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(k4[3], rOut), module, CyberlineOralChamber::PROBE_OUTPUT));
	}

	void appendContextMenu(rack::ui::Menu* menu) override {
		ModuleWidget::appendContextMenu(menu);
		appendQualityMenu(menu, module, CyberlineOralChamber::QUALITY_PARAM);
	}

	void draw(const DrawArgs& args) override {
		drawPanel(args, PANEL_W);
		ModuleWidget::draw(args);
		drawCpuMeterText(args, static_cast<CyberlineOralChamber*>(module));
	}
};

} // namespace cyberline

namespace cyberline {
rack::plugin::Model* modelCyberlineOralChamber =
	rack::createModel<CyberlineOralChamber, CyberlineOralChamberWidget>("cyberline-oral-chamber");
} // namespace cyberline
