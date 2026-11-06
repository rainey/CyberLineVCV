// cyberline — model declarations shared by registration and widgets.

#pragma once
#include "CyberlineQuality.hpp"
#include <rack.hpp>
#include <cstdio>
#include <cstdint>

// The per-plugin instance pointer (defined in plugin.cpp at global scope;
// JayBird's build renames the token to pluginInstance__cyberline via
// -DpluginInstance on all cyberline TUs). Declared at global scope, before
// namespace cyberline, so the mangled name matches plugin.cpp's definition
// and getLabelFont() below can use it.
extern rack::Plugin* pluginInstance;

namespace cyberline {

// Shared label font for programmatic panels. Re-resolved every draw (the
// host Window's own fontCache makes that cheap) so a destroyed-and-recreated
// GL context re-loads instead of holding a dead nanovg font handle.
//
// Resolution order:
//   1. Plugin-bundled copy (res/fonts/DejaVuSans.ttf, vendored with its
//      permissive Bitstream/DejaVu license). Resolves in Cardinal/JayBird
//      via share/cardinal/<slug>/ and in stock Rack via the plugin install
//      dir — the only path that works everywhere without host help.
//   2. Rack/Cardinal system font. Under stock Rack this is a real file;
//      under Cardinal the Window constructor pre-seeds its fontCache with
//      an embedded fallback under exactly this key, so no disk file is
//      needed even when res/ is not deployed.
inline std::shared_ptr<rack::window::Font> getLabelFont() {
	auto ctx = rack::contextGet();
	if (!ctx)
		return nullptr;
	if (pluginInstance && !pluginInstance->path.empty()) {
		auto font = ctx->window->loadFont(
			rack::asset::plugin(pluginInstance, "res/fonts/DejaVuSans.ttf"));
		if (font && font->handle >= 0)
			return font;
	}
	return ctx->window->loadFont(rack::asset::system("res/fonts/DejaVuSans.ttf"));
}

// Panel title: brand + module name, bottom-center (all four panels have
// dead space below the output row; the corner screws don't reach the
// centered text). Called at the end of each drawPanel.
inline void drawPanelTitle(const rack::widget::Widget::DrawArgs& args, float panelW, const char* name) {
	auto font = getLabelFont();
	if (!font || font->handle < 0)
		return;
	nvgFontFaceId(args.vg, font->handle);
	nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
	nvgFillColor(args.vg, nvgRGB(0x6a, 0x74, 0x80));
	nvgFontSize(args.vg, 7);
	nvgText(args.vg, panelW * 0.5f, 362, "CYBERLINE", NULL);
	nvgFillColor(args.vg, nvgRGB(0xd8, 0xde, 0xe4));
	nvgFontSize(args.vg, 11);
	nvgTextLetterSpacing(args.vg, 1.0);
	nvgText(args.vg, panelW * 0.5f, 374, name, NULL);
	nvgTextLetterSpacing(args.vg, 0.0);
}

extern rack::plugin::Model* modelCyberlineLarynx;
extern rack::plugin::Model* modelCyberlineLungs;
extern rack::plugin::Model* modelCyberlineOralChamber;
extern rack::plugin::Model* modelCyberlineVoice;
extern rack::plugin::Model* modelCyberlineHeart;
extern rack::plugin::Model* modelCyberlineBroca;
extern rack::plugin::Model* modelCyberlineVisual1;
extern rack::plugin::Model* modelCyberlineSagitta;

// DSP-quality context menu for the vocal-tract modules. The selection
// lives in a hidden param (0 = HIGH full-fidelity DSP, 1 = LOW fast
// approximations) so it saves with the patch and participates in undo.
//
// LOW gates only the perceptually-transparent cost savers: the tract's
// block-rate reflection refresh (16-sample fade instead of a fresh
// target + 1-sample crossfade every sample) and the glottis' incremental
// exponentials / fast aspiration-gate sine. The always-on fixes
// (denormal protection, VTL slew + state resampling, transient decay
// precomputation) apply in both modes and do not change the sound.
// DSP-quality context menu shared by every cyberline module.
//
// Two layers:
//   - Global (all modules): persists in the plugin settings file, applies
//     to every module in FOLLOW mode (the default). Undo is not recorded
//     for the global — it is a plugin setting, not patch data.
//   - Per-module override (paramId >= 0): hidden QUALITY_PARAM,
//     0 = HIGH pinned, 1 = LOW pinned, 2 = FOLLOW GLOBAL (default).
//     Saved with the patch and undoable.
//
// Modules without quality-dependent DSP pass module = nullptr / paramId
// = -1 and expose only the global section.
inline void appendQualityMenu(rack::ui::Menu* menu, rack::engine::Module* module, int paramId) {
	menu->addChild(new rack::ui::MenuSeparator);
	menu->addChild(rack::createMenuLabel("DSP quality (all modules)"));

	struct GlobalItem : rack::ui::MenuItem {
		bool low;
		void onAction(const rack::event::Action& e) override {
			CyberlineQuality::setGlobal(low);
		}
	};
	auto* ghi = rack::createMenuItem<GlobalItem>("High (full fidelity)",
		CHECKMARK(!CyberlineQuality::getGlobal()));
	ghi->low = false;
	menu->addChild(ghi);
	auto* glo = rack::createMenuItem<GlobalItem>("Low (fast approximations)",
		CHECKMARK(CyberlineQuality::getGlobal()));
	glo->low = true;
	menu->addChild(glo);

	if (!module || paramId < 0)
		return;

	menu->addChild(new rack::ui::MenuSeparator);
	menu->addChild(rack::createMenuLabel("This module (override)"));

	struct QualityItem : rack::ui::MenuItem {
		rack::engine::Module* module;
		int paramId;
		float value;   // 0 = HIGH pinned, 1 = LOW pinned, 2 = follow global
		void onAction(const rack::event::Action& e) override {
			float old = module->params[paramId].getValue();
			if (old == value)
				return;
			module->params[paramId].setValue(value);
			auto* h = new rack::history::ParamChange;
			h->name = "set quality override";
			h->moduleId = module->id;
			h->paramId = paramId;
			h->oldValue = old;
			h->newValue = value;
			APP->history->push(h);
		}
	};
	float cur = module->params[paramId].getValue();
	int mode = (int) std::floor(cur + 0.5f);
	const char* followLabel = CyberlineQuality::getGlobal()
		? "Follow global (currently Low)"
		: "Follow global (currently High)";
	auto* fo = rack::createMenuItem<QualityItem>(followLabel,
		CHECKMARK(mode == 2));
	fo->module = module;
	fo->paramId = paramId;
	fo->value = 2.f;
	menu->addChild(fo);
	auto* hi = rack::createMenuItem<QualityItem>("High for this module",
		CHECKMARK(mode == 0));
	hi->module = module;
	hi->paramId = paramId;
	hi->value = 0.f;
	menu->addChild(hi);
	auto* lo = rack::createMenuItem<QualityItem>("Low for this module",
		CHECKMARK(mode == 1));
	lo->module = module;
	lo->paramId = paramId;
	lo->value = 1.f;
	menu->addChild(lo);
}
// Plugin-side CPU metering + percent overlay.
//
// Rack's stock per-module strip (cpuMeter setting) draws its percent text
// with blendish; observed on this suite: the strip's graph line + plate
// render on cyberline modules but the blendish text does not (stock
// Fundamental modules show it). Rather than depend on that, each module
// measures its own process() duration on a 37-frame cadence (matching
// Rack's METER_DIVIDER; two system::getTime() calls then, ~40 ns every
// 37 samples, ~0.02% overhead) into a public ring buffer, and the widget
// overlays the same-style percent text with our own font — reliable on
// every host. No-op cost when performance meters are off (guard on
// settings::cpuMeter).
struct CyberlineCpuMeter {
	static constexpr int LEN = 32;
	static constexpr int DIVIDER = 37;   // same cadence Rack uses
	float ring[LEN] = {};                // avg seconds per process(), ring
	int idx = 0;
	long long counter = 0;
	bool measuring = false;              // this sample is a measurement
	double t0 = 0.0;

