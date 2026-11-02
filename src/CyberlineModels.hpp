// cyberline — model declarations shared by registration and widgets.

#pragma once
#include <rack.hpp>

namespace cyberline {

// Shared label font for programmatic panels (inline: one instance per TU
// set via the linker). Loaded lazily from the Rack system dir; drawn via
// nvgFontFaceId like Rack's own LedDisplay widgets.
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
// later: modelCyberlineOralChamber, modelCyberlineVoice
} // namespace cyberline
