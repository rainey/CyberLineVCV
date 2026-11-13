// CyberlineCantorWidget — programmatic panel (no SVG assets).
// 12 HP. Grid: label 16 px above its widget; rows spread over 380 px.

#include "CyberlineCantor.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>

namespace cyberline {

struct CyberlineCantorWidget : rack::app::ModuleWidget {
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

	// Cantor Top HUD:
	// - Left pane: endoscopic vocal folds whose length dynamically tracks
	//   module->telemetryEffFoldMm (2..45 mm) and aperture tracks curTenseness & intensity,
	//   plus live note name & effective fold length readout.
	// - Right pane: live LF glottal pulse (with blended harmonic carrier visual when
	//   CARRMIX_PARAM > 0) + cyan aspiration envelope trace.
	struct CantorGauge : rack::widget::Widget {
		CyberlineCantor* module;
		CantorGauge(CyberlineCantor* m) : module(m) {}

		void draw(const DrawArgs& args) override {
			NVGcontext* vg = args.vg;
			float tens = 0.6f, amp = 0.8f, foldMm = 12.82f, te = 0.52f, f0 = 261.6256f;
			float carrMix = 0.f;
			if (module && std::isfinite(module->glottis.curTenseness)) {
				tens = rack::math::clamp(module->glottis.curTenseness, 0.f, 1.f);
				amp = rack::math::clamp(module->glottis.intensity, 0.f, 1.f);
				foldMm = rack::math::clamp(module->telemetryEffFoldMm, 2.f, 45.f);
				f0 = module->telemetryF0;
				if (module->glottis.Te > 0.05f && module->glottis.Te < 0.98f)
					te = module->glottis.Te;
				carrMix = rack::math::clamp(module->params[CyberlineCantor::CARRMIX_PARAM].getValue(), 0.f, 1.f);
			}
			float w = box.size.x, h = box.size.y;

			NVGcolor neonGold  = nvgRGB(0xf4, 0xbe, 0x3a);
			NVGcolor neonCyan  = nvgRGB(0x48, 0xd8, 0xf8);
			NVGcolor neonRose  = nvgRGB(0xe8, 0x68, 0x90);
			NVGcolor neonCoral = nvgRGB(0xff, 0x54, 0x68);

			// Smoothly rounded neon HUD bezel
			nvgBeginPath(vg);
			nvgRoundedRect(vg, 0.f, 0.f, w, h, 7.f);
			nvgFillColor(vg, nvgRGB(0x0c, 0x0d, 0x12));
			nvgFill(vg);
			strokeNeon(vg, nvgRGBA(0xf4, 0xbe, 0x3a, 185), 1.0f, 2.8f, 35);

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
			nvgStrokeColor(vg, nvgRGBA(0x5a, 0x48, 0x38, 55));
			nvgStroke(vg);

			// Vertical divider between endoscopic folds (left) and wave (right)
			float splitX = w * 0.46f;
			nvgBeginPath(vg);
			nvgMoveTo(vg, splitX, 6.f);
			nvgLineTo(vg, splitX, h - 6.f);
			nvgStrokeWidth(vg, 0.8f);
			nvgStrokeColor(vg, nvgRGBA(0x78, 0x58, 0x42, 110));
			nvgStroke(vg);

			// ---- Left Pane: Curved Hollow Wireframe Vocal Folds ----
			float lcx = splitX * 0.5f;
			float lcy = h * 0.60f;
			float lenNorm = rack::math::clamp((foldMm - 2.f) / 30.f, 0.f, 1.f);
			float halfLen = 9.f + 5.5f * lenNorm;
			float chinkW = (1.3f + 4.8f * (1.f - tens)) * (0.4f + 0.6f * amp);

			// Outer hollow laryngeal ring
			nvgBeginPath(vg);
			nvgEllipse(vg, lcx, lcy, 16.5f, halfLen + 3.5f);
			strokeNeon(vg, nvgRGBA(0xb8, 0x78, 0x48, 155), 0.9f, 2.5f, 30);

			// Epiglottis crescent at anterior (top) commissure
			nvgBeginPath(vg);
			nvgMoveTo(vg, lcx - 10.5f, lcy - halfLen - 1.f);
			nvgBezierTo(vg, lcx - 5.5f, lcy - halfLen - 5.5f,
				lcx + 5.5f, lcy - halfLen - 5.5f, lcx + 10.5f, lcy - halfLen - 1.f);
			nvgLineCap(vg, NVG_ROUND);
			strokeNeon(vg, neonGold, 1.3f, 3.2f, 45);

			// Curved horizontal mucosal wireframe ribs
			nvgBeginPath(vg);
			for (int r = -2; r <= 2; r++) {
				float ry = lcy + (float) r * (halfLen * 0.38f);
				float ribW = 12.5f * std::sqrt(1.f - ((float) r * 0.33f) * ((float) r * 0.33f));
				nvgMoveTo(vg, lcx - ribW, ry);
				nvgQuadTo(vg, lcx - chinkW * 0.5f - 1.f, ry - 1.2f, lcx - chinkW * 0.5f, ry);
				nvgMoveTo(vg, lcx + chinkW * 0.5f, ry);
				nvgQuadTo(vg, lcx + chinkW * 0.5f + 1.f, ry - 1.2f, lcx + ribW, ry);
			}
			nvgStrokeWidth(vg, 0.6f);
			nvgStrokeColor(vg, nvgRGBA(0xc8, 0x90, 0x58, 85));
			nvgStroke(vg);

			// Left and right vocal fold medial margins (rima glottidis)
			nvgBeginPath(vg);
			nvgMoveTo(vg, lcx - 0.5f, lcy - halfLen);
			nvgBezierTo(vg, lcx - chinkW - 1.2f, lcy - halfLen * 0.3f,
				lcx - chinkW - 1.2f, lcy + halfLen * 0.3f, lcx - 0.5f, lcy + halfLen);
			strokeNeon(vg, neonRose, 1.05f, 2.8f, 40);

			nvgBeginPath(vg);
			nvgMoveTo(vg, lcx + 0.5f, lcy - halfLen);
			nvgBezierTo(vg, lcx + chinkW + 1.2f, lcy - halfLen * 0.3f,
				lcx + chinkW + 1.2f, lcy + halfLen * 0.3f, lcx + 0.5f, lcy + halfLen);
			strokeNeon(vg, neonRose, 1.05f, 2.8f, 40);

			// Glottal slit aperture glow
			if (amp > 0.05f) {
				nvgBeginPath(vg);
				nvgEllipse(vg, lcx, lcy, chinkW * 0.65f, halfLen * 0.72f);
				int glowAlpha = (int) (amp * 165.f);
				NVGcolor glowCol = nvgRGBA(0xf4, 0xbe, 0x3a, glowAlpha);
				nvgFillColor(vg, glowCol);
				nvgFill(vg);
			}

			// Left pane header: "FOLDS" + note name & fold length readout (e.g. "C4 · 12.8mm")
			char noteBuf[32];
			if (f0 > 10.f) {
				float semitones = 12.f * std::log2(f0 / 440.f) + 69.f;
				int midi = (int) std::round(semitones);
				static const char* const noteNames[] = {"C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B"};
				int noteIdx = (midi % 12 + 12) % 12;
				int oct = (midi / 12) - 1;
				std::snprintf(noteBuf, sizeof(noteBuf), "%s%d \xC2\xB7 %.1fmm", noteNames[noteIdx], oct, foldMm);
			} else {
				std::snprintf(noteBuf, sizeof(noteBuf), "--- \xC2\xB7 %.1fmm", foldMm);
			}

			auto font = getLabelFont();
			if (font && font->handle >= 0) {
				nvgFontFaceId(vg, font->handle);
				nvgFontSize(vg, 5.5f);
				nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
				nvgFillColor(vg, nvgRGBA(0xf4, 0xbe, 0x3a, 195));
				nvgText(vg, lcx, 4.f, "FOLDS", NULL);
				nvgFontSize(vg, 6.0f);
				nvgFillColor(vg, nvgRGBA(0xfa, 0xd0, 0x70, 220));
				nvgText(vg, lcx, 11.5f, noteBuf, NULL);
			}

			// ---- Right Pane: Live LF Glottal Wave / Carrier Blend & Aspiration ----
			float wx0 = splitX + 7.f;
			float wx1 = w - 7.f;
			float ww  = wx1 - wx0;
			float wyMid = h * 0.52f;

			// Zero-axis baseline
			nvgBeginPath(vg);
			nvgMoveTo(vg, wx0, wyMid);
			nvgLineTo(vg, wx1, wyMid);
			nvgStrokeWidth(vg, 0.65f);
			nvgStrokeColor(vg, nvgRGBA(0x68, 0x58, 0x74, 100));
			nvgStroke(vg);

			// Plot one cycle: LF pulse blended with harmonic carrier when CARRMIX > 0
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

				if (carrMix > 0.01f) {
					float carrierVis = std::sin(p * 8.f * (float) M_PI) * 0.45f
						+ std::sin(p * 4.f * (float) M_PI) * 0.35f;
					yVal = (1.f - carrMix * 0.75f) * yVal + (carrMix * 0.75f) * carrierVis;
				}

				float px = wx0 + p * ww;
				float py = wyMid - yVal * ampScale * 13.5f;
				py = rack::math::clamp(py, 9.f, h - 7.f);
				if (i == 0) nvgMoveTo(vg, px, py);
				else nvgLineTo(vg, px, py);
			}
			nvgLineJoin(vg, NVG_ROUND);
			NVGcolor waveColor = (carrMix > 0.15f)
				? nvgRGB(0xf4, 0xbe, 0x3a) // neon gold when carrier is blended
				: neonCoral;
			strokeNeon(vg, waveColor, 1.3f, 3.6f, 55);

