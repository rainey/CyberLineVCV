// CyberlineVisual1Widget — programmatic panel (no SVG assets). 16 HP.
// The image area is a NanoVG RGBA texture updated from the core's
// double-buffered frames; neon HUD bezel and control column follow the
// suite's HeartGauge/BreathGauge style.
//
// Texture lifecycle: the NVG image handle is (re)created on demand inside
// draw (Cardinal may destroy/recreate the GL context at any time — same
// discipline as getLabelFont()).
//
// PERTURB source: bundled res/ demo image (JPEG/PNG, decoded once via
// stb_image) or user-picked file through the host file dialog. In
// Cardinal the dialog is async (async_dialog_filebrowser); in stock Rack
// the blocking osdialog_file runs on a deferred action so the audio
// thread is never blocked by the dialog itself.

#include "CyberlineVisual1.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>

// stb_image for decoding the perturb source (bundled demo + user files).
// Static-scope implementation: this TU is the only stb consumer in the
// plugin (Cyberline* prefix suite — no clash risk, but static keeps the
// symbols out of the archive entirely).
#define STB_IMAGE_STATIC
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#ifdef USING_CARDINAL_NOT_RACK
#include <common.hpp> // async_dialog_filebrowser
#else
#include <osdialog.h>
#endif

namespace cyberline {

// Bundled demo image (decode failure falls back to the synthetic gradient).
// (Definition lives below, after decodeAndLoad.)
static void loadBundledImage(CyberlineVisual1* module);
// open host file dialog and decode the chosen image into the core
static void loadImageDialog(CyberlineVisual1* module);

struct CyberlineVisual1Widget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 16 * 15.f;
	// Image area geometry (screen px, in panel space)
	static constexpr float IMG_X = 16.f;
	static constexpr float IMG_Y = 16.f;
	static constexpr float IMG_W_PX = 206.f;
	static constexpr float IMG_H_PX = 155.f; // 256x192 aspect (4:3)

	CyberlineVisual1* m;
	int nvgImage = -1;
	int nvgImageGen = -1; // frameSeq at last nvgUpdateImage

	struct VisualGauge : rack::widget::Widget {
		CyberlineVisual1Widget* w;

