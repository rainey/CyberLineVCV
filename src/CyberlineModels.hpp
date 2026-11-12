// cyberline — model declarations shared by registration and widgets.

#pragma once
#include "CyberlineQuality.hpp"
#include <rack.hpp>
#include <cstdio>
#include <string>
#include <cstdint>
#include <cmath>
#include <functional>

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

// ---- Octave encoder knob (shared by Larynx & Voice) ----
// The engine rounds the OCT param to whole octaves (std::round in both
// process() paths), but stock knob widgets let you drag to fractional
// values. The GUI now matches the engine: a snap ParamQuantity (drag
// steps in integer octaves, context-menu shows integers) drawn as a
// rotary encoder — 5 detents (-2..+2) around a dark cap, the active
// detent lit, a neon pointer notch, and the whole-octave readout in
// the cap.

struct OctaveParamQuantity : rack::engine::ParamQuantity {
	OctaveParamQuantity() { snapEnabled = true; }
};

// Generic snap ParamQuantity for integer-valued params (encoder knobs).
struct SnapParamQuantity : rack::engine::ParamQuantity {
	SnapParamQuantity() { snapEnabled = true; }
};

struct OctEncoderKnob : rack::Knob {
	// -2..+2 octaves = 5 detents
	static constexpr int NUM_DETENTS = 5;
	static constexpr float PI = 3.14159265f;

	OctEncoderKnob() {
		box.size = rack::Vec(26.f, 26.f);
	}

	void draw(const DrawArgs& args) override {
		float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
		float rOuter = box.size.x * 0.5f - 1.f;
		float minAng = -PI * 0.75f, maxAng = PI * 0.75f;
		auto detentAng = [&](int i) {
			return minAng + (float) i / (float) (NUM_DETENTS - 1)
				* (maxAng - minAng);
		};

		// Bezel: dark body + rim
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, rOuter);
		nvgFillColor(args.vg, nvgRGB(0x0d, 0x0f, 0x12));
		nvgFill(args.vg);
		nvgStrokeWidth(args.vg, 1.f);
		nvgStrokeColor(args.vg, nvgRGB(0x2a, 0x2e, 0x35));
		nvgStroke(args.vg);

		// Neon pointer notch at the active detent (glow pass + core pass)
		float v = getParamQuantity() ? getParamQuantity()->getValue() : 0.f;
		int active = (int) std::lround(rack::math::clamp(v, -2.f, 2.f)) + 2;
		float a = detentAng(active);
		float ux = std::sin(a), uy = -std::cos(a);
		float x0 = cx + ux * rOuter * 0.34f, y0 = cy + uy * rOuter * 0.34f;
		float x1 = cx + ux * rOuter * 0.55f, y1 = cy + uy * rOuter * 0.55f;
		for (int pass = 0; pass < 2; pass++) {
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, x0, y0);
			nvgLineTo(args.vg, x1, y1);
			if (pass == 0) {
				nvgStrokeWidth(args.vg, 3.0f);
				nvgStrokeColor(args.vg, nvgRGBA(0x48, 0xd8, 0xf8, 50));
			} else {
				nvgStrokeWidth(args.vg, 1.2f);
				nvgStrokeColor(args.vg, nvgRGB(0x48, 0xd8, 0xf8));
			}
			nvgStroke(args.vg);
		}

		// Detent dots; the active one is lit (drawn after the notch so it
		// reads as a lit detent, not a notch tip)
		float rDetent = rOuter - 3.f;
		for (int i = 0; i < NUM_DETENTS; i++) {
			float da = detentAng(i);
			float dx = cx + std::sin(da) * rDetent;
			float dy = cy - std::cos(da) * rDetent;
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, dx, dy, 1.3f);
			nvgFillColor(args.vg, i == active
				? nvgRGB(0x48, 0xd8, 0xf8)
				: nvgRGB(0x44, 0x4c, 0x56));
			nvgFill(args.vg);
		}

		// Whole-octave readout in the cap
		auto font = getLabelFont();
		if (font && font->handle >= 0) {
			char buf[8];
			if (active == 2)
				std::snprintf(buf, sizeof(buf), "0");
			else
				std::snprintf(buf, sizeof(buf), "%+d", active - 2);
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, 8.f);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, nvgRGB(0xa8, 0xb4, 0xc0));
			nvgText(args.vg, cx, cy + 0.5f, buf, NULL);
		}
	}
};

