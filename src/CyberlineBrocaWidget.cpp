// CyberlineBrocaWidget — programmatic 14 HP hollow neon vector panel for "BROCA"
// (Broca-Brika articulatory cortex & phoneme generator).

#include "CyberlineBroca.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>
#include <cstdio>

namespace cyberline {

struct CyberlineBrocaWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 14 * 15.f; // 210 px (14 HP)

	static void strokeNeon(NVGcontext* vg, NVGcolor col, float coreW = 1.2f, float glowW = 3.4f, int glowAlpha = 50) {
		nvgStrokeWidth(vg, glowW);
		nvgStrokeColor(vg, nvgRGBA(col.r * 255, col.g * 255, col.b * 255, glowAlpha));
		nvgStroke(vg);
		nvgStrokeWidth(vg, coreW);
		nvgStrokeColor(vg, col);
		nvgStroke(vg);
	}

	struct SnapBlackKnob : rack::componentlibrary::RoundBlackKnob {
		SnapBlackKnob() {
			snap = true;
		}
	};

	// Top Holographic HUD: IPA Articulatory Vowel Trapezoid + Consonant Trajectory
	// + Live Syllable Telemetry ("BROCA-BRIKA").
	struct BrocaCortexGauge : rack::widget::Widget {
		CyberlineBroca* module;
		BrocaCortexGauge(CyberlineBroca* m) : module(m) {}

		// Map anatomical (tongueX 0..10, tongueY 0..10) into the IPA Vowel Trapezoid
		// screen space: Front (high tongueX) on the LEFT, Back (low tongueX) on the RIGHT,
		// Close (low tongueY) at the TOP, Open (high tongueY) at the BOTTOM.
		static void mapTrapezoid(float tx, float ty, float x0, float y0, float w, float h, float* ox, float* oy) {
			float uFront = rack::math::clamp(tx / 10.f, 0.f, 1.f); // 1 = front (left), 0 = back (right)
			float uOpen  = rack::math::clamp(ty / 10.f, 0.f, 1.f); // 0 = close (top),  1 = open (bottom)
			// IPA trapezoid narrows at the bottom-left (open-front slope)
			float leftEdge  = x0 + uOpen * (w * 0.28f);
			float rightEdge = x0 + w;
			*ox = rightEdge - uFront * (rightEdge - leftEdge);
			*oy = y0 + uOpen * h;
		}