			// Aspiration noise-gate envelope trace (neon cyan at bottom of right pane)
			nvgBeginPath(vg);
			float nyBase = h - 7.f;
			float nDepth = (1.f - 0.65f * std::sqrt(tens)) * ampScale * 7.f;
			nvgMoveTo(vg, wx0, nyBase - nDepth * 0.3f);
			nvgQuadTo(vg, wx0 + te * ww * 0.5f, nyBase - nDepth * 1.3f,
				wx0 + te * ww, nyBase - nDepth * 0.3f);
			nvgLineTo(vg, wx1, nyBase - nDepth * 0.2f);
			strokeNeon(vg, neonCyan, 0.95f, 2.5f, 35);

			// Right pane header: "CANTOR WAVE"
			if (font && font->handle >= 0) {
				nvgFontFaceId(vg, font->handle);
				nvgFontSize(vg, 5.5f);
				nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
				nvgFillColor(vg, (carrMix > 0.15f) ? nvgRGBA(0xf4, 0xbe, 0x3a, 200) : nvgRGBA(0xff, 0x80, 0x88, 200));
				nvgText(vg, (splitX + w) * 0.5f, 4.f, "CANTOR WAVE", NULL);
			}
		}
	};

	static void drawPanel(const rack::widget::Widget::DrawArgs& args, float panelW) {
		NVGcontext* vg = args.vg;
		NVGcolor bg = nvgRGB(0x14, 0x13, 0x18);
		NVGcolor edge = nvgRGB(0x2a, 0x2e, 0x35);
		NVGcolor label = nvgRGB(0xb8, 0xc2, 0xcc);
		NVGcolor goldCol = nvgRGB(0xf4, 0xbe, 0x3a);
		NVGcolor cyanCol = nvgRGB(0x48, 0xd8, 0xf8);
		NVGcolor magCol  = nvgRGB(0xe8, 0x54, 0xb8);
		NVGcolor coralCol = nvgRGB(0xf2, 0x7c, 0x82);

		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, panelW, rack::app::RACK_GRID_HEIGHT);
		nvgFillColor(vg, bg);
		nvgFill(vg);
		nvgStrokeWidth(vg, 1.f);
		nvgStrokeColor(vg, edge);
		nvgStroke(vg);

