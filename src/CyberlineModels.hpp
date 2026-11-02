// cyberline — model declarations shared by registration and widgets.

#pragma once
#include <rack.hpp>

namespace cyberline {

// Shared label font for programmatic panels. `inline` + function-local
// static = exactly one instance program-wide (the earlier per-.cpp copies
// were ODR violations). Loaded lazily from the Rack system dir; drawn via
// nvgFontFaceId like Rack's own LedDisplay widgets.
//
// Assumes a single Rack context for the process lifetime (true for the
// standalone app and Cardinal). The cached nanovg font handle belongs to
// that context's Window, so a host that destroyed and recreated the context
// while keeping the plugin loaded would render dead handles (invisible
// labels) — re-key the cache on the context pointer if that ever matters.
inline std::shared_ptr<rack::window::Font> getLabelFont() {
	static std::shared_ptr<rack::window::Font> font;
	if (!font && rack::contextGet()) {
		font = rack::contextGet()->window->loadFont(
			rack::system::join(rack::asset::systemDir, "res/fonts/DejaVuSans.ttf"));
	}
	return font;
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
} // namespace cyberline