// 16-detent rotary encoder for register lengths (1..16 steps), same
// visual language as OctEncoderKnob: dark cap, detent ring with the
// active detent lit, neon pointer notch, integer readout in the cap.
// Use with SnapParamQuantity so drags land on whole steps.
struct LenEncoderKnob : rack::Knob {
	static constexpr int NUM_DETENTS = 16;   // 1..16 steps
	static constexpr float PI = 3.14159265f;

	LenEncoderKnob() {
		box.size = rack::Vec(26.f, 26.f);
	}

	void draw(const DrawArgs& args) override {
		float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
		float rOuter = box.size.x * 0.5f - 1.f;
		float minAng = -PI * 0.75f, maxAng = PI * 0.75f;
		auto detentAng = [&](int i) {
			return minAng + (float) i / (float) (NUM_DETENTS - 1)
				* (maxAng - minAng);
		};

		// Bezel: dark body + rim
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, rOuter);
		nvgFillColor(args.vg, nvgRGB(0x0d, 0x0f, 0x12));
		nvgFill(args.vg);
		nvgStrokeWidth(args.vg, 1.f);
		nvgStrokeColor(args.vg, nvgRGB(0x2a, 0x2e, 0x35));
		nvgStroke(args.vg);

		// Neon pointer notch at the active detent (glow pass + core pass)
		float v = getParamQuantity() ? getParamQuantity()->getValue() : 8.f;
		int active = (int) std::lround(rack::math::clamp(v, 1.f, 16.f)) - 1;
		float a = detentAng(active);
		float ux = std::sin(a), uy = -std::cos(a);
		float x0 = cx + ux * rOuter * 0.34f, y0 = cy + uy * rOuter * 0.34f;
		float x1 = cx + ux * rOuter * 0.55f, y1 = cy + uy * rOuter * 0.55f;
		for (int pass = 0; pass < 2; pass++) {
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, x0, y0);
			nvgLineTo(args.vg, x1, y1);
			if (pass == 0) {
				nvgStrokeWidth(args.vg, 3.0f);
				nvgStrokeColor(args.vg, nvgRGBA(0x48, 0xd8, 0xf8, 50));
			} else {
				nvgStrokeWidth(args.vg, 1.2f);
				nvgStrokeColor(args.vg, nvgRGB(0x48, 0xd8, 0xf8));
			}
			nvgStroke(args.vg);
		}

		// Detent dots; the active one is lit
		float rDetent = rOuter - 3.f;
		for (int i = 0; i < NUM_DETENTS; i++) {
			float da = detentAng(i);
			float dx = cx + std::sin(da) * rDetent;
			float dy = cy - std::cos(da) * rDetent;
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, dx, dy, 1.1f);
			nvgFillColor(args.vg, i == active
				? nvgRGB(0x48, 0xd8, 0xf8)
				: nvgRGB(0x44, 0x4c, 0x56));
			nvgFill(args.vg);
		}

		// Step-count readout in the cap
		auto font = getLabelFont();
		if (font && font->handle >= 0) {
			char buf[8];
			std::snprintf(buf, sizeof(buf), "%d", active + 1);
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, 8.f);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, nvgRGB(0xa8, 0xb4, 0xc0));
			nvgText(args.vg, cx, cy + 0.5f, buf, NULL);
		}
	}
};

// Infinite (wrapping) detent encoder: vertical drag steps through the
// detents one at a time and wraps at both edges (last -> first and
// vice versa), so the control behaves like a rotary encoder rather
// than a clamped knob. Dots are drawn only for small detent counts
// (21 detents would be visually dense); the neon pointer notch always
// shows the active position. Pair with SnapParamQuantity. When
// autoArmFn is set, starting a drag arms the register editor (auto-arm
// is only wanted on the V/C encoders, not POS).
struct WrapEncoderKnob : rack::Knob {
	int numDetents;
	float minVal;
	static constexpr float STEP_PX = 14.f;   // drag px per detent
	std::function<void()> autoArmFn;

	WrapEncoderKnob(int detents, float minV = 0.f) {
		numDetents = detents;
		minVal = minV;
		box.size = rack::Vec(26.f, 26.f);
	}