		// Top neon accent bar: gold -> cyan -> magenta singer gradient
		NVGpaint grad1 = nvgLinearGradient(vg, 0.f, 0.f, panelW * 0.5f, 0.f, goldCol, cyanCol);
		nvgBeginPath(vg);
		nvgRect(vg, 0.f, 0.f, panelW * 0.5f, 5.f);
		nvgFillPaint(vg, grad1);
		nvgFill(vg);

		NVGpaint grad2 = nvgLinearGradient(vg, panelW * 0.5f, 0.f, panelW, 0.f, cyanCol, magCol);
		nvgBeginPath(vg);
		nvgRect(vg, panelW * 0.5f, 0.f, panelW * 0.5f, 5.f);
		nvgFillPaint(vg, grad2);
		nvgFill(vg);

		// Sub-trace
		nvgBeginPath(vg);
		nvgMoveTo(vg, 0.f, 7.5f);
		nvgLineTo(vg, panelW, 7.5f);
		nvgStrokeWidth(vg, 0.8f);
		nvgStrokeColor(vg, nvgRGBA(0xf4, 0xbe, 0x3a, 120));
		nvgStroke(vg);

		float c1 = panelW * 0.25f, cMid = panelW * 0.5f, c2 = panelW * 0.75f;
		float r[7] = {98.f, 136.f, 171.f, 206.f, 246.f, 286.f, 330.f};

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
		nvgStrokeColor(vg, nvgRGBA(0xb8, 0x88, 0x58, 95));
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