		void draw(const DrawArgs& args) override {
			NVGcontext* vg = args.vg;
			float x = w->IMG_X, y = w->IMG_Y;
			float iw = w->IMG_W_PX, ih = w->IMG_H_PX;

			// Hollow chamfered bezel + grid, HeartGauge style
			float ch = 5.f;
			nvgBeginPath(vg);
			nvgMoveTo(vg, x + ch, y);
			nvgLineTo(vg, x + iw - ch, y);
			nvgLineTo(vg, x + iw, y + ch);
			nvgLineTo(vg, x + iw, y + ih - ch);
			nvgLineTo(vg, x + iw - ch, y + ih);
			nvgLineTo(vg, x + ch, y + ih);
			nvgLineTo(vg, x, y + ih - ch);
			nvgLineTo(vg, x, y + ch);
			nvgClosePath(vg);
			nvgFillColor(vg, nvgRGB(0x0b, 0x0d, 0x11));
			nvgFill(vg);

			// --- the texture: core frame -> RGBA image -> pattern fill ---
			CyberlineVisual1* module = w->m;
			if (module) {
				auto& core = module->core;
				int seq = core.frameSeq.load(std::memory_order_acquire);
				if (w->nvgImage < 0) {
					w->nvgImage = nvgCreateImageRGBA(vg,
						CyberlineVisual1Core::IMG_W, CyberlineVisual1Core::IMG_H,
						0, core.frameRGBA());
					w->nvgImageGen = seq - 1; // force one update
				}
				if (w->nvgImage >= 0 && seq != w->nvgImageGen) {
					nvgUpdateImage(vg, w->nvgImage, core.frameRGBA());
					w->nvgImageGen = seq;
				}
				if (w->nvgImage >= 0) {
					// inset so the bezel frames the image
					float inset = 2.5f;
					NVGpaint imgPaint = nvgImagePattern(vg,
						x + inset, y + inset,
						iw - 2 * inset, ih - 2 * inset,
						0.f, w->nvgImage, 1.f);
					nvgBeginPath(vg);
					nvgRect(vg, x + inset, y + inset, iw - 2 * inset, ih - 2 * inset);
					nvgFillPaint(vg, imgPaint);
					nvgFill(vg);
				}
			} else {
				// Module browser: draw a static demo mosaic from the core's
				// neutral frame
				static const uint8_t demo[16 * 3] = {
					64, 84, 104,  40, 60, 90,  90, 60, 110, 110, 60, 90,
					40, 90, 90,   64, 84, 104, 90, 60, 110,  110, 60, 90,
					90, 60, 110,  40, 90, 90,   64, 84, 104,  40, 60, 90,
					110, 60, 90,  90, 60, 110,  40, 90, 90,   64, 84, 104,
				};
				float inset = 2.5f;
				for (int gy = 0; gy < 12; gy++) {
					for (int gx = 0; gx < 16; gx++) {
						const uint8_t* c = demo + ((gy % 4) * 16 + (gx % 4)) * 3;
						nvgBeginPath(vg);
						nvgRect(vg,
							x + inset + gx * (iw - 2 * inset) / 16.f,
							y + inset + gy * (ih - 2 * inset) / 12.f,
							(iw - 2 * inset) / 16.f, (ih - 2 * inset) / 12.f);
						nvgFillColor(vg, nvgRGB(c[0], c[1], c[2]));
						nvgFill(vg);
					}
				}
			}

			// Neon bezel stroke over everything
			float w8 = iw, h8 = ih;
			(void) w8; (void) h8;
			auto strokeNeon = [](NVGcontext* vgc, NVGcolor col, float coreW, float glowW, int glowAlpha) {
				nvgStrokeWidth(vgc, glowW);
				nvgStrokeColor(vgc, nvgRGBA(col.r * 255, col.g * 255, col.b * 255, glowAlpha));
				nvgStroke(vgc);
				nvgStrokeWidth(vgc, coreW);
				nvgStrokeColor(vgc, col);
				nvgStroke(vgc);
			};
			nvgBeginPath(vg);
			nvgMoveTo(vg, x + ch, y);
			nvgLineTo(vg, x + iw - ch, y);
			nvgLineTo(vg, x + iw, y + ch);
			nvgLineTo(vg, x + iw, y + ih - ch);
			nvgLineTo(vg, x + iw - ch, y + ih);
			nvgLineTo(vg, x + ch, y + ih);
			nvgLineTo(vg, x, y + ih - ch);
			nvgLineTo(vg, x, y + ch);
			nvgClosePath(vg);
			strokeNeon(vg, nvgRGBA(0x48, 0xc8, 0xe8, 185), 1.0f, 2.8f, 35);

			// Corner HUD brackets (cyan TL/BL, coral TR/BR)
			nvgBeginPath(vg);
			nvgMoveTo(vg, x, y + ch + 8.f); nvgLineTo(vg, x, y + ch); nvgLineTo(vg, x + ch, y); nvgLineTo(vg, x + ch + 8.f, y);
			nvgMoveTo(vg, x, y + ih - ch - 8.f); nvgLineTo(vg, x, y + ih - ch); nvgLineTo(vg, x + ch, y + ih); nvgLineTo(vg, x + ch + 8.f, y + ih);
			nvgStrokeWidth(vg, 1.4f);
			nvgStrokeColor(vg, nvgRGB(0x40, 0xe0, 0xff));
			nvgStroke(vg);
			nvgBeginPath(vg);
			nvgMoveTo(vg, x + iw - ch - 8.f, y); nvgLineTo(vg, x + iw - ch, y); nvgLineTo(vg, x + iw, y + ch); nvgLineTo(vg, x + iw, y + ch + 8.f);
			nvgMoveTo(vg, x + iw - ch - 8.f, y + ih); nvgLineTo(vg, x + iw - ch, y + ih); nvgLineTo(vg, x + iw, y + ih - ch); nvgLineTo(vg, x + iw, y + ih - ch - 8.f);
			nvgStrokeWidth(vg, 1.4f);
			nvgStrokeColor(vg, nvgRGB(0xff, 0x58, 0x64));
			nvgStroke(vg);

			// Micro-labels
			auto font = getLabelFont();
			if (font && font->handle >= 0) {
				nvgFontFaceId(vg, font->handle);
				nvgFontSize(vg, 6.f);
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
				nvgFillColor(vg, nvgRGBA(0x58, 0x98, 0xb0, 170));
				nvgText(vg, x + 8.f, y + 7.f, "DCT-MOSAIC 256x192", NULL);
				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
				nvgText(vg, x + iw - 8.f, y + 7.f, "FFT->2D", NULL);
			}
		}
	};

	static void drawPanel(const rack::widget::Widget::DrawArgs& args, float panelW) {
		NVGcontext* vg = args.vg;
		NVGcolor bg = nvgRGB(0x12, 0x15, 0x1a);
		NVGcolor edge = nvgRGB(0x28, 0x30, 0x3a);
		NVGcolor label = nvgRGB(0xb8, 0xc2, 0xcc);
		NVGcolor accent = nvgRGB(0x48, 0xd8, 0xf8);

		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, panelW, rack::app::RACK_GRID_HEIGHT);
		nvgFillColor(vg, bg);
		nvgFill(vg);
		nvgStrokeWidth(vg, 1.f);
		nvgStrokeColor(vg, edge);
		nvgStroke(vg);