	void onDragStart(const rack::event::DragStart& e) override {
		Knob::onDragStart(e);
		if (e.button != GLFW_MOUSE_BUTTON_LEFT)
			return;
		if (autoArmFn)
			autoArmFn();
		dragAccum = 0.f;
	}

	void onDragMove(const rack::event::DragMove& e) override {
		// Displace from the base Knob's linear drag; we step discretely
		dragAccum += e.mouseDelta.x - e.mouseDelta.y;
		auto* pq = getParamQuantity();
		if (!pq)
			return;
		while (dragAccum >= STEP_PX) {
			float v = std::round(pq->getValue());
			pq->setValue(v + 1.f > minVal + numDetents - 1 ? minVal : v + 1.f);
			dragAccum -= STEP_PX;
		}
		while (dragAccum <= -STEP_PX) {
			float v = std::round(pq->getValue());
			pq->setValue(v - 1.f < minVal ? minVal + numDetents - 1 : v - 1.f);
			dragAccum += STEP_PX;
		}
	}

	void draw(const DrawArgs& args) override {
		float cx = box.size.x * 0.5f, cy = box.size.y * 0.5f;
		float rOuter = box.size.x * 0.5f - 1.f;

		// Bezel: dark body + rim
		nvgBeginPath(args.vg);
		nvgCircle(args.vg, cx, cy, rOuter);
		nvgFillColor(args.vg, nvgRGB(0x0d, 0x0f, 0x12));
		nvgFill(args.vg);
		nvgStrokeWidth(args.vg, 1.f);
		nvgStrokeColor(args.vg, nvgRGB(0x2a, 0x2e, 0x35));
		nvgStroke(args.vg);

		// Neon pointer notch at 12 o'clock — an infinite encoder has no
		// absolute angle, the pointer just marks the shaft
		for (int pass = 0; pass < 2; pass++) {
			nvgBeginPath(args.vg);
			nvgMoveTo(args.vg, cx, cy - rOuter * 0.30f);
			nvgLineTo(args.vg, cx, cy - rOuter * 0.58f);
			if (pass == 0) {
				nvgStrokeWidth(args.vg, 3.0f);
				nvgStrokeColor(args.vg, nvgRGBA(0x48, 0xd8, 0xf8, 50));
			} else {
				nvgStrokeWidth(args.vg, 1.2f);
				nvgStrokeColor(args.vg, nvgRGB(0x48, 0xd8, 0xf8));
			}
			nvgStroke(args.vg);
		}

		// Detent dots around the ring, active one lit (small counts only)
		float v = getParamQuantity() ? getParamQuantity()->getValue() : minVal;
		int active = (int) std::lround(rack::math::clamp(v, minVal, minVal + numDetents - 1)) - (int) minVal;
		if (numDetents <= 16) {
			float minAng = -3.14159265f * 0.75f, maxAng = 3.14159265f * 0.75f;
			float rDetent = rOuter - 3.f;
			for (int i = 0; i < numDetents; i++) {
				float da = minAng + (float) i / (float) (numDetents - 1) * (maxAng - minAng);
				float dx = cx + std::sin(da) * rDetent;
				float dy = cy - std::cos(da) * rDetent;
				nvgBeginPath(args.vg);
				nvgCircle(args.vg, dx, dy, 1.1f);
				nvgFillColor(args.vg, i == active
					? nvgRGB(0x48, 0xd8, 0xf8)
					: nvgRGB(0x44, 0x4c, 0x56));
				nvgFill(args.vg);
			}
		} else {
			// Dense ring: plain tick ring, active shown by the readout
			nvgBeginPath(args.vg);
			nvgCircle(args.vg, cx, cy, rOuter - 3.f);
			nvgStrokeWidth(args.vg, 1.f);
			nvgStrokeColor(args.vg, nvgRGB(0x3a, 0x42, 0x4e));
			nvgStroke(args.vg);
		}

		// Value readout in the cap
		auto font = getLabelFont();
		if (font && font->handle >= 0) {
			char buf[8];
			std::snprintf(buf, sizeof(buf), "%d", active + (int) minVal);
			nvgFontFaceId(args.vg, font->handle);
			nvgFontSize(args.vg, 8.f);
			nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
			nvgFillColor(args.vg, nvgRGB(0xa8, 0xb4, 0xc0));
			nvgText(args.vg, cx, cy + 0.5f, buf, NULL);
		}
	}

private:
	float dragAccum = 0.f;
};