		void draw(const DrawArgs& args) override {
			float w = box.size.x, h = box.size.y;
			NVGcontext* vg = args.vg;

			NVGcolor neonViolet = nvgRGB(0x9b, 0x6d, 0xff);
			NVGcolor neonCyan   = nvgRGB(0x48, 0xd8, 0xf8);
			NVGcolor neonCoral  = nvgRGB(0xf0, 0x5c, 0x64);
			NVGcolor neonAmber  = nvgRGB(0xf4, 0xb0, 0x48);
			NVGcolor neonMint   = nvgRGB(0x58, 0xe0, 0xa8);

			// 1. Chamfered HUD frame
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
			nvgFillColor(vg, nvgRGB(0x0b, 0x0d, 0x12));
			nvgFill(vg);
			strokeNeon(vg, nvgRGBA(0x8e, 0x62, 0xf8, 195), 1.0f, 2.8f, 35);

			// Corner brackets
			nvgBeginPath(vg);
			nvgMoveTo(vg, 0.f, ch + 7.f); nvgLineTo(vg, 0.f, ch); nvgLineTo(vg, ch, 0.f); nvgLineTo(vg, ch + 7.f, 0.f);
			nvgMoveTo(vg, w - ch - 7.f, 0.f); nvgLineTo(vg, w - ch, 0.f); nvgLineTo(vg, w, ch); nvgLineTo(vg, w, ch + 7.f);
			nvgMoveTo(vg, 0.f, h - ch - 7.f); nvgLineTo(vg, 0.f, h - ch); nvgLineTo(vg, ch, h); nvgLineTo(vg, ch + 7.f, h);
			nvgMoveTo(vg, w - ch - 7.f, h); nvgLineTo(vg, w - ch, h); nvgLineTo(vg, w, h - ch); nvgLineTo(vg, w, h - ch - 7.f);
			nvgStrokeWidth(vg, 1.3f);
			nvgStrokeColor(vg, neonCyan);
			nvgStroke(vg);

			// Vertical divider between Vowel Trapezoid (left) and Syllable Telemetry (right)
			float splitX = 104.f;
			nvgBeginPath(vg);
			nvgMoveTo(vg, splitX, 5.f);
			nvgLineTo(vg, splitX, h - 5.f);
			nvgStrokeWidth(vg, 0.7f);
			nvgStrokeColor(vg, nvgRGBA(0x68, 0x58, 0xa8, 90));
			nvgStroke(vg);

			// Read live state
			int vA = module ? module->activeVowelA : 0;
			int vB = module ? module->activeVowelB : 4;
			int cIdx = module ? module->activeConsonant : 11;
			float vMorph = module ? module->activeVMorph : 0.f;
			float cvMix  = module ? module->activeCVMix : 0.f;
			float curTx  = module ? module->curTongueX : 8.4f;
			float curTy  = module ? module->curTongueY : 2.2f;
			float curVel = module ? module->curVelum : 0.01f;
			float curFric = module ? module->curFric : 0.f;
			float curVoic = module ? module->curVoicing : 1.0f;

			// ---- 2. Left Pane: Hollow Wireframe IPA Vowel Trapezoid ----
			float tx0 = 12.f, ty0 = 14.f, tw = 82.f, th = h - 22.f;
			float tlX, tlY, trX, trY, brX, brY, blX, blY;
			mapTrapezoid(9.2f, 1.5f, tx0, ty0, tw, th, &tlX, &tlY);
			mapTrapezoid(1.2f, 1.5f, tx0, ty0, tw, th, &trX, &trY);
			mapTrapezoid(1.2f, 9.2f, tx0, ty0, tw, th, &brX, &brY);
			mapTrapezoid(9.2f, 9.2f, tx0, ty0, tw, th, &blX, &blY);

			// Outer trapezoid + mid crossbars
			nvgBeginPath(vg);
			nvgMoveTo(vg, tlX, tlY);
			nvgLineTo(vg, trX, trY);
			nvgLineTo(vg, brX, brY);
			nvgLineTo(vg, blX, blY);
			nvgClosePath(vg);
			// Central vertical line & mid horizontal line
			nvgMoveTo(vg, (tlX + trX) * 0.5f, tlY);
			nvgLineTo(vg, (blX + brX) * 0.5f, blY);
			nvgMoveTo(vg, (tlX + blX) * 0.5f, (tlY + blY) * 0.5f);
			nvgLineTo(vg, (trX + brX) * 0.5f, (trY + brY) * 0.5f);
			nvgStrokeWidth(vg, 0.8f);
			nvgStrokeColor(vg, nvgRGBA(0x7a, 0x62, 0xc8, 110));
			nvgStroke(vg);

			// 12 Vowel constellation dots
			for (int i = 0; i < CyberlineBroca::NUM_VOWELS; i++) {
				float vx, vy;
				mapTrapezoid(CyberlineBroca::VOWELS[i].tongueX,
					CyberlineBroca::VOWELS[i].tongueY, tx0, ty0, tw, th, &vx, &vy);
				nvgBeginPath(vg);
				nvgCircle(vg, vx, vy, (i == vA || i == vB) ? 2.2f : 1.2f);
				nvgFillColor(vg, (i == vA) ? neonCyan : ((i == vB) ? neonViolet : nvgRGBA(0x88, 0x94, 0xb0, 130)));
				nvgFill(vg);
			}

			// Vowel A -> Vowel B morph trajectory line
			float ax, ay, bx, by;
			mapTrapezoid(CyberlineBroca::VOWELS[vA].tongueX,
				CyberlineBroca::VOWELS[vA].tongueY, tx0, ty0, tw, th, &ax, &ay);
			mapTrapezoid(CyberlineBroca::VOWELS[vB].tongueX,
				CyberlineBroca::VOWELS[vB].tongueY, tx0, ty0, tw, th, &bx, &by);
			nvgBeginPath(vg);
			nvgMoveTo(vg, ax, ay);
			nvgLineTo(vg, bx, by);
			strokeNeon(vg, nvgRGBA(0x48, 0xd8, 0xf8, 170), 0.9f, 2.4f, 35);

			// Live tongue reticle (diamond)
			float lx, ly;
			mapTrapezoid(curTx, curTy, tx0, ty0, tw, th, &lx, &ly);
			if (cIdx > 0 && cvMix > 0.04f) {
				// Consonant onset vector (coral-amber)
				float cx, cy;
				mapTrapezoid(CyberlineBroca::CONSONANTS[cIdx].tongueX,
					CyberlineBroca::CONSONANTS[cIdx].tongueY, tx0, ty0, tw, th, &cx, &cy);
				nvgBeginPath(vg);
				nvgMoveTo(vg, cx, cy);
				nvgLineTo(vg, lx, ly);
				strokeNeon(vg, neonCoral, 1.0f, 2.6f, 45);
			}
			nvgBeginPath(vg);
			nvgMoveTo(vg, lx, ly - 3.6f);
			nvgLineTo(vg, lx + 3.6f, ly);
			nvgLineTo(vg, lx, ly + 3.6f);
			nvgLineTo(vg, lx - 3.6f, ly);
			nvgClosePath(vg);
			strokeNeon(vg, cvMix > 0.25f ? neonAmber : neonCyan, 1.1f, 3.0f, 55);

			// ---- 3. Right Pane: Syllable Readout & Telemetry Bars ----
			auto font = getLabelFont();
			if (font && font->handle >= 0) {
				nvgFontFaceId(vg, font->handle);

				nvgFontSize(vg, 5.5f);
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
				nvgFillColor(vg, nvgRGBA(0x9b, 0x6d, 0xff, 200));
				nvgText(vg, 7.f, 4.f, "IPA CORTEX", NULL);

				nvgFillColor(vg, nvgRGBA(0x48, 0xd8, 0xf8, 200));
				nvgText(vg, splitX + 6.f, 4.f, "BROCA-BRIKA", NULL);

				// Active Phoneme / Syllable String
				char sylBuf[32];
				const char* cCode = CyberlineBroca::CONSONANTS[cIdx].code;
				const char* vACode = CyberlineBroca::VOWELS[vA].code;
				const char* vBCode = CyberlineBroca::VOWELS[vB].code;
				if (cIdx > 0) {
					if (vMorph > 0.12f && vA != vB)
						std::snprintf(sylBuf, sizeof(sylBuf), "%s-%s>%s", cCode, vACode, vBCode);
					else
						std::snprintf(sylBuf, sizeof(sylBuf), "%s + %s", cCode, (vMorph > 0.5f ? vBCode : vACode));
				} else {
					if (vA != vB)
						std::snprintf(sylBuf, sizeof(sylBuf), "%s <> %s", vACode, vBCode);
					else
						std::snprintf(sylBuf, sizeof(sylBuf), "%s %s", vACode, CyberlineBroca::VOWELS[vA].ipa);
				}

				nvgFontSize(vg, 9.5f);
				nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
				nvgFillColor(vg, cvMix > 0.25f ? neonAmber : neonCyan);
				nvgText(vg, (splitX + w) * 0.5f, 22.f, sylBuf, NULL);

				// 3 Mini Horizontal Telemetry Bars: VOX (tension), VEL (nasal), FRC (fricative)
				struct BarInfo { const char* name; float val; NVGcolor col; };
				BarInfo bars[3] = {
					{"VOX", rack::math::clamp(curVoic, 0.f, 1.f), neonCoral},
					{"VEL", rack::math::clamp(curVel,  0.f, 1.f), neonMint},
					{"FRC", rack::math::clamp(curFric, 0.f, 1.f), neonAmber}
				};
				float bx = splitX + 22.f, bw = w - bx - 7.f;
				for (int i = 0; i < 3; i++) {
					float by = 34.f + i * 9.5f;
					nvgFontSize(vg, 5.2f);
					nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
					nvgFillColor(vg, nvgRGB(0x88, 0x94, 0xa4));
					nvgText(vg, splitX + 5.f, by, bars[i].name, NULL);

					nvgBeginPath(vg);
					nvgRect(vg, bx, by - 2.2f, bw, 4.4f);
					nvgFillColor(vg, nvgRGB(0x16, 0x1a, 0x22));
					nvgFill(vg);

					if (bars[i].val > 0.02f) {
						nvgBeginPath(vg);
						nvgRect(vg, bx, by - 2.2f, bw * bars[i].val, 4.4f);
						nvgFillColor(vg, bars[i].col);
						nvgFill(vg);
					}
				}
			}
		}
	};

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
		nvgFillColor(vg, neonViolet);
		nvgFill(vg);
		nvgBeginPath(vg);
		nvgMoveTo(vg, 0, 7.5f);
		nvgLineTo(vg, panelW, 7.5f);
		nvgStrokeWidth(vg, 0.8f);
		nvgStrokeColor(vg, nvgRGBA(0x9b, 0x6d, 0xff, 130));
		nvgStroke(vg);

