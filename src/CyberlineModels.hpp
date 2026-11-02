// cyberline — model declarations shared by registration and widgets.

#pragma once
#include <rack.hpp>

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
} // namespace cyberline