// ---- Range presets (Inhale-max & size bands) ----
// The Lungs INHALE knob and the anatomy knobs (FOLDLEN / VTL) offer
// right-click submenus that narrow the knob's range to a physical band.
// Narrowing only clamps the existing ParamQuantity min/max (never widening
// beyond the module's configured full range), and the current value is
// pulled inside. The chosen band index persists in the patch via
// dataToJson; on load, Rack restores params first and then calls
// dataFromJson, so re-applying the range there is safe and deterministic.

// Clamp-and-apply a [lo, hi] band to a ParamQuantity. Full band = the
// module's original configured range (no-op re-apply is harmless).
// NOTE: uses the raw engine Param for the current value, NOT
// ParamQuantity::getValue() — the latter routes through APP->engine and is
// unavailable in headless contexts (tests, no host). In the live app both
// paths agree; the raw read is safe because the quantity's param pointer is
// the same object the engine uses.
inline void applyParamRange(rack::engine::ParamQuantity* pq, float lo, float hi) {
	if (!pq)
		return;
	float fullLo = pq->getMinValue(), fullHi = pq->getMaxValue();
	lo = rack::math::clamp(lo, fullLo, fullHi);
	hi = rack::math::clamp(hi, lo, fullHi);
	pq->minValue = lo;
	pq->maxValue = hi;
	rack::engine::Param* p = pq->getParam();
	if (p) {
		float v = p->getValue();
		if (v < lo || v > hi)
			p->setValue(rack::math::clamp(v, lo, hi));
	}
}

// ---- Inhale-max presets for the Lungs INHALE knob ----
// The knob's seconds are read raw by the engine (CYCLE ratio, TRIG seconds,
// PHASOR split = raw/5 s), so narrowing the max narrows every reachable
// ratio/time span consistently — exactly what "I always want short inhales"
// calls for.
static constexpr float INHALE_MAX_CHOICES[] = {5.f, 2.f, 1.f, 0.5f};
static constexpr const char* INHALE_MAX_LABELS[] = {
	"5 s (Full)", "2 s", "1 s", "0.5 s"};

// Short range-label formatter ("2", "0.5", "17.5" — no trailing zeros).
inline std::string formatMm(float v) {
	char buf[12];
	std::snprintf(buf, sizeof(buf), "%g", v);
	return buf;
}

// ---- Size bands for the anatomy knobs (FOLDLEN mm / VTL cm) ----
// Bands follow the suite's allometric pitch law (F0 = 10^(4.01-1.43·log10 L)
// at tension 0.6): each band maps to a physical animal-size class, and the
// full span is the original configured range.
struct SizeBand {
	float foldLo, foldHi;   // mm (interpreted when band applies to FOLDLEN)
	float vtlLo, vtlHi;     // cm (interpreted when band applies to VTL)
	const char* name;       // short band name
	const char* cue;        // menu-label animal cue
	const char* examples;   // longer example list for the spec/docs
};

static constexpr SizeBand SIZE_BANDS[] = {
	// FOLD mm,        VTL cm,      name      cue                  examples
	{2.f,   45.f,      6.f,  60.f,  "Full",   "2-45 mm / 6-60 cm", "Full range (2-45 mm, 6-60 cm): shrew-to-elephant anatomy"},
	{2.f,   6.f,       6.f,  12.f,  "Small",  "mouse..kitten",     "Mouse (~2 mm), bat, rat (~3 mm), guinea pig, kitten, small dog (chihuahua). Note: rodent/bird tracts sit at or below the 6 cm floor — pitch them with OCT instead"},
	{5.f,   12.f,      10.f, 18.f,  "Medium", "cat..child",        "Cat (purr recipe), rabbit, small/medium dog, human child, marmoset; adult-female human at the top edge (11.5 mm moan recipe)"},
	{10.f,  20.f,      15.f, 28.f,  "Large",  "human..chimp",      "Adult humans (female 11.5 mm / male 17 mm recipes), chimpanzee, medium/large dog, young deer"},
	{16.f,  45.f,      25.f, 60.f,  "XL",     "big cat..horse",    "Deep baritone (~19-20 mm), sea lion, big dog (great dane), big cats (lion/tiger ~25-33 mm), horse (~30 mm), red-deer stag roar; elephant (without trunk) at the VTL ceiling — the recipe's nasal-trunk radiation extends beyond"},
	{3.f,   19.f,      7.f,  22.f,  "Human",  "newborn..bass",     "Human newborn (3 mm — cry realism via OCT -2), child, adult female, deep bass"},
};
static constexpr int NUM_SIZE_BANDS = (int) (sizeof(SIZE_BANDS) / sizeof(SIZE_BANDS[0]));
// Indexes into SIZE_BANDS
static constexpr int BAND_FULL = 0, BAND_SMALL = 1, BAND_MEDIUM = 2,
	BAND_LARGE = 3, BAND_XL = 4, BAND_HUMAN = 5;

