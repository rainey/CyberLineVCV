// CyberlineLarynxWidget — programmatic panel (no SVG assets).
// 12 HP. Grid: label 16 px above its widget; rows spread over 380 px.

#include "CyberlineLarynx.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineLarynxWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 12 * 15.f; // 12 HP in px

	// Helper: neon double-stroke (outer soft bloom + crisp inner vector core)
	static void strokeNeon(NVGcontext* vg, NVGcolor col, float coreW = 1.2f, float glowW = 3.6f, int glowAlpha = 50) {
		nvgStrokeWidth(vg, glowW);
		nvgStrokeColor(vg, nvgRGBA(col.r * 255, col.g * 255, col.b * 255, glowAlpha));
		nvgStroke(vg);
		nvgStrokeWidth(vg, coreW);
		nvgStrokeColor(vg, col);
		nvgStroke(vg);
	}

	// Laryngoscope & LF-glottal-waveform hollow neon wireframe header:
	// - Left: smooth curved hollow wireframe endoscopic vocal folds (rima glottidis)
	//   whose length scales with FOLD mm, abduction/adduction gap scales with
	//   TENSION, and glottal aperture glows with subglottal PRESSURE / phonation.
	// - Right: live Fant-Liljencrants-Lin (LF) glottal pulse derivative curve
	//   showing the open phase and sharp return spike shaped by TENSION & FOLD.
	struct GlottisGauge : rack::widget::Widget {
		CyberlineLarynx* module;
		GlottisGauge(CyberlineLarynx* m) : module(m) {}

		void draw(const DrawArgs& args) override {
			NVGcontext* vg = args.vg;
			float tens = 0.6f, amp = 0.8f, foldMm = 14.f, te = 0.52f;
			if (module && std::isfinite(module->glottis.curTenseness)) {
				tens = rack::math::clamp(module->glottis.curTenseness, 0.f, 1.f);
				amp = rack::math::clamp(module->glottis.intensity, 0.f, 1.f);
				foldMm = rack::math::clamp(module->glottis.targetFoldLen, 2.f, 45.f);
				if (module->glottis.Te > 0.05f && module->glottis.Te < 0.98f)
					te = module->glottis.Te;
			}
			float w = box.size.x, h = box.size.y;

			NVGcolor neonCoral = nvgRGB(0xff, 0x54, 0x68);
			NVGcolor neonRose  = nvgRGB(0xe8, 0x68, 0x90);

			// Smoothly rounded neon HUD bezel (less angular, organic curves)
			nvgBeginPath(vg);
			nvgRoundedRect(vg, 0.f, 0.f, w, h, 7.f);
			nvgFillColor(vg, nvgRGB(0x0c, 0x0d, 0x12));
			nvgFill(vg);
			strokeNeon(vg, nvgRGBA(0xe0, 0x54, 0x5a, 185), 1.0f, 2.8f, 35);

			// Subtle vector grid
			nvgBeginPath(vg);
			for (float gx = 21.f; gx < w - 8.f; gx += 21.f) {
				nvgMoveTo(vg, gx, 3.f);
				nvgLineTo(vg, gx, h - 3.f);
			}
			for (float gy = 14.f; gy < h - 6.f; gy += 14.f) {
				nvgMoveTo(vg, 4.f, gy);
				nvgLineTo(vg, w - 4.f, gy);
			}
			nvgStrokeWidth(vg, 0.5f);
			nvgStrokeColor(vg, nvgRGBA(0x60, 0x38, 0x48, 55));
			nvgStroke(vg);

			// Vertical divider between laryngoscope (left) and LF wave (right)
			float splitX = w * 0.46f;
			nvgBeginPath(vg);
			nvgMoveTo(vg, splitX, 6.f);
			nvgLineTo(vg, splitX, h - 6.f);
			nvgStrokeWidth(vg, 0.8f);
			nvgStrokeColor(vg, nvgRGBA(0x68, 0x42, 0x52, 110));
			nvgStroke(vg);

			// ---- Left Pane: Curved Hollow Wireframe Vocal Folds ----
			float lcx = splitX * 0.5f;
			float lcy = h * 0.57f;
			float lenNorm = rack::math::clamp((foldMm - 2.f) / 30.f, 0.f, 1.f);
			float halfLen = 9.5f + 5.5f * lenNorm;
			float chinkW = (1.3f + 4.8f * (1.f - tens)) * (0.4f + 0.6f * amp);

			// Outer hollow laryngeal ring (smooth ellipse)
			nvgBeginPath(vg);
			nvgEllipse(vg, lcx, lcy, 16.5f, halfLen + 3.5f);
			strokeNeon(vg, nvgRGBA(0xb8, 0x48, 0x62, 155), 0.9f, 2.5f, 30);

			// Epiglottis crescent at anterior (top) commissure
			nvgBeginPath(vg);
			nvgMoveTo(vg, lcx - 10.5f, lcy - halfLen - 1.f);
			nvgBezierTo(vg, lcx - 5.5f, lcy - halfLen - 5.5f,
				lcx + 5.5f, lcy - halfLen - 5.5f, lcx + 10.5f, lcy - halfLen - 1.f);
			nvgLineCap(vg, NVG_ROUND);
			strokeNeon(vg, neonCoral, 1.3f, 3.2f, 45);

			// Curved horizontal mucosal wireframe ribs across the left & right vocal folds
			nvgBeginPath(vg);
			for (int r = -2; r <= 2; r++) {
				float u = r * 0.34f; // -0.68 .. +0.68
				float ry = lcy + u * halfLen;
				float bulge = std::sqrt(std::fmax(0.f, 1.f - u * u));
				float outerX = 11.5f * bulge;
				float innerX = chinkW * bulge;
				// Left fold rib
				nvgMoveTo(vg, lcx - outerX, ry);
				nvgQuadTo(vg, lcx - (outerX + innerX) * 0.5f, ry - 0.8f, lcx - innerX, ry);
				// Right fold rib
				nvgMoveTo(vg, lcx + innerX, ry);
				nvgQuadTo(vg, lcx + (outerX + innerX) * 0.5f, ry - 0.8f, lcx + outerX, ry);
			}
			nvgStrokeWidth(vg, 0.7f);
			nvgStrokeColor(vg, nvgRGBA(0xf0, 0x68, 0x88, 105));
			nvgStroke(vg);

			// Outer curved lateral contours of the left & right vocal folds
			nvgBeginPath(vg);
			nvgMoveTo(vg, lcx, lcy - halfLen);
			nvgBezierTo(vg, lcx - 13.f, lcy - halfLen * 0.55f,
				lcx - 13.f, lcy + halfLen * 0.55f, lcx - 1.2f, lcy + halfLen);
			nvgMoveTo(vg, lcx, lcy - halfLen);
			nvgBezierTo(vg, lcx + 13.f, lcy - halfLen * 0.55f,
				lcx + 13.f, lcy + halfLen * 0.55f, lcx + 1.2f, lcy + halfLen);
			strokeNeon(vg, neonRose, 1.1f, 3.0f, 45);

			// Rima glottidis (inner curved vocal-fold edges around the glottal opening)
			NVGcolor slitCol = nvgRGB(
				(uint8_t)(0x40 + (0xff - 0x40) * tens),
				(uint8_t)(0xe0 + (0x54 - 0xe0) * tens),
				(uint8_t)(0xff + (0x68 - 0xff) * tens));
			nvgBeginPath(vg);
			nvgMoveTo(vg, lcx, lcy - halfLen + 0.4f);
			nvgBezierTo(vg, lcx - chinkW * 1.25f, lcy - halfLen * 0.35f,
				lcx - chinkW * 1.4f, lcy + halfLen * 0.45f,
				lcx, lcy + halfLen - 0.4f);
			nvgBezierTo(vg, lcx + chinkW * 1.4f, lcy + halfLen * 0.45f,
				lcx + chinkW * 1.25f, lcy - halfLen * 0.35f,
				lcx, lcy - halfLen + 0.4f);
			nvgClosePath(vg);
			nvgFillColor(vg, nvgRGBA(slitCol.r * 255, slitCol.g * 255, slitCol.b * 255, (uint8_t)(30 + 45 * amp)));
			nvgFill(vg);
			strokeNeon(vg, slitCol, 1.2f, 3.4f, 60);

			// ---- Right Pane: Live LF Glottal Pulse Derivative Waveform ----
			float wx0 = splitX + 7.f;
			float wx1 = w - 7.f;
			float ww = wx1 - wx0;
			float wyMid = h * 0.54f;

			// Zero-axis baseline
			nvgBeginPath(vg);
			nvgMoveTo(vg, wx0, wyMid);
			nvgLineTo(vg, wx1, wyMid);
			nvgStrokeWidth(vg, 0.65f);
			nvgStrokeColor(vg, nvgRGBA(0x68, 0x58, 0x74, 100));
			nvgStroke(vg);

			// Plot one LF glottal cycle (open phase 0..te, return spike te..1)
			nvgBeginPath(vg);
			int N = 36;
			float ampScale = (0.25f + 0.75f * amp);
			float sharpness = 2.5f + 8.5f * tens;
			for (int i = 0; i <= N; i++) {
				float p = (float) i / (float) N;
				float yVal = 0.f;
				if (p <= te) {
					float u = p / std::fmax(te, 0.1f);
					yVal = std::sin(u * (float) M_PI) * std::exp(1.2f * (u - 0.5f));
					if (u > 0.72f) {
						float d = (u - 0.72f) / 0.28f;
						yVal -= d * d * (1.2f + 1.1f * tens);
					}
				} else {
					float r = (p - te) / std::fmax(1.f - te, 0.05f);
					float negPeak = -(0.55f + 0.95f * tens);
					yVal = negPeak * (std::exp(-sharpness * r) - std::exp(-sharpness))
						/ (1.f - std::exp(-sharpness));
				}
				float px = wx0 + p * ww;
				float py = wyMid - yVal * ampScale * 13.5f;
				py = rack::math::clamp(py, 9.f, h - 7.f);
				if (i == 0) nvgMoveTo(vg, px, py);
				else nvgLineTo(vg, px, py);
			}
			nvgLineJoin(vg, NVG_ROUND);
			strokeNeon(vg, neonCoral, 1.3f, 3.6f, 55);

			// Aspiration noise-gate envelope trace (neon cyan at bottom of right pane)
			nvgBeginPath(vg);
			float nyBase = h - 7.f;
			float nDepth = (1.f - 0.65f * std::sqrt(tens)) * ampScale * 7.f;
			nvgMoveTo(vg, wx0, nyBase - nDepth * 0.3f);
			nvgQuadTo(vg, wx0 + te * ww * 0.5f, nyBase - nDepth * 1.3f,
				wx0 + te * ww, nyBase - nDepth * 0.3f);
			nvgLineTo(vg, wx1, nyBase - nDepth * 0.2f);
			strokeNeon(vg, nvgRGBA(0x40, 0xe0, 0xff, 190), 0.95f, 2.5f, 35);

			// Pane headers
			auto font = getLabelFont();
			if (font && font->handle >= 0) {
				nvgFontFaceId(vg, font->handle);
				nvgFontSize(vg, 5.5f);
				nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
				nvgFillColor(vg, nvgRGBA(0xf4, 0x90, 0xa8, 195));
				nvgText(vg, lcx, 4.f, "FOLDS", NULL);
				nvgFillColor(vg, nvgRGBA(0xff, 0x80, 0x88, 200));
				nvgText(vg, (splitX + w) * 0.5f, 4.f, "LF PULSE", NULL);
			}
		}
	};

	static void drawPanel(const rack::widget::Widget::DrawArgs& args, float panelW) {
		NVGcontext* vg = args.vg;
		NVGcolor bg = nvgRGB(0x14, 0x13, 0x18);
		NVGcolor edge = nvgRGB(0x2a, 0x2e, 0x35);
		NVGcolor label = nvgRGB(0xb8, 0xc2, 0xcc);
		NVGcolor accent = nvgRGB(0xf0, 0x54, 0x62);
		NVGcolor cyanCol = nvgRGB(0x48, 0xd8, 0xf8);  // matches BREATH PRESS & N.MOD
		NVGcolor fbCol = nvgRGB(0xf0, 0xa4, 0x48);    // matches TRACT PROBE -> FB IN

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
		nvgStrokeColor(vg, nvgRGBA(0xf0, 0x54, 0x62, 120));
		nvgStroke(vg);

		float c1 = panelW * 0.25f, c2 = panelW * 0.75f;
		float r[7] = {98.f, 136.f, 171.f, 206.f, 246.f, 286.f, 330.f};

		NVGcolor pitchCol = nvgRGB(0xe8, 0x54, 0xb8); // neon magenta (matches VOICE PITCH)
		NVGcolor tensCol  = nvgRGB(0xf0, 0x64, 0x74); // neon coral-rose (matches TENSION)

		// Chamfered wireframe input bus bracket around the 4 CV inputs (r[4]..r[5])
		float bx0 = 16.f, bx1 = panelW - 16.f;
		float by0 = r[4] - 25.f, by1 = r[5] + 16.5f;
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
		nvgFillColor(vg, nvgRGBA(0x0c, 0x0d, 0x13, 110));
		nvgFill(vg);
		nvgStrokeWidth(vg, 0.9f);
		nvgStrokeColor(vg, nvgRGBA(0xa8, 0x58, 0x78, 95));
		nvgStroke(vg);

		// ---- Hollow Neon Port Rings & Smooth Curved Coupling Trace ----
		auto drawHollowPortRing = [&](float x, float y, NVGcolor col, bool isOutput) {
			if (isOutput) {
				nvgBeginPath(vg);
				nvgCircle(vg, x, y, 14.8f);
				nvgFillColor(vg, nvgRGB(0x0b, 0x0c, 0x10));
				nvgFill(vg);
				strokeNeon(vg, col, 1.1f, 3.0f, 45);
			} else {
				nvgBeginPath(vg);
				nvgCircle(vg, x, y, 13.6f);
				strokeNeon(vg, col, 0.95f, 2.5f, 32);
			}
		};

		// Row r[4] inputs: PITCH (magenta) & TENS (coral-rose)
		drawHollowPortRing(c1, r[4], pitchCol, false);
		drawHollowPortRing(c2, r[4], tensCol, false);
		// Row r[5] inputs: PRESS (cyan -> BREATH PRESS) & FB IN (amber -> TRACT PROBE)
		drawHollowPortRing(c1, r[5], cyanCol, false);
		drawHollowPortRing(c2, r[5], fbCol, false);

		// Smoothly curved coupling trace from FB IN (c2, r[5]) up to FB GAIN knob (c1, r[3])
		nvgBeginPath(vg);
		nvgMoveTo(vg, c2 - 14.5f, r[5]);
		nvgLineTo(vg, panelW * 0.5f + 7.f, r[5]);
		nvgQuadTo(vg, panelW * 0.5f, r[5], panelW * 0.5f, r[5] - 7.f);
		nvgLineTo(vg, panelW * 0.5f, r[3] + 7.f);
		nvgQuadTo(vg, panelW * 0.5f, r[3], panelW * 0.5f - 7.f, r[3]);
		nvgLineTo(vg, c1 + 16.f, r[3]);
		strokeNeon(vg, nvgRGBA(0xf0, 0xa4, 0x48, 160), 1.0f, 2.5f, 32);

		nvgBeginPath(vg);
		nvgMoveTo(vg, c1 + 14.5f, r[3]);
		nvgLineTo(vg, c1 + 18.5f, r[3] - 2.3f);
		nvgLineTo(vg, c1 + 18.5f, r[3] + 2.3f);
		nvgClosePath(vg);
		nvgFillColor(vg, nvgRGBA(0xf0, 0xa4, 0x48, 190));
		nvgFill(vg);

		// VOICED output (crimson-coral -> TRACT SRC in) & N.MOD output (cyan -> TRACT INJECT in)
		drawHollowPortRing(c1, r[6], accent, true);
		drawHollowPortRing(c2, r[6], cyanCol, true);

		nvgFontSize(args.vg, 8);
		auto font = getLabelFont();
		if (font && font->handle >= 0)
			nvgFontFaceId(args.vg, font->handle);
		nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
		nvgFillColor(args.vg, label);

		auto text = [&](float x, float y, const char* s) {
			nvgText(args.vg, x, y, s, NULL);
		};

		// params (color-coded primary glottal drive & feedback knobs)
		nvgFillColor(args.vg, nvgRGB(0xf2, 0x88, 0x94));
		text(c1, 81, "TENSION");
		nvgFillColor(args.vg, nvgRGB(0x7e, 0xd4, 0xe8));
		text(c2, 81, "PRESSURE");
		nvgFillColor(args.vg, label);
		text(c1, 122, "FOLD mm");
		nvgFillColor(args.vg, nvgRGB(0xe8, 0x8c, 0xd0));
		text(c2, 122, "OCT");
		nvgFillColor(args.vg, label);
		text(c1, 157, "DRIFT");    text(c2, 157, "VIB Hz");
		nvgFillColor(args.vg, nvgRGB(0xec, 0xb0, 0x6a));
		text(c1, 192, "FB GAIN");
		nvgFillColor(args.vg, label);
		text(c2, 192, "P-PITCH");

		// inputs (color-coded inside the CV input bus bracket)
		nvgFillColor(args.vg, nvgRGB(0xe8, 0x8c, 0xd0));
		text(c1, 230, "PITCH");
		nvgFillColor(args.vg, label);
		text(96, 230, "CV");
		nvgFillColor(args.vg, nvgRGB(0xf2, 0x88, 0x94));
		text(c2, 230, "TENS/POLY");
		nvgFillColor(args.vg, nvgRGB(0x7e, 0xd4, 0xe8));
		text(c1, 270, "PRESS");
		nvgFillColor(args.vg, nvgRGB(0xec, 0xb0, 0x6a));
		text(c2, 270, "FB IN");

		// outputs
		nvgFillColor(args.vg, nvgRGB(0xf2, 0x7c, 0x82));
		text(c1, 313, "VOICED");
		nvgFillColor(args.vg, nvgRGB(0x7e, 0xd4, 0xe8));
		text(c2, 313, "N.MOD");

		drawPanelTitle(args, panelW, "LARYNX");
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
		using rack::componentlibrary::Trimpot;
		using rack::componentlibrary::PJ301MPort;

		// Top laryngoscope + LF pulse telemetry display
		{
			auto gauge = new GlottisGauge(module);
			gauge->box.pos = rack::Vec(16.f, 15.f);
			gauge->box.size = rack::Vec(148.f, 54.f);
			addChild(gauge);
		}

		float c1 = PANEL_W * 0.25f, c2 = PANEL_W * 0.75f;
		float r[7] = {98.f, 136.f, 171.f, 206.f, 246.f, 286.f, 330.f};

		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(c1, r[0]), module, CyberlineLarynx::TENSION_PARAM));
		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(c2, r[0]), module, CyberlineLarynx::PRESSURE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[1]), module, CyberlineLarynx::FOLDLEN_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, r[1]), module, CyberlineLarynx::OCT_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[2]), module, CyberlineLarynx::DRIFT_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, r[2]), module, CyberlineLarynx::VIBRATE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[3]), module, CyberlineLarynx::FBGAIN_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, r[3]), module, CyberlineLarynx::PCOUPLE_PARAM));
		addParam(rack::createParamCentered<Trimpot>(rack::Vec(96.f, r[4]), module, CyberlineLarynx::TENSIONCV_PARAM));

		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c1, r[4]), module, CyberlineLarynx::PITCH_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c2, r[4]), module, CyberlineLarynx::TENSION_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c1, r[5]), module, CyberlineLarynx::PRESSURE_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c2, r[5]), module, CyberlineLarynx::FEEDBACK_INPUT));

		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c1, r[6]), module, CyberlineLarynx::VOICED_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c2, r[6]), module, CyberlineLarynx::NOISEMOD_OUTPUT));
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