		// Row r[4] inputs: PITCH (magenta) & TENS/POLY (coral-rose)
		drawHollowPortRing(c1, r[4], magCol, false);
		drawHollowPortRing(c2, r[4], nvgRGB(0xf0, 0x64, 0x74), false);
		// Row r[5] inputs: PRESS (cyan) & CARRIER (gold)
		drawHollowPortRing(c1, r[5], cyanCol, false);
		drawHollowPortRing(c2, r[5], goldCol, false);

		// Smoothly curved coupling trace from CARRIER input (c2, r[5]) up to CARR MIX knob (c1, r[3])
		nvgBeginPath(vg);
		nvgMoveTo(vg, c2 - 14.5f, r[5]);
		nvgLineTo(vg, panelW * 0.5f + 7.f, r[5]);
		nvgQuadTo(vg, panelW * 0.5f, r[5], panelW * 0.5f, r[5] - 7.f);
		nvgLineTo(vg, panelW * 0.5f, r[3] + 7.f);
		nvgQuadTo(vg, panelW * 0.5f, r[3], panelW * 0.5f - 7.f, r[3]);
		nvgLineTo(vg, c1 + 16.f, r[3]);
		strokeNeon(vg, nvgRGBA(0xf4, 0xbe, 0x3a, 160), 1.0f, 2.5f, 32);

		// Arrowhead pointing into CARR MIX knob
		nvgBeginPath(vg);
		nvgMoveTo(vg, c1 + 14.5f, r[3]);
		nvgLineTo(vg, c1 + 18.5f, r[3] - 2.3f);
		nvgLineTo(vg, c1 + 18.5f, r[3] + 2.3f);
		nvgClosePath(vg);
		nvgFillColor(vg, nvgRGBA(0xf4, 0xbe, 0x3a, 190));
		nvgFill(vg);

		// Outputs: VOICED (coral) & N.MOD (cyan)
		drawHollowPortRing(c1, r[6], coralCol, true);
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

		// Row r[0] labels (y = 81)
		nvgFillColor(args.vg, nvgRGB(0xf2, 0x88, 0x94));
		text(c1, 81, "TENSION");
		text(cMid, 81, "CV");
		nvgFillColor(args.vg, nvgRGB(0x7e, 0xd4, 0xe8));
		text(c2, 81, "PRESSURE");

		// Row r[1] labels (y = 120)
		nvgFillColor(args.vg, nvgRGB(0xfa, 0xd0, 0x70));
		text(34.f, 120, "TUNE");
		text(90.f, 120, "MASS");
		nvgFillColor(args.vg, magCol);
		text(146.f, 120, "OCT");

		// Row r[2] labels (y = 155)
		nvgFillColor(args.vg, label);
		text(c1, 155, "DRIFT");
		text(c2, 155, "VIB Hz");

		// Row r[3] labels (y = 190)
		nvgFillColor(args.vg, goldCol);
		text(c1, 190, "CARR MIX");
		nvgFillColor(args.vg, label);
		text(c2, 190, "P-BEND");

		// Inputs labels inside wireframe bracket (y = 230, 270)
		nvgFillColor(args.vg, magCol);
		text(c1, 230, "PITCH");
		nvgFillColor(args.vg, nvgRGB(0xf2, 0x88, 0x94));
		text(c2, 230, "TENS/POLY");
		nvgFillColor(args.vg, cyanCol);
		text(c1, 270, "PRESS");
		nvgFillColor(args.vg, goldCol);
		text(c2, 270, "CARRIER");

		// Outputs labels (y = 313)
		nvgFillColor(args.vg, coralCol);
		text(c1, 313, "VOICED");
		nvgFillColor(args.vg, cyanCol);
		text(c2, 313, "N.MOD");