// ---- Pink Trombone poly-bus mode (shared by Broca & BrocaSq) ----
// PT (Vega Deftwing's PinkTromboneVCV) accepts mono CVs with different
// conventions than cyberline's internal buses. When ptMode is on, the
// TRACT POLY output re-encodes in place to a 10-channel PT-oriented bus
// (LARYNX POLY is mode-invariant and unchanged). The breakout module
// (CyberlinePtBreakout) splits this bus into PT's individual CV inputs.
//
// Channel map (PT mode):
//   ch0  TNGX      ±10 V  -> PT TONGUE XI  (attenuversion 1.0 => ±1 offset)
//   ch1  TNGY      ±10 V  -> PT TONGUE YI
//   ch2  THROAT X  ±10 V  -> PT THROAT XI  (constriction axis 0.06..1)
//   ch3  FRIC FC   0-5 V  -> PT FRICFCI    (x2000 Hz/V => 0-10 kHz)
//   ch4  FRIC LVL  0-10 V -> PT FRICL      (0-10 V = 0-1 fricative level)
//   ch5  VEL       0-10 V -> monitor only  (PT has no velum CV)
//   ch6  TENS      0 V    (offset convention: sink owns tension)
//   ch7  PRESS     0-10 V -> monitor only  (PT has no pressure CV)
//   ch8  VOICING   0-10 V -> monitor only  (breakout fuses into TENSE)
//   ch9  spare     0 V
// The same map with channels 0-4 re-encoded; TENS/PRESS/VOICING/VEL
// channels carry identical values in both modes.
struct PtBusEncode {
	// Encode one frame's cyberline-channel values into PT mode channels.
	// in:  [0]=tngX (±5), [1]=tngY (±5), [2]=vel (0-10), [3]=cpos (0-10),
	//      [4]=cwid (±5), [5]=fric (0-10), [6..8] pass through.
	// out: 10 channels per the map above.
	static void encode(const float* in, float* out) {
		// Tongue: cyberline ±5 V around 5.0 -> PT ±10 V around the knob
		// (PT: value = knob + attenuversion * V/20, so ±10 V = ±1 on the
		// 0..1 tongue axes; our ±5 offset = ±1 in our 0..10 space, doubled
		// to fill PT's span with the same articulator excursion).
		out[0] = in[0] * 2.f;
		out[1] = in[1] * 2.f;
		// Constriction position 0..10 (glottal->labial) -> PT throat X
		// 0.06..1.0 mapped to ±10 V (PT adds V/20 to its 0.06..0.999 knob):
		// axis position u/10 -> V = (u/10 * 2 - 1) * 10
		float u = rack::math::clamp(in[3], 0.f, 10.f) / 10.f;
		out[2] = (u * 2.f - 1.f) * 10.f;
		// Constriction width ±5 V around 5.0 -> fricative filter cutoff
		// 0-5 V (PT FRICFCI: 2000 Hz/V, so 5 V = 10 kHz ceiling)
		float w = rack::math::clamp(in[4] + 5.f, 0.f, 10.f);   // 0..10 narrow->wide
		out[3] = (1.f - w / 10.f) * 5.f;                       // narrow = bright
		// Fricative level: direct match (PT FRICL is 0-10 V = 0-1)
		out[4] = in[5];
		// Velum, tension, pressure, voicing: pass through unchanged
		out[5] = in[2];
		out[6] = in[6];
		out[7] = in[7];
		out[8] = in[8];
		out[9] = 0.f;
	}
};

