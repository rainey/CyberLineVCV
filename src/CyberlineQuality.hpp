// CyberlineQuality — plugin-wide DSP quality setting with per-module
// override.
//
// One global flag applies to every cyberline module that has quality-
// dependent DSP paths (Larynx, Oral Chamber, Voice). A hidden module param
// can pin an individual module to High/Low or let it follow the global
// (default). The global persists across sessions in a tiny settings file
// inside the plugin's user directory (the suite's only file I/O — one
// line, written on change); see docs/spec.md §2.
//
// Default is LOW (fast approximations): the savings are perceptually
// transparent and keep multi-module vocal patches cheap. HIGH remains
// available globally or per module.
//
// Modes (hidden QUALITY_PARAM, saved with the patch):
//   0 = HIGH  (pinned full fidelity)
//   1 = LOW   (pinned fast approximations)
//   2 = FOLLOW GLOBAL (default)

#pragma once
#include <rack.hpp>
#include <cstdio>
#include <cstring>
#include <string>

namespace cyberline {

struct CyberlineQuality {
	// Function-local statics: header-only, safe across translation units.
	static bool& globalLowRef() {
		static bool low = true;   // default LOW
		return low;
	}
	static std::string& settingsPathRef() {
		static std::string path;
		return path;
	}

	// Called once from plugin init(). Without it (test harnesses) the
	// global stays at its LOW default and save() is a no-op.
	static void init(rack::plugin::Plugin* p) {
		settingsPathRef() = rack::asset::plugin(p, "quality.json");
		globalLowRef() = true;   // default until the file says otherwise
		FILE* f = std::fopen(settingsPathRef().c_str(), "r");
		if (f) {
			char buf[32] = {0};
			// one line: "low=0" or "low=1"; anything else -> default LOW
			if (std::fgets(buf, sizeof(buf), f)) {
				const char* eq = std::strchr(buf, '=');
				if (eq && (eq[1] == '0' || eq[1] == '1'))
					globalLowRef() = (eq[1] == '1');
			}
			std::fclose(f);
		}
	}

	static bool getGlobal() {
		return globalLowRef();
	}

	static void setGlobal(bool low) {
		globalLowRef() = low;
		save();
	}

	static void save() {
		const std::string& path = settingsPathRef();
		if (path.empty())
			return;
		FILE* f = std::fopen(path.c_str(), "w");
		if (!f)
			return;
		std::fprintf(f, "low=%d\n", globalLowRef() ? 1 : 0);
		std::fclose(f);
	}

	// Effective LOW flag for a module from its QUALITY_PARAM value:
	// pinned 0/1, or the global when the param is at FOLLOW (2).
	static bool effectiveLow(float paramValue) {
		int mode = (int) std::floor(paramValue + 0.5f);
		if (mode <= 0)
			return false;   // pinned HIGH
		if (mode == 1)
			return true;    // pinned LOW
		return globalLowRef();
	}
};

} // namespace cyberline