	// Call at the TOP of process():
	inline bool frameStart() {
		if (!rack::settings::cpuMeter)
			return false;
		bool measure = (counter++ % DIVIDER) == 0;
		if (measure)
			t0 = rack::system::getTime();
		return measure;
	}
	// Call at the END of process() with frameStart()'s return value:
	inline void frameEnd(bool measure, float sampleRate) {
		if (!measure)
			return;
		float duration = (float) (rack::system::getTime() - t0);
		// running average over the last DIVIDER samples before ring push,
		// so the displayed number is smooth (Rack averages the same way)
		avg = (avg < 0.f) ? duration : avg * 0.85f + duration * 0.15f;
		idx = (idx + 1) % LEN;
		ring[idx] = avg;
		(void) sampleRate;
	}
	float avg = -1.f;

	// Percent of one audio core at the current sample rate (what the
	// stock meter's number means).
	float percent(float sampleRate) const {
		if (avg < 0.f)
			return 0.f;
		return avg * sampleRate * 100.f;
	}
};

// Widget overlay: the stock strip's number, drawn with our font. Call at
// the end of the widget's draw(), after ModuleWidget::draw(args), passing
// the CONCRETE module type (ModuleWidget::module is the base pointer, so
// the cpuMeter member needs the downcast). No-op when performance meters
// are off / no module.
template <typename ModuleT>
inline void drawCpuMeterText(const rack::widget::Widget::DrawArgs& args,
		ModuleT* module) {
	if (!module || !rack::settings::cpuMeter)
		return;
	float sampleRate = rack::contextGet()->engine->getSampleRate();
	const CyberlineCpuMeter* meter = &module->cpuMeter;
	char text[32];
	std::snprintf(text, sizeof(text), "%.2f%%", meter->percent(sampleRate));

	auto* vg = args.vg;
	auto font = getLabelFont();
	if (!font)
		return;
	nvgFontFaceId(vg, font->handle);
	nvgFontSize(vg, 11.f);
	nvgFillColor(vg, nvgRGB(0xff, 0xa5, 0x42));   // matches Rack's orange strip
	nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BASELINE);
	nvgText(vg, args.clipBox.size.x - 4.f, args.clipBox.size.y - 3.f, text, NULL);
}

} // namespace cyberline