extern rack::plugin::Model* modelCyberlineLarynx;extern rack::plugin::Model* modelCyberlineLungs;
extern rack::plugin::Model* modelCyberlineOralChamber;
extern rack::plugin::Model* modelCyberlineVoice;
extern rack::plugin::Model* modelCyberlineHeart;
extern rack::plugin::Model* modelCyberlineBroca;
extern rack::plugin::Model* modelCyberlineBrocaSq;
extern rack::plugin::Model* modelCyberlinePtBreakout;
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

// ---- Range submenu items ----

// A single "Inhale max" choice: clamps INHALE's max (min stays the full
// floor), pulls the current value inside, and persists via the module's
// json marker (written by CyberlineLungs::dataToJson / read in
// dataFromJson).
struct InhaleMaxItem : rack::ui::MenuItem {
	rack::engine::Module* module = nullptr;
	int paramId = 0;
	float maxS = 5.f;

	void onAction(const rack::event::Action& e) override {
		MenuItem::onAction(e);
		if (!module)
			return;
		applyParamRange(module->paramQuantities[paramId], 0.05f, maxS);
	}
};

// Append the "Inhale max" submenu items to a knob's context menu.
// curMax selects the CHECKMARK; the engine reads the range from the
// ParamQuantity itself, so no engine change is needed for CYCLE/TRIG.
inline void appendInhaleMaxMenu(rack::ui::Menu* menu, rack::engine::Module* module, int paramId) {
	menu->addChild(rack::createSubmenuItem("Inhale max", "",
		[=](rack::ui::Menu* sub) {
			float curMax = module && module->paramQuantities[paramId]
				? module->paramQuantities[paramId]->getMaxValue() : 5.f;
			for (int i = 0; i < (int) (sizeof(INHALE_MAX_CHOICES) / sizeof(INHALE_MAX_CHOICES[0])); i++) {
				auto* it = rack::createMenuItem<InhaleMaxItem>(INHALE_MAX_LABELS[i],
					CHECKMARK(std::fabs(curMax - INHALE_MAX_CHOICES[i]) < 0.01f));
				it->module = module;
				it->paramId = paramId;
				it->maxS = INHALE_MAX_CHOICES[i];
				sub->addChild(it);
			}
		}));
}

// A single size-band choice for an anatomy knob. kind: 0 = FOLDLEN (mm),
// 1 = VTL (cm). The band's lo/hi pair is read from the band table; the
// index persists via the module's json marker ("sizeBand" / "vSizeBand").
struct SizeBandItem : rack::ui::MenuItem {
	rack::engine::Module* module = nullptr;
	int paramId = 0;
	int bandIndex = 0;
	int kind = 0;   // 0 = FOLDLEN, 1 = VTL
	// Optional second knob applied together (Voice: FOLD + VTL in one band).
	// < 0 = unused.
	int secondParamId = -1;
	int secondKind = -1;

	void onAction(const rack::event::Action& e) override {
		MenuItem::onAction(e);
		if (!module || bandIndex < 0 || bandIndex >= NUM_SIZE_BANDS)
			return;
		const SizeBand& b = SIZE_BANDS[bandIndex];
		float lo = (kind == 0) ? b.foldLo : b.vtlLo;
		float hi = (kind == 0) ? b.foldHi : b.vtlHi;
		applyParamRange(module->paramQuantities[paramId], lo, hi);
		if (secondParamId >= 0 && secondKind >= 0) {
			float slo = (secondKind == 0) ? b.foldLo : b.vtlLo;
			float shi = (secondKind == 0) ? b.foldHi : b.vtlHi;
			applyParamRange(module->paramQuantities[secondParamId], slo, shi);
		}
	}
};