		// Top neon accent bar + sub-trace
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, panelW, 5);
		nvgFillColor(vg, accent);
		nvgFill(vg);
		nvgBeginPath(vg);
		nvgMoveTo(vg, 0, 6.5f);
		nvgLineTo(vg, panelW, 6.5f);
		nvgStrokeWidth(vg, 0.8f);
		nvgStrokeColor(vg, nvgRGBA(0x48, 0xd8, 0xf8, 120));
		nvgStroke(vg);

		float c1 = 40.f;      // knob column
		float c2 = 128.f;     // switch/extra column
		float r[4] = {214.f, 256.f, 298.f, 340.f};

		// Image area occupies y 16..171; row labels start below
		nvgFontSize(vg, 8);
		auto font = getLabelFont();
		if (font && font->handle >= 0)
			nvgFontFaceId(vg, font->handle);
		nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
		nvgFillColor(vg, label);
		auto text = [&](float x, float y, const char* s) {
			nvgText(vg, x, y, s, NULL);
		};

		text(c1, 200, "GAIN");
		text(c2, 200, "SPREAD");
		nvgFillColor(vg, nvgRGB(0x88, 0xc8, 0xf0));
		text(c1, 242, "DECAY");
		nvgFillColor(vg, nvgRGB(0x80, 0xe0, 0xb0));
		text(c2, 242, "MODE");
		nvgFillColor(vg, nvgRGB(0xd8, 0xa0, 0xf8));
		text(c2, 284, "STEREO");
		nvgFillColor(vg, label);
		text(c1, 284, "SRC");

		// bottom jack row: L/R inputs + FRAME/ACT outputs, evenly spaced
		nvgFillColor(vg, nvgRGB(0x7e, 0xd8, 0xf8));
		text(30.f, 322, "L");
		text(90.f, 322, "R");
		nvgFillColor(vg, nvgRGB(0x88, 0xe0, 0xb4));
		text(150.f, 322, "FRAME");
		nvgFillColor(vg, nvgRGB(0xf0, 0x98, 0x54));
		text(210.f, 322, "ACT");

		// 3-position tick labels under MODE / STEREO switches
		if (font && font->handle >= 0) {
			nvgFontSize(vg, 5.5f);
			nvgFillColor(vg, nvgRGB(0x7c, 0x8a, 0x98));
			static const char* modeNames[3] = {"MOS", "PHS", "PTB"};
			static const char* stNames[3] = {"SUM", "INT", "SPL"};
			for (int i = 0; i < 3; i++) {
				text(c2 - 12.f, 252 + i * 12, modeNames[i]);
				text(c2 + 26.f, 294 + i * 12, stNames[i]);
			}
			nvgFontSize(vg, 8.f);
		}

		drawPanelTitle(args, panelW, "VISUAL1");
	}

	CyberlineVisual1Widget(CyberlineVisual1* module) : m(module) {
		setModule(module);
		box.size = rack::Vec(PANEL_W, rack::app::RACK_GRID_HEIGHT);

		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, rack::app::RACK_GRID_HEIGHT - 15)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, rack::app::RACK_GRID_HEIGHT - 15)));

		using rack::componentlibrary::RoundSmallBlackKnob;
		using rack::componentlibrary::PJ301MPort;

		float c1 = 40.f, c2 = 128.f;
		float r[4] = {214.f, 256.f, 298.f, 340.f};

		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[0]), module, CyberlineVisual1::GAIN_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c2, r[0]), module, CyberlineVisual1::BAND_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(rack::Vec(c1, r[1]), module, CyberlineVisual1::DECAY_PARAM));

		// MODE / STEREO as snapping 3-step value dials (SmallWhiteKnob over
		// snapped quantity keeps the suite's all-programmatic look)
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(
			rack::Vec(c2, r[1]), module, CyberlineVisual1::MODE_PARAM));
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(
			rack::Vec(c2, r[2]), module, CyberlineVisual1::STEREO_PARAM));

		// (image source loading lives in the context menu)

		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(30.f, r[3]), module, CyberlineVisual1::L_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(90.f, r[3]), module, CyberlineVisual1::R_INPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(150.f, r[3]), module, CyberlineVisual1::FRAME_OUTPUT));
		addOutput(rack::createOutputCentered<PJ301MPort>(rack::Vec(210.f, r[3]), module, CyberlineVisual1::ACT_OUTPUT));

		auto gauge = new VisualGauge();
		gauge->w = this;
		gauge->box.pos = rack::Vec(0.f, 0.f);
		gauge->box.size = rack::Vec(PANEL_W, rack::app::RACK_GRID_HEIGHT);
		addChild(gauge);
	}

	void appendContextMenu(rack::ui::Menu* menu) override {
		CyberlineVisual1* module = dynamic_cast<CyberlineVisual1*>(this->module);
		if (!module)
			return;
		menu->addChild(new rack::ui::MenuSeparator);

		struct LoadImageItem : rack::ui::MenuItem {
			CyberlineVisual1* module;
			void onAction(const rack::event::Action& e) override {
				loadImageDialog(module);
			}
		};
		auto* loadItem = rack::createMenuItem<LoadImageItem>("Load image (perturb source)");
		loadItem->module = module;
		menu->addChild(loadItem);

		struct ReloadBundledItem : rack::ui::MenuItem {
			CyberlineVisual1* module;
			void onAction(const rack::event::Action& e) override {
				loadBundledImage(module);
			}
		};
		auto* bundledItem = rack::createMenuItem<ReloadBundledItem>("Reload bundled demo image");
		bundledItem->module = module;
		menu->addChild(bundledItem);
	}

	// ---- image loading ----

	// Decode + push into the core. Runs on any thread (decode is local,
	// core->loadSourcePixels writes its own arrays + one atomic store).
	static void decodeAndLoad(CyberlineVisual1* module, const uint8_t* data, int len, const char* hintName) {
		if (!module || !data || len <= 0)
			return;
		int w = 0, h = 0, ch = 0;
		uint8_t* pixels = stbi_load_from_memory(data, len, &w, &h, &ch, 3);
		if (!pixels) {
			WARN("CyberlineVisual1: image decode failed (%s)", hintName ? hintName : "?");
			return;
		}
		module->core.loadSourcePixels(pixels, w, h);
		stbi_image_free(pixels);
	}

	static void loadBundledImage(CyberlineVisual1* module) {
		if (!module || !pluginInstance || pluginInstance->path.empty())
			return;
		std::string path = rack::asset::plugin(pluginInstance, "res/visual1-demo.jpg");
		FILE* f = std::fopen(path.c_str(), "rb");
		if (!f) {
			// fallback: ship without the demo? synthesize a gradient source
			int w = 256, h = 192;
			std::vector<uint8_t> img(w * h * 3);
			for (int y = 0; y < h; y++)
				for (int x = 0; x < w; x++) {
					uint8_t* p = &img[(y * w + x) * 3];
					float u = x / (float) (w - 1), v = y / (float) (h - 1);
					p[0] = (uint8_t) (u * 255.f);
					p[1] = (uint8_t) (v * 255.f);
					p[2] = 128;
				}
			module->core.loadSourcePixels(img.data(), w, h);
			return;
		}
		std::fseek(f, 0, SEEK_END);
		long len = std::ftell(f);
		std::fseek(f, 0, SEEK_SET);
		std::vector<uint8_t> data(len);
		if (std::fread(data.data(), 1, len, f) == (size_t) len)
			decodeAndLoad(module, data.data(), (int) len, "bundled demo");
		std::fclose(f);
	}

	static void loadImageDialog(CyberlineVisual1* module) {
		if (!module)
			return;
#ifdef USING_CARDINAL_NOT_RACK
		// Cardinal: async dialog; resume in the callback (audio thread safe
		// via atomics — decode here, then one atomic publish)
		async_dialog_filebrowser(false, nullptr, nullptr, "Load image",
			[module](char* pathC) {
				if (!pathC) return;
				std::string path = pathC;
				free(pathC);
				// decode on this (UI) thread; core publish is atomic
				FILE* f = std::fopen(path.c_str(), "rb");
				if (!f) return;
				std::fseek(f, 0, SEEK_END);
				long len = std::ftell(f);
				std::fseek(f, 0, SEEK_SET);
				std::vector<uint8_t> data(len);
				if (std::fread(data.data(), 1, len, f) == (size_t) len)
					decodeAndLoad(module, data.data(), (int) len, path.c_str());
				std::fclose(f);
			});
#else
		// Stock Rack: grab the file dialog synchronously on a deferred
		// action (dialog blocks UI, not engine — Rack runs modules on
		// their own threads and this runs on the widget thread)
		std::string dir = rack::asset::user("plugins");
		char* pathC = osdialog_file(OSDIALOG_OPEN, dir.c_str(), NULL, NULL);
		if (!pathC) return;
		std::string path = pathC;
		std::free(pathC);
		FILE* f = std::fopen(path.c_str(), "rb");
		if (!f) return;
		std::fseek(f, 0, SEEK_END);
		long len = std::ftell(f);
		std::fseek(f, 0, SEEK_SET);
		std::vector<uint8_t> data(len);
		if (std::fread(data.data(), 1, len, f) == (size_t) len)
			decodeAndLoad(module, data.data(), (int) len, path.c_str());
		std::fclose(f);
#endif
	}
};

} // namespace cyberline

namespace cyberline {
rack::plugin::Model* modelCyberlineVisual1 =
	rack::createModel<CyberlineVisual1, CyberlineVisual1Widget>("cyberline-visual1");
} // namespace cyberline