		drawPanelTitle(args, panelW, "CANTOR");
	}

	CyberlineCantorWidget(CyberlineCantor* module) {
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

		// Top Cantor HUD
		{
			auto gauge = new CantorGauge(module);
			gauge->box.pos = rack::Vec(16.f, 15.f);
			gauge->box.size = rack::Vec(148.f, 54.f);
			addChild(gauge);
		}

		float c1 = PANEL_W * 0.25f, cMid = PANEL_W * 0.5f, c2 = PANEL_W * 0.75f;
		float r[7] = {98.f, 136.f, 171.f, 206.f, 246.f, 286.f, 330.f};

		// Row r[0] = 98.f: TENSION, TENSIONCV, PRESSURE
		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(c1, r[0]), module, CyberlineCantor::TENSION_PARAM));
		addParam(rack::createParamCentered<Trimpot>(rack::Vec(cMid, r[0]), module, CyberlineCantor::TENSIONCV_PARAM));
		addParam(rack::createParamCentered<RoundBlackKnob>(rack::Vec(c2, r[0]), module, CyberlineCantor::PRESSURE_PARAM));

		// Row r[1] = 136.f: TUNE (34.f), MASS (90.f), OCT (146.f)
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(34.f, r[1]), module, CyberlineCantor::TUNE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(90.f, r[1]), module, CyberlineCantor::MASS_PARAM));
		addParam(rack::createParamCentered<OctEncoderKnob>(rack::Vec(146.f, r[1]), module, CyberlineCantor::OCT_PARAM));

		// Row r[2] = 171.f: DRIFT, VIBRATE
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[2]), module, CyberlineCantor::DRIFT_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, r[2]), module, CyberlineCantor::VIBRATE_PARAM));

		// Row r[3] = 206.f: CARRMIX, PCOUPLE
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[3]), module, CyberlineCantor::CARRMIX_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, r[3]), module, CyberlineCantor::PCOUPLE_PARAM));

		// Inputs (r[4] = 246.f, r[5] = 286.f)
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c1, r[4]), module, CyberlineCantor::PITCH_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c2, r[4]), module, CyberlineCantor::TENSION_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c1, r[5]), module, CyberlineCantor::PRESSURE_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(c2, r[5]), module, CyberlineCantor::CARRIER_INPUT));

		// Outputs (r[6] = 330.f)
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c1, r[6]), module, CyberlineCantor::VOICED_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(c2, r[6]), module, CyberlineCantor::NOISEMOD_OUTPUT));
	}

	void appendContextMenu(rack::ui::Menu* menu) override {
		ModuleWidget::appendContextMenu(menu);
		auto* mod = dynamic_cast<CyberlineCantor*>(module);

		menu->addChild(new rack::ui::MenuSeparator);
		menu->addChild(rack::createSubmenuItem("Reference tuning (0 V)", "",
			[=](rack::ui::Menu* sub) {
				struct RefTuningItem : rack::ui::MenuItem {
					CyberlineCantor* module;
					int ref;
					void onAction(const rack::event::Action& e) override {
						if (module)
							module->refTuning = ref;
					}
				};
				auto addRef = [&](const char* label, int ref) {
					auto* item = rack::createMenuItem<RefTuningItem>(label,
						CHECKMARK(mod && mod->refTuning == ref));
					item->module = mod;
					item->ref = ref;
					sub->addChild(item);
				};
				addRef("C4 — 261.63 Hz (VCV standard)", CyberlineCantor::REF_C4);
				addRef("C3 — 130.81 Hz (Vocal center)", CyberlineCantor::REF_C3);
				addRef("A3 — 220.00 Hz", CyberlineCantor::REF_A3);
				addRef("C2 — 65.41 Hz (Bass / drone)", CyberlineCantor::REF_C2);
			}));

		menu->addChild(rack::createBoolMenuItem("Hard-sync glottis to CARRIER input", "",
			[=]() { return mod ? mod->carrierHardSync : false; },
			[=](bool val) { if (mod) mod->carrierHardSync = val; }));

		menu->addChild(new rack::ui::MenuSeparator);
		appendQualityMenu(menu, module, CyberlineCantor::QUALITY_PARAM);
	}

	void draw(const DrawArgs& args) override {
		drawPanel(args, PANEL_W);
		ModuleWidget::draw(args);
		drawCpuMeterText(args, static_cast<CyberlineCantor*>(module));
	}
};

} // namespace cyberline

namespace cyberline {
rack::plugin::Model* modelCyberlineCantor =
	rack::createModel<CyberlineCantor, CyberlineCantorWidget>("cyberline-cantor");
} // namespace cyberline