// Append the "Size range" submenu items for one anatomy knob, nested inside
// a parent submenu ("Size range") to keep the module menu compact.
// curBand selects the CHECKMARK (the caller reads the persisted index or
// derives it from the current range when absent). Optional secondParamId/
// secondKind applies a paired knob with the same band (Voice: FOLD + VTL).
inline void appendSizeBandMenu(rack::ui::Menu* menu, rack::engine::Module* module, int paramId, int kind, int curBand, int secondParamId = -1, int secondKind = -1) {
	menu->addChild(rack::createSubmenuItem("Size range", "",
		[=](rack::ui::Menu* sub) {
			for (int i = 0; i < NUM_SIZE_BANDS; i++) {
				const SizeBand& b = SIZE_BANDS[i];
				std::string label = std::string(b.name) + " \xC2\xB7 "
					+ (kind == 0
						? (std::string("") + formatMm(b.foldLo) + "-" + formatMm(b.foldHi) + " mm")
						: (std::string("") + formatMm(b.vtlLo) + "-" + formatMm(b.vtlHi) + " cm"))
					+ " (" + b.cue + ")";
				auto* it = rack::createMenuItem<SizeBandItem>(label, CHECKMARK(i == curBand));
				it->module = module;
				it->paramId = paramId;
				it->bandIndex = i;
				it->kind = kind;
				it->secondParamId = secondParamId;
				it->secondKind = secondKind;
				sub->addChild(it);
			}
			sub->addChild(new rack::ui::MenuSeparator);
			// Example animals for the selected band, as a disabled info line
			if (curBand >= 0 && curBand < NUM_SIZE_BANDS)
				sub->addChild(rack::createMenuLabel(std::string("Examples: ") + SIZE_BANDS[curBand].examples));
		}));
}

// Derive which band (if any) a knob's current range corresponds to; -1 when
// the range is custom/absent (e.g. legacy patches before persistence).
inline int deriveBandIndex(rack::engine::Module* module, int paramId, int kind) {
	if (!module || !module->paramQuantities[paramId])
		return -1;
	float lo = module->paramQuantities[paramId]->getMinValue();
	float hi = module->paramQuantities[paramId]->getMaxValue();
	for (int i = 0; i < NUM_SIZE_BANDS; i++) {
		float blo = (kind == 0) ? SIZE_BANDS[i].foldLo : SIZE_BANDS[i].vtlLo;
		float bhi = (kind == 0) ? SIZE_BANDS[i].foldHi : SIZE_BANDS[i].vtlHi;
		if (std::fabs(lo - blo) < 0.01f && std::fabs(hi - bhi) < 0.01f)
			return i;
	}
	return -1;
}

// ---- TaperKnob: multiplicative drag for wide logarithmic spans ----
// Stock Rack knobs move value linearly with drag distance, so a 0.05-5 s
// range gives the bottom decade only ~9% of travel. This knob drags the
// value multiplicatively (equal pixel distance = equal ratio change), which
// gives every decade the same travel; the gain is tuned so speed at the top
// of the range matches the stock feel. Scroll wheel uses the same curve;
// shift-drag precision, double-click reset, and the context-menu value
// field stay stock (raw units).
struct TaperKnob : rack::componentlibrary::RoundSmallBlackKnob {
	// Ratio change per pixel of drag (stock small-knob linear speed at
	// max->min over ~150 px is ~2.7 units/s; ~0.5%/px feels equivalent at
	// mid-range and gives ~3 decades over the drag span).
	float perPixel = 0.005f;

	void onDragMove(const rack::event::DragMove& e) override {
		auto* pq = getParamQuantity();
		if (!pq || e.button != GLFW_MOUSE_BUTTON_LEFT) {
			RoundSmallBlackKnob::onDragMove(e);
			return;
		}
		float lo = pq->getMinValue(), hi = pq->getMaxValue();
		if (lo <= 0.f || hi <= lo) {
			// Degenerate/nonpositive span: fall back to stock behavior
			RoundSmallBlackKnob::onDragMove(e);
			return;
		}
		// Vertical drag up = increase. Rack screen coords have y growing
		// downward, so up-drag gives a NEGATIVE mouseDelta.y — subtract it
		// (stock knobs do the same via (delta.x - delta.y)).
		float v = pq->getValue();
		float norm = std::log(v / lo) / std::log(hi / lo);          // 0..1
		norm -= e.mouseDelta.y * perPixel / std::log(hi / lo);
		norm = rack::math::clamp(norm, 0.f, 1.f);
		pq->setValue(lo * std::pow(hi / lo, norm));
	}
};

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
