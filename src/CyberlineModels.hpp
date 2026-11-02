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

extern rack::plugin::Model* modelCyberlineLarynx;
extern rack::plugin::Model* modelCyberlineLungs;
extern rack::plugin::Model* modelCyberlineOralChamber;
extern rack::plugin::Model* modelCyberlineVoice;
} // namespace cyberline