		float c1 = 38.f, c2 = 105.f, c3 = 172.f;
		float cvX[6] = {22.f, 55.2f, 88.4f, 121.6f, 154.8f, 188.f};
		float outX[4] = {32.f, 80.7f, 129.3f, 178.f};
		float rIn = 268.f, rOut = 328.f;

		// Subtle chamfered wireframe divider between Vowel Row (Row 1) and Consonant Row (Row 2)
		nvgBeginPath(vg);
		nvgMoveTo(vg, 18.f, 150.f);
		nvgLineTo(vg, 23.f, 147.f);
		nvgLineTo(vg, panelW - 23.f, 147.f);
		nvgLineTo(vg, panelW - 18.f, 150.f);
		nvgStrokeWidth(vg, 0.8f);
		nvgStrokeColor(vg, nvgRGBA(0x8e, 0x62, 0xf8, 75));
		nvgStroke(vg);

		// Chamfered wireframe input bus bracket around the 5 CV/Trigger inputs (rIn)
		float bx0 = 8.f, bx1 = panelW - 8.f;
		float by0 = rIn - 26.f, by1 = rIn + 16.5f;
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

		auto drawPortRing = [&](float x, float y, NVGcolor col, bool isOutput, bool isPoly) {
			nvgBeginPath(vg);
			nvgCircle(vg, x, y, isOutput ? 14.8f : 13.4f);
			if (isOutput) {
				nvgFillColor(vg, nvgRGB(0x09, 0x0b, 0x10));
				nvgFill(vg);
				strokeNeon(vg, col, isPoly ? 1.35f : 1.05f, isPoly ? 3.6f : 2.8f, isPoly ? 60 : 40);
				if (isPoly) {
					// Outer double-ring halo indicating polyphonic bus output
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

		// Input rings: SYNC, TRIG, VOWEL, V.MRPH, CONS, C/V
		drawPortRing(cvX[0], rIn, neonMint,   false, false); // SYNC
		drawPortRing(cvX[1], rIn, neonMint,   false, false); // TRIG
		drawPortRing(cvX[2], rIn, neonCyan,   false, false); // VOWEL A CV
		drawPortRing(cvX[3], rIn, neonViolet, false, false); // V.MORPH CV
		drawPortRing(cvX[4], rIn, neonAmber,  false, false); // CONS CV
		drawPortRing(cvX[5], rIn, neonCoral,  false, false); // C/V CV

		// Output rings: TRACT POLY (violet), LARYNX POLY (coral), C/V ENV (cyan), BURST (amber)
		drawPortRing(outX[0], rOut, neonViolet, true, true);
		drawPortRing(outX[1], rOut, neonCoral,  true, true);
		drawPortRing(outX[2], rOut, neonCyan,   true, false);
		drawPortRing(outX[3], rOut, neonAmber,  true, false);

		auto font = getLabelFont();
		if (font && font->handle >= 0)
			nvgFontFaceId(vg, font->handle);
		nvgFontSize(vg, 8.f);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);

		auto text = [&](float x, float y, const char* s) {
			nvgText(vg, x, y, s, NULL);
		};

		// Row 1 labels: VOWEL A, V.MORPH, VOWEL B
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(c1, 96, "VOWEL A");
		nvgFillColor(vg, label);
		text(c2, 96, "V.MORPH");
		nvgFillColor(vg, nvgRGB(0xd0, 0xb0, 0xff));
		text(c3, 96, "VOWEL B");

		// Row 2 labels: CONSONANT, C/V MIX, ONSET s
		nvgFillColor(vg, nvgRGB(0xf8, 0xc0, 0x78));
		text(c1, 174, "CONSONANT");
		nvgFillColor(vg, nvgRGB(0xf8, 0x90, 0x98));
		text(c2, 174, "C/V MIX");
		nvgFillColor(vg, label);
		text(c3, 174, "ONSET s");

		// Row 3 input labels inside bracket
		nvgFontSize(vg, 6.6f);
		nvgFillColor(vg, nvgRGB(0x80, 0xe8, 0xb8));
		text(cvX[0], 251, "SYNC");
		text(cvX[1], 251, "TRIG");
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(cvX[2], 251, "VOWEL");
		nvgFillColor(vg, nvgRGB(0xd0, 0xb0, 0xff));
		text(cvX[3], 251, "V.MRPH");
		nvgFillColor(vg, nvgRGB(0xf8, 0xc0, 0x78));
		text(cvX[4], 251, "CONS");
		nvgFillColor(vg, nvgRGB(0xf8, 0x90, 0x98));
		text(cvX[5], 251, "C/V");

		// Row 4 output labels
		nvgFontSize(vg, 7.2f);
		nvgFillColor(vg, nvgRGB(0xd0, 0xb0, 0xff));
		text(outX[0], 308, "TRACT POLY");
		nvgFillColor(vg, nvgRGB(0xf8, 0x90, 0x98));
		text(outX[1], 308, "LARYNX POLY");
		nvgFillColor(vg, nvgRGB(0x88, 0xe4, 0xfc));
		text(outX[2], 308, "C/V ENV");
		nvgFillColor(vg, nvgRGB(0xf8, 0xc0, 0x78));
		text(outX[3], 308, "BURST");
		nvgFontSize(vg, 8.f);

		drawPanelTitle(args, panelW, "BROCA");
	}

	CyberlineBrocaWidget(CyberlineBroca* module) {
		setModule(module);
		box.size = rack::Vec(PANEL_W, rack::app::RACK_GRID_HEIGHT);

		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, rack::app::RACK_GRID_HEIGHT - 15)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, rack::app::RACK_GRID_HEIGHT - 15)));

		using rack::componentlibrary::RoundBlackKnob;
		using rack::componentlibrary::RoundSmallBlackKnob;
		using rack::componentlibrary::PJ301MPort;

		// Top holographic IPA vowel-trapezoid & syllable cortex HUD
		{
			auto gauge = new BrocaCortexGauge(module);
			gauge->box.pos = rack::Vec(15.f, 14.f);
			gauge->box.size = rack::Vec(180.f, 64.f);
			addChild(gauge);
		}

		float c1 = 38.f, c2 = 105.f, c3 = 172.f;
		float rKnob1 = 116.f, rKnob2 = 194.f;
		float cvX[6] = {22.f, 55.2f, 88.4f, 121.6f, 154.8f, 188.f};
		float outX[4] = {32.f, 80.7f, 129.3f, 178.f};
		float rIn = 268.f, rOut = 328.f;

		// Row 1: VOWEL A (snap), V.MORPH, VOWEL B (snap)
		addParam(rack::createParamCentered<SnapBlackKnob>(rack::Vec(c1, rKnob1), module, CyberlineBroca::VOWELA_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, rKnob1), module, CyberlineBroca::VMORPH_PARAM));
		addParam(rack::createParamCentered<SnapBlackKnob>(rack::Vec(c3, rKnob1), module, CyberlineBroca::VOWELB_PARAM));

		// Row 2: CONSONANT (snap), C/V MIX, ONSET s
		addParam(rack::createParamCentered<SnapBlackKnob>(rack::Vec(c1, rKnob2), module, CyberlineBroca::CONSONANT_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, rKnob2), module, CyberlineBroca::CVMORPH_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c3, rKnob2), module, CyberlineBroca::ONSET_PARAM));

		// Row 3: 6 CV/Trigger inputs (SYNC, TRIG, VOWEL, V.MRPH, CONS, C/V)
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cvX[0], rIn), module, CyberlineBroca::SYNC_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cvX[1], rIn), module, CyberlineBroca::TRIG_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cvX[2], rIn), module, CyberlineBroca::VOWELA_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cvX[3], rIn), module, CyberlineBroca::VMORPH_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cvX[4], rIn), module, CyberlineBroca::CONSONANT_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(cvX[5], rIn), module, CyberlineBroca::CVMORPH_INPUT));

		// Row 4: 4 outputs (TRACT POLY, LARYNX POLY, C/V ENV, BURST)
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[0], rOut), module, CyberlineBroca::TRACT_POLY_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[1], rOut), module, CyberlineBroca::LARYNX_POLY_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[2], rOut), module, CyberlineBroca::CVENV_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(outX[3], rOut), module, CyberlineBroca::BURST_OUTPUT));
	}

	void draw(const DrawArgs& args) override {
		drawPanel(args, PANEL_W);
		ModuleWidget::draw(args);
	}
};

} // namespace cyberline

namespace cyberline {
rack::plugin::Model* modelCyberlineBroca =
	rack::createModel<CyberlineBroca, CyberlineBrocaWidget>("cyberline-broca");
} // namespace cyberline
