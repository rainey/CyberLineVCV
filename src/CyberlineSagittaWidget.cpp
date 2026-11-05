// CyberlineSagittaWidget — programmatic panel (no SVG assets). 26 HP.
// A side-view (sagittal) fluoroscopy of the vocal system, drawn in the
// suite's neon style from the core's live telemetry snapshot:
//
//   - lungs + diaphragm inflate with Lungs VOLUME; airflow particles ride
//     the trachea with the FLOW sign (cyan inhale / coral exhale) and exit
//     through the mouth or the nostrils depending on the velum
//   - larynx box with vocal folds that vibrate at the measured F0 of the
//     patched voiced audio; tension colors them cyan->magenta, subglottal
//     pressure glows below
//   - oral tract drawn from the replica waveguide's ACTUAL diameter
//     profile: the tongue body rises toward the palate as the cavity
//     narrows (the drawn constriction IS the model's constriction); the
//     nasal branch is shaped by the velum
//   - the internal sound-pressure meters glow amber inside the cavity
//     while it speaks; turbulence sparks at the fricative constriction
//   - exaggerations: big cartoon eye that glances at the constriction,
//     cheek/throat puff under pressure with an occluded tract
//
// Geometry note: the model's section indexing places the velopharynx
// (nasal-branch junction, noseStart = 32/44) 73% of the way from glottis
// to lips — the ascending pharynx IS most of the tract. The drawing maps
// section index to arc length piecewise (knot at the bend) so the drawn
// constriction lands at the anatomically faithful spot.
//
// All state comes from CyberlineSagittaCore::snapshot() (double-buffered,
// atomic publish); this TU never touches the audio path.

#include "CyberlineSagitta.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>
#include <chrono>

namespace cyberline {

// ---- tiny polyline toolkit ----
struct SPt { float x, y; };

static float polyLen(const SPt* p, int n) {
	float L = 0.f;
	for (int i = 1; i < n; i++) {
		float dx = p[i].x - p[i - 1].x, dy = p[i].y - p[i - 1].y;
		L += std::sqrt(dx * dx + dy * dy);
	}
	return L;
}

// Cumulative length fraction at polyline vertex `idx`.
static float polyVertexFrac(const SPt* p, int n, int idx) {
	if (idx <= 0) return 0.f;
	if (idx >= n - 1) return 1.f;
	return polyLen(p, idx + 1) / polyLen(p, n);
}

// Sample a polyline by length fraction s in 0..1.
static SPt polyAt(const SPt* p, int n, float s) {
	float total = polyLen(p, n);
	float target = s * total;
	float acc = 0.f;
	for (int i = 1; i < n; i++) {
		float dx = p[i].x - p[i - 1].x, dy = p[i].y - p[i - 1].y;
		float seg = std::sqrt(dx * dx + dy * dy);
		if (acc + seg >= target || i == n - 1) {
			float f = (seg > 0.f) ? (target - acc) / seg : 0.f;
			f = f < 0.f ? 0.f : (f > 1.f ? 1.f : f);
			return {p[i - 1].x + dx * f, p[i - 1].y + dy * f};
		}
		acc += seg;
	}
	return p[n - 1];
}

// Unit tangent at length fraction s (central difference).
static SPt polyTangent(const SPt* p, int n, float s) {
	float e = 0.01f;
	SPt a = polyAt(p, n, s - e < 0.f ? 0.f : s - e);
	SPt b = polyAt(p, n, s + e > 1.f ? 1.f : s + e);
	float dx = b.x - a.x, dy = b.y - a.y;
	float L = std::sqrt(dx * dx + dy * dy);
	if (L < 1e-5f) return {1.f, 0.f};
	return {dx / L, dy / L};
}

// "Palate side" unit normal (screen coords, y down): rotate tangent by
// -90 deg. A rightward-running path gets an UP normal (palate roof); the
// ascending pharynx (tangent up) gets a BACKWARD normal (posterior wall).
static SPt polyNormal(const SPt* p, int n, float s) {
	SPt t = polyTangent(p, n, s);
	return {t.y, -t.x};
}

// ---- anatomical control paths (hand-tuned, panel pixel space) ----

// Oral tract centerline, glottis (t=0) -> lips (t=1). Vertex 8 is the
// velopharynx bend; noseStart sections land there via the piecewise map.
static const SPt TRACT_CP[] = {
	{196, 238}, {190, 222}, {184, 206}, {176, 192}, {166, 178},
	{158, 164}, {152, 150}, {152, 136}, {158, 126},
	{170, 120}, {186, 116}, {206, 114}, {228, 116}, {244, 120}, {256, 122},
};
static const int TRACT_NCP = (int) (sizeof(TRACT_CP) / sizeof(SPt));
static const int TRACT_BEND_VTX = 8;   // velopharynx bend vertex

// Nasal branch, velum junction (= bend vertex) -> nostril exit.
static const SPt NOSE_CP[] = {
	{158, 126}, {160, 112}, {168, 100}, {182, 90}, {202, 82},
	{226, 76}, {246, 76}, {260, 84}, {268, 94}, {266, 102},
};
static const int NOSE_NCP = (int) (sizeof(NOSE_CP) / sizeof(SPt));

// Trachea, larynx bottom -> lungs top (drawn downward).
static const SPt TRACHEA_CP[] = {
	{196, 260}, {190, 280}, {176, 296}, {156, 308}, {136, 314},
};
static const int TRACHEA_NCP = (int) (sizeof(TRACHEA_CP) / sizeof(SPt));

// Airflow particle routes: lungs -> up the trachea -> out. Length
// fraction 0 = deep in the lungs, 1 = outside the face.
static const SPt AIR_ORAL[] = {
	{116, 308}, {136, 314}, {156, 308}, {176, 296}, {190, 280}, {196, 260},
	{196, 238}, {190, 222}, {184, 206}, {176, 192}, {166, 178}, {158, 164},
	{152, 150}, {152, 136}, {158, 126}, {170, 120}, {186, 116}, {206, 114},
	{228, 116}, {244, 120}, {256, 122}, {272, 124},
};
static const int AIR_ORAL_N = (int) (sizeof(AIR_ORAL) / sizeof(SPt));
static const SPt AIR_NOSE[] = {
	{116, 308}, {136, 314}, {156, 308}, {176, 296}, {190, 280}, {196, 260},
	{196, 238}, {190, 222}, {184, 206}, {176, 192}, {166, 178}, {158, 164},
	{152, 150}, {152, 136}, {158, 126}, {160, 112}, {168, 100}, {182, 90},
	{202, 82}, {226, 76}, {246, 76}, {260, 84}, {268, 94}, {276, 96},
};
static const int AIR_NOSE_N = (int) (sizeof(AIR_NOSE) / sizeof(SPt));

// Head profile (facing right), exaggerated cartoon proportions. The mouth
// is drawn OPEN: the outline dips into the mouth corner between the lips
// so the tract exit is visible; a small notch marks the nostril.
static const SPt HEAD_CP[] = {
	{36, 262}, {34, 196}, {36, 136}, {42, 94}, {60, 50}, {112, 20},
	{172, 12}, {228, 28}, {252, 52}, {262, 72}, {268, 86}, {276, 98},
	{266, 104}, {270, 110}, {274, 114}, {256, 122}, {272, 128}, {260, 142},
	{246, 156}, {200, 170}, {150, 186}, {130, 204}, {124, 240}, {122, 278},
};
static const int HEAD_NCP = (int) (sizeof(HEAD_CP) / sizeof(SPt));

static const SPt JAW_CP[] = {
	{44, 250}, {96, 222}, {148, 206}, {178, 210}, {192, 226},
};
static const int JAW_NCP = (int) (sizeof(JAW_CP) / sizeof(SPt));

namespace {

struct CyberlineSagittaWidget;

// Browser-preview core: neutral anatomy when no module is instantiated.
static CyberlineSagittaCore& browserCore() {
	static CyberlineSagittaCore core;
	return core;
}

struct CyberlineSagittaWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 26 * 15.f;

	// ---- widget-side animation state (pure decoration) ----
	struct Particle {
		float s = 0.f;      // length fraction along its route
		float spd = 0.25f;  // length fractions per second
		int route = 0;      // 0 oral, 1 nasal
		bool alive = false;
	};
	static constexpr int NP = 48;
	Particle parts[NP];
	float spawnAcc = 0.f;
	float blinkT = 3.f;
	bool blinkSquint = false;
	float pupilX = 0.f, pupilY = 0.f;
	std::chrono::steady_clock::time_point lastDraw;
	bool lastDrawInit = false;

	// ---- 2-position snap switch (BUS / DIRECT), suite pattern ----
	struct ModeSwitch : rack::app::ParamWidget {
		float pressY = 0.f, oldValue = 0.f, dragDeltaY = 0.f;
		bool dragged = false;

		ModeSwitch() {
			box.size = rack::Vec(16.f, 26.f);
		}

		void initParamQuantity() override {
			ParamWidget::initParamQuantity();
			if (auto* pq = getParamQuantity()) {
				pq->snapEnabled = true;
				pq->smoothEnabled = false;
			}
		}

		void onButton(const rack::event::Button& e) override {
			if (e.action == GLFW_PRESS && e.button == GLFW_MOUSE_BUTTON_LEFT)
				pressY = e.pos.y;
			ParamWidget::onButton(e);
		}

		void onDragStart(const rack::event::DragStart& e) override {
			ParamWidget::onDragStart(e);
			if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
			if (auto* pq = getParamQuantity()) oldValue = std::round(pq->getValue());
			dragDeltaY = 0.f;
			dragged = false;
		}

		void onDragMove(const rack::event::DragMove& e) override {
			ParamWidget::onDragMove(e);
			if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
			auto* pq = getParamQuantity();
			if (!pq) return;
			dragDeltaY += e.mouseDelta.y;
			if (std::fabs(dragDeltaY) >= 2.5f) dragged = true;
			if (dragged) {
				float stepPx = 9.f;
				float target = std::round(oldValue + dragDeltaY / stepPx);
				pq->setValue(rack::math::clamp(target, pq->getMinValue(), pq->getMaxValue()));
			}
		}

		void onDragEnd(const rack::event::DragEnd& e) override {
			ParamWidget::onDragEnd(e);
			if (e.button != GLFW_MOUSE_BUTTON_LEFT) return;
			auto* pq = getParamQuantity();
			if (!pq) return;
			if (!dragged)
				pq->setValue(std::round(oldValue) == 0.f ? 1.f : 0.f); // click toggles
			float newValue = std::round(pq->getValue());
			if (module && oldValue != newValue) {
				auto* h = new rack::history::ParamChange;
				h->name = "move switch";
				h->moduleId = module->id;
				h->paramId = paramId;
				h->oldValue = oldValue;
				h->newValue = newValue;
				APP->history->push(h);
			}
		}

		void draw(const DrawArgs& args) override {
			NVGcontext* vg = args.vg;
			float w = box.size.x, h = box.size.y;
			float v = module ? module->params[paramId].getValue() : 0.f;
			// track
			nvgBeginPath(vg);
			nvgRoundedRect(vg, w * 0.5f - 3.4f, 2.f, 6.8f, h - 4.f, 3.4f);
			nvgFillColor(vg, nvgRGB(0x0a, 0x0c, 0x10));
			nvgFill(vg);
			nvgStrokeWidth(vg, 1.f);
			nvgStrokeColor(vg, nvgRGB(0x33, 0x3c, 0x46));
			nvgStroke(vg);
			// handle
			float hy = 4.f + (1.f - v) * (h - 12.f);
			nvgBeginPath(vg);
			nvgCircle(vg, w * 0.5f, hy + 3.f, 4.6f);
			nvgFillColor(vg, nvgRGB(0xd8, 0xd2, 0xc8));
			nvgFill(vg);
			nvgStrokeColor(vg, nvgRGB(0x48, 0xd8, 0xf8));
			nvgStrokeWidth(vg, 1.2f);
			nvgStroke(vg);
		}
	};

	// ================= anatomy view =================
	struct FluoroView : rack::widget::Widget {
		CyberlineSagittaWidget* w;

		// bend arc fraction of the oral centerline (velopharynx)
		float bendFrac = 0.6f;

		void begin() {
			bendFrac = polyVertexFrac(TRACT_CP, TRACT_NCP, TRACT_BEND_VTX);
		}

		// section index -> arc fraction (piecewise, knot at the bend).
		// Capped below 1.0 so the lips end INSIDE the mouth corner (the
		// polyline continues past the face for the airflow routes only).
		float arcF(int i, int n, int noseStart) const {
			if (noseStart <= 0 || noseStart >= n - 1)
				return 0.955f * (float) i / (float) (n - 1);
			if (i <= noseStart)
				return 0.955f * bendFrac * (float) i / (float) noseStart;
			return 0.955f * (bendFrac + (1.f - bendFrac)
				* (float) (i - noseStart) / (float) (n - 1 - noseStart));
		}

		// neon double-stroke helper (suite idiom)
		static void strokeNeon(NVGcontext* vg, NVGcolor col,
				float coreW = 1.2f, float glowW = 3.2f, int glowAlpha = 40) {
			nvgStrokeWidth(vg, glowW);
			nvgStrokeColor(vg, nvgRGBA((int) (col.r * 255), (int) (col.g * 255),
				(int) (col.b * 255), glowAlpha));
			nvgStroke(vg);
			nvgStrokeWidth(vg, coreW);
			nvgStrokeColor(vg, col);
			nvgStroke(vg);
		}

		static float nowSec() {
			return std::chrono::duration<float>(
				std::chrono::steady_clock::now().time_since_epoch()).count();
		}

		void draw(const DrawArgs& args) override {
			NVGcontext* vg = args.vg;
			CyberlineSagitta* module = dynamic_cast<CyberlineSagitta*>(
				w->getModule());
			const CyberlineSagittaCore::Snapshot* s = module
				? module->core.snapshot()
				: browserCore().snapshot();
			if (!s || !s->alive)
				return;
			begin();

			// dt for particles / blink
			auto now = std::chrono::steady_clock::now();
			float dt = 1.f / 60.f;
			if (w->lastDrawInit) {
				dt = std::chrono::duration<float>(now - w->lastDraw).count();
				dt = dt < 0.f ? 0.f : (dt > 0.1f ? 0.1f : dt);
			}
			w->lastDraw = now;
			w->lastDrawInit = true;

			const float EX = 12.f;   // px per cm (exaggerated anatomy scale)

			// inside-out: face fill first, anatomy on top of it, profile
			// stroke last so it frames the whole figure
			drawHeadFill(vg);
			drawLungs(vg, s);
			drawTrachea(vg);
			drawOralCavity(vg, s, EX);
			drawNasalBranch(vg, s);
			drawLarynx(vg, s);
			drawProfileStroke(vg);
			drawAirParticles(vg, s, dt);
			drawFace(vg, s, dt);
			drawHud(vg, s);
		}

		// ---- lungs + diaphragm ----
		void drawLungs(NVGcontext* vg, const CyberlineSagittaCore::Snapshot* s) {
			float vol = s->lungsVol;
			float scale = 0.72f + 0.42f * vol;
			// chest boundary (behind lungs)
			nvgBeginPath(vg);
			nvgMoveTo(vg, 52, 268);
			nvgLineTo(vg, 46, 296);
			nvgLineTo(vg, 64, 320);
			nvgLineTo(vg, 174, 316);
			nvgLineTo(vg, 178, 298);
			nvgLineTo(vg, 172, 270);
			nvgStrokeWidth(vg, 1.f);
			nvgStrokeColor(vg, nvgRGBA(0x38, 0x42, 0x4c, 140));
			nvgStroke(vg);

			// rear lobe
			nvgBeginPath(vg);
			nvgEllipse(vg, 92, 298, 36, 20 * scale);
			nvgFillColor(vg, nvgRGBA(0x48, 0xd8, 0xf8, 26));
			nvgFill(vg);
			nvgStrokeWidth(vg, 1.4f);
			nvgStrokeColor(vg, nvgRGBA(0x48, 0xd8, 0xf8, 150));
			nvgStroke(vg);
			// front lobe
			nvgBeginPath(vg);
			nvgEllipse(vg, 140, 294, 30, 16 * scale);
			nvgFillColor(vg, nvgRGBA(0x48, 0xd8, 0xf8, 36));
			nvgFill(vg);
			nvgStrokeColor(vg, nvgRGBA(0x48, 0xd8, 0xf8, 180));
			nvgStroke(vg);

			// diaphragm arc (bows down as the lungs fill)
			float dip = 322.f - 8.f * vol;
			nvgBeginPath(vg);
			nvgMoveTo(vg, 58, 318);
			nvgQuadTo(vg, 119, dip + 6, 178, 314);
			strokeNeon(vg, nvgRGBA(0x7e, 0xe0, 0xa8, 170), 1.2f, 3.f, 28);
		}

		// ---- trachea with cartilage rings ----
		void drawTrachea(NVGcontext* vg) {
			nvgBeginPath(vg);
			nvgMoveTo(vg, TRACHEA_CP[0].x, TRACHEA_CP[0].y);
			for (int i = 1; i < TRACHEA_NCP; i++)
				nvgLineTo(vg, TRACHEA_CP[i].x, TRACHEA_CP[i].y);
			nvgStrokeWidth(vg, 5.f);
			nvgStrokeColor(vg, nvgRGBA(0x0a, 0x10, 0x16, 220));
			nvgLineCap(vg, NVG_ROUND);
			nvgStroke(vg);
			nvgStrokeWidth(vg, 1.2f);
			nvgStrokeColor(vg, nvgRGBA(0x58, 0x98, 0xb0, 200));
			nvgStroke(vg);
			// rings
			for (float f = 0.08f; f < 0.96f; f += 0.13f) {
				SPt p = polyAt(TRACHEA_CP, TRACHEA_NCP, f);
				SPt nrm = polyNormal(TRACHEA_CP, TRACHEA_NCP, f);
				nvgBeginPath(vg);
				nvgMoveTo(vg, p.x - nrm.x * 2.6f, p.y - nrm.y * 2.6f);
				nvgLineTo(vg, p.x + nrm.x * 2.6f, p.y + nrm.y * 2.6f);
				nvgStrokeWidth(vg, 1.f);
				nvgStrokeColor(vg, nvgRGBA(0x58, 0x98, 0xb0, 110));
				nvgStroke(vg);
			}
		}

		// ---- airflow particles ----
		void drawAirParticles(NVGcontext* vg,
				const CyberlineSagittaCore::Snapshot* s, float dt) {
			float flow = s->lungsFlow;   // + inhale, - exhale
			bool exhale = flow < -0.04f;
			bool inhale = flow > 0.04f;
			float rate = 16.f * std::fmin(std::fabs(flow), 1.f);
			bool nasalOpen = s->velum > 0.25f;

			// spawning
			if (exhale || inhale) {
				w->spawnAcc += rate * dt;
				while (w->spawnAcc >= 1.f) {
					w->spawnAcc -= 1.f;
					for (int i = 0; i < NP; i++) {
						Particle& p = w->parts[i];
						if (p.alive) continue;
						p.alive = true;
						p.route = (nasalOpen && (i & 1)) ? 1 : 0;
						if (exhale) {
							p.s = 0.f;
							p.spd = 0.16f + 0.16f * std::fabs(flow);
						} else {
							p.s = 1.f;
							p.spd = -(0.14f + 0.14f * flow);
						}
						break;
					}
				}
			} else {
				w->spawnAcc = 0.f;
			}

			for (int i = 0; i < NP; i++) {
				Particle& p = w->parts[i];
				if (!p.alive) continue;
				p.s += p.spd * dt;
				if (p.s <= 0.f || p.s >= 1.f) {
					p.alive = false;
					continue;
				}
				const SPt* path = (p.route == 1) ? AIR_NOSE : AIR_ORAL;
				int n = (p.route == 1) ? AIR_NOSE_N : AIR_ORAL_N;
				SPt pt = polyAt(path, n, p.s);
				NVGcolor col = (p.spd > 0.f)
					? nvgRGBA(0xff, 0x6a, 0x74, 200)   // exhale coral
					: nvgRGBA(0x48, 0xe0, 0xff, 200);  // inhale cyan
				nvgBeginPath(vg);
				nvgCircle(vg, pt.x, pt.y, 1.6f);
				nvgFillColor(vg, col);
				nvgFill(vg);
				nvgBeginPath(vg);
				nvgCircle(vg, pt.x, pt.y, 3.2f);
				nvgFillColor(vg, nvgRGBA((int) (col.r * 255), (int) (col.g * 255),
					(int) (col.b * 255), 40));
				nvgFill(vg);
			}

			// nasal stream hint when the port is open during speech
			if (nasalOpen && s->noseEnv > 0.02f) {
				float tsec = nowSec();
				for (int k = 0; k < 3; k++) {
					float f = 0.55f + 0.15f * k
						+ 0.05f * std::fmod(k * 0.33f + std::fmod(tsec, 1.f), 1.f);
					SPt pt = polyAt(NOSE_CP, NOSE_NCP, f);
					nvgBeginPath(vg);
					nvgCircle(vg, pt.x, pt.y, 1.4f);
					nvgFillColor(vg, nvgRGBA(0xb0, 0xe8, 0xff, 90));
					nvgFill(vg);
				}
			}
		}

		// ---- head/face profile ----
		void drawHeadFill(NVGcontext* vg) {
			// soft face fill (behind all anatomy)
			nvgBeginPath(vg);
			nvgMoveTo(vg, HEAD_CP[0].x, HEAD_CP[0].y);
			for (int i = 1; i < HEAD_NCP; i++)
				nvgLineTo(vg, HEAD_CP[i].x, HEAD_CP[i].y);
			nvgClosePath(vg);
			nvgFillColor(vg, nvgRGBA(0x14, 0x18, 0x20, 230));
			nvgFill(vg);
			// jaw hint
			nvgBeginPath(vg);
			nvgMoveTo(vg, JAW_CP[0].x, JAW_CP[0].y);
			for (int i = 1; i < JAW_NCP; i++)
				nvgLineTo(vg, JAW_CP[i].x, JAW_CP[i].y);
			nvgStrokeWidth(vg, 1.f);
			nvgStrokeColor(vg, nvgRGBA(0xc7, 0x4d, 0x5e, 110));
			nvgStroke(vg);
		}

		// neon profile stroke only (frames the figure on top)
		void drawProfileStroke(NVGcontext* vg) {
			nvgBeginPath(vg);
			nvgMoveTo(vg, HEAD_CP[0].x, HEAD_CP[0].y);
			for (int i = 1; i < HEAD_NCP; i++)
				nvgLineTo(vg, HEAD_CP[i].x, HEAD_CP[i].y);
			strokeNeon(vg, nvgRGBA(0xff, 0x7a, 0x6b, 200), 1.2f, 3.4f, 26);
		}

		// ---- nasal branch (velum-shaped) ----
		void drawNasalBranch(NVGcontext* vg,
				const CyberlineSagittaCore::Snapshot* s) {
			const float EXN = 9.f;   // px per cm on the nasal branch
			int nl = s->noseLength;
			if (nl < 2) return;
			// build wall points from noseDiam profile
			SPt up[NOSE_SECTIONS_MAX], dn[NOSE_SECTIONS_MAX];
			for (int i = 0; i < nl; i++) {
				float f = (float) i / (float) (nl - 1);
				SPt c = polyAt(NOSE_CP, NOSE_NCP, f);
				SPt nrm = polyNormal(NOSE_CP, NOSE_NCP, f);
				float d = s->noseDiam[i];
				d = d < 0.04f ? 0.04f : (d > 2.2f ? 2.2f : d);
				up[i] = {c.x + nrm.x * d * EXN * 0.5f, c.y + nrm.y * d * EXN * 0.5f};
				dn[i] = {c.x - nrm.x * d * EXN * 0.5f, c.y - nrm.y * d * EXN * 0.5f};
			}
			// cavity fill
			nvgBeginPath(vg);
			nvgMoveTo(vg, up[0].x, up[0].y);
			for (int i = 1; i < nl; i++) nvgLineTo(vg, up[i].x, up[i].y);
			for (int i = nl - 1; i >= 0; i--) nvgLineTo(vg, dn[i].x, dn[i].y);
			nvgClosePath(vg);
			nvgFillColor(vg, nvgRGBA(0x08, 0x0a, 0x0e, 225));
			nvgFill(vg);
			// walls
			nvgBeginPath(vg);
			nvgMoveTo(vg, up[0].x, up[0].y);
			for (int i = 1; i < nl; i++) nvgLineTo(vg, up[i].x, up[i].y);
			nvgStrokeWidth(vg, 1.1f);
			nvgStrokeColor(vg, nvgRGBA(0x9a, 0xc2, 0xd8, 170));
			nvgStroke(vg);
			nvgBeginPath(vg);
			nvgMoveTo(vg, dn[0].x, dn[0].y);
			for (int i = 1; i < nl; i++) nvgLineTo(vg, dn[i].x, dn[i].y);
			nvgStroke(vg);
			// nasal sound-pressure glow
			for (int i = 0; i < nl; i++) {
				float a = std::fmin(s->noseAmp[i] * 3.2f, 0.8f);
				if (a < 0.02f) continue;
				float f = (float) i / (float) (nl - 1);
				SPt c = polyAt(NOSE_CP, NOSE_NCP, f);
				nvgBeginPath(vg);
				nvgCircle(vg, c.x, c.y, 2.5f + 5.f * a);
				nvgFillColor(vg, nvgRGBA(0xb0, 0xe8, 0xff, (int) (a * 255)));
				nvgFill(vg);
			}
			// velum flap at the junction: closed = sealed up-forward,
			// open = hangs down into the pharynx
			float vel = s->velum;
			SPt j = NOSE_CP[0];
			float hang = 4.f + 10.f * vel;    // flap length when engaged
			float ang = -2.2f + 1.5f * vel;   // rad: up-forward -> down-forward
			SPt tip = {j.x + std::cos(ang) * hang, j.y - std::sin(ang) * hang};
			nvgBeginPath(vg);
			nvgMoveTo(vg, j.x, j.y);
			nvgLineTo(vg, tip.x, tip.y);
			strokeNeon(vg, nvgRGBA(0xc7, 0x9f, 0xfb, 190), 1.4f, 3.f, 36);
		}

		// ---- oral cavity: walls, tongue, wave glow ----
		void drawOralCavity(NVGcontext* vg,
				const CyberlineSagittaCore::Snapshot* s, float EX) {
			int n = s->n;
			if (n < 8) return;
			static const float PHARYNX_WALL = 13.f;  // posterior wall depth
			static const float PALATE = 22.f;        // palate height
			static const float FLOORH = 36.f;        // tongue body depth

			SPt cl[TRACT_SECTIONS_MAX];   // centerline per section
			SPt nrm[TRACT_SECTIONS_MAX];
			SPt up[TRACT_SECTIONS_MAX];   // upper wall (palate / throat back)
			SPt tg[TRACT_SECTIONS_MAX];   // tongue surface
			for (int i = 0; i < n; i++) {
				float f = arcF(i, n, s->noseStart);
				cl[i] = polyAt(TRACT_CP, TRACT_NCP, f);
				nrm[i] = polyNormal(TRACT_CP, TRACT_NCP, f);
				// wall depth: pharynx back, tall palate, taper to the lips
				float U;
				if (i < s->bladeStart) U = PHARYNX_WALL;
				else if (i < s->tipStart)
					U = PHARYNX_WALL + (PALATE - PHARYNX_WALL)
						* (float) (i - s->bladeStart)
						/ (float) (s->tipStart - s->bladeStart);
				else
					U = PALATE - (PALATE - 8.f)
						* (float) (i - s->tipStart)
						/ (float) (n - 1 - s->tipStart);
				up[i] = {cl[i].x + nrm[i].x * U, cl[i].y + nrm[i].y * U};
				float d = s->diam[i];
				d = d < 0.03f ? 0.03f : (d > 4.2f ? 4.2f : d);
				tg[i] = {cl[i].x + nrm[i].x * (U - d * EX),
					cl[i].y + nrm[i].y * (U - d * EX)};
			}

			// tongue body fill: surface -> floor line
			nvgBeginPath(vg);
			nvgMoveTo(vg, tg[0].x, tg[0].y);
			for (int i = 1; i < n; i++) nvgLineTo(vg, tg[i].x, tg[i].y);
			for (int i = n - 1; i >= 0; i--) {
				SPt fl = {cl[i].x - nrm[i].x * FLOORH, cl[i].y - nrm[i].y * FLOORH};
				nvgLineTo(vg, fl.x, fl.y);
			}
			nvgClosePath(vg);
			nvgFillColor(vg, nvgRGBA(0xc7, 0x4d, 0x5e, 64));
			nvgFill(vg);
			// tongue surface outline
			nvgBeginPath(vg);
			nvgMoveTo(vg, tg[0].x, tg[0].y);
			for (int i = 1; i < n; i++) nvgLineTo(vg, tg[i].x, tg[i].y);
			strokeNeon(vg, nvgRGBA(0xff, 0x6a, 0x8a, 215), 1.3f, 3.2f, 30);

			// cavity fill (dark space between palate and tongue)
			nvgBeginPath(vg);
			nvgMoveTo(vg, up[0].x, up[0].y);
			for (int i = 1; i < n; i++) nvgLineTo(vg, up[i].x, up[i].y);
			for (int i = n - 1; i >= 0; i--) nvgLineTo(vg, tg[i].x, tg[i].y);
			nvgClosePath(vg);
			nvgFillColor(vg, nvgRGBA(0x07, 0x09, 0x0d, 235));
			nvgFill(vg);

			// palate / pharyngeal wall
			nvgBeginPath(vg);
			nvgMoveTo(vg, up[0].x, up[0].y);
			for (int i = 1; i < n; i++) nvgLineTo(vg, up[i].x, up[i].y);
			nvgStrokeWidth(vg, 1.3f);
			nvgStrokeColor(vg, nvgRGBA(0xd8, 0xd2, 0xc8, 190));
			nvgStroke(vg);

			// constriction emphasis: ring where the cavity is narrowest
			float minD = 9.f;
			int minI = -1;
			for (int i = s->bladeStart; i < n; i++) {
				if (s->diam[i] < minD) {
					minD = s->diam[i];
					minI = i;
				}
			}
			if (minI >= 0 && minD < 1.f && s->constrPos > 0.01f) {
				SPt c = cl[minI];
				nvgBeginPath(vg);
				nvgCircle(vg, c.x, c.y, 3.f + 2.5f * minD);
				nvgStrokeWidth(vg, 1.4f);
				nvgStrokeColor(vg, nvgRGBA(0xff, 0xc4, 0x6b, 190));
				nvgStroke(vg);
				// fricative turbulence sparks
				if (s->fricative > 0.12f) {
					float tsec = nowSec();
					for (int k = 0; k < 7; k++) {
						float hx = std::fmod(0.7371f * (k * 13 + minI * 7) + tsec * 3.f, 1.f);
						float hy = std::fmod(0.5219f * (k * 29 + minI * 3) + tsec * 5.f, 1.f);
						float ox = (hx - 0.5f) * 14.f;
						float oy = (hy - 0.5f) * 14.f;
						nvgBeginPath(vg);
						nvgCircle(vg, c.x + ox, c.y + oy, 1.f);
						nvgFillColor(vg, nvgRGBA(0xff, 0xe0, 0x9a,
							(int) (110.f * s->fricative)));
						nvgFill(vg);
					}
				}
			}

			// internal sound-pressure glow (the actual waveguide meters):
			// a soft band along the cavity centerline instead of beads
			{
				// band outline through the section points
				nvgBeginPath(vg);
				nvgMoveTo(vg, cl[0].x, cl[0].y);
				for (int i = 1; i < n; i++) nvgLineTo(vg, cl[i].x, cl[i].y);
				nvgStrokeWidth(vg, 7.f);
				nvgLineCap(vg, NVG_ROUND);
				nvgLineJoin(vg, NVG_ROUND);
				nvgStrokeColor(vg, nvgRGBA(0xff, 0xd8, 0x7a, 34));
				nvgStroke(vg);
				nvgBeginPath(vg);
				nvgMoveTo(vg, cl[0].x, cl[0].y);
				for (int i = 1; i < n; i++) nvgLineTo(vg, cl[i].x, cl[i].y);
				nvgStrokeWidth(vg, 3.f);
				nvgStrokeColor(vg, nvgRGBA(0xff, 0xd8, 0x7a, 60));
				nvgStroke(vg);
				// hot core where the wave is strong
				for (int i = 0; i < n; i++) {
					float a = std::fmin(s->amp[i] * 3.4f, 0.85f);
					if (a < 0.05f) continue;
					nvgBeginPath(vg);
					nvgCircle(vg, cl[i].x, cl[i].y, 1.5f + 4.5f * a);
					nvgFillColor(vg, nvgRGBA(0xff, 0xe8, 0xb0, (int) (a * 190)));
					nvgFill(vg);
				}
			}
		}

		// ---- larynx box + vocal folds ----
		void drawLarynx(NVGcontext* vg, const CyberlineSagittaCore::Snapshot* s) {
			// box (contains the glottal end of the tract)
			nvgBeginPath(vg);
			nvgRoundedRect(vg, 172, 224, 52, 36, 4.f);
			nvgFillColor(vg, nvgRGBA(0x0a, 0x0e, 0x14, 220));
			nvgFill(vg);
			nvgStrokeWidth(vg, 1.2f);
			nvgStrokeColor(vg, nvgRGBA(0xc7, 0x9f, 0xfb, 140));
			nvgStroke(vg);

			// subglottal pressure glow under the folds
			float glow = std::fmin(s->press * (0.35f + 0.65f * s->voicedEnv), 1.f);
			if (glow > 0.02f) {
				NVGpaint g = nvgRadialGradient(vg, 198, 256, 2.f, 15.f,
					nvgRGBA(0xff, 0x88, 0x64, (int) (glow * 130)),
					nvgRGBA(0xff, 0x88, 0x64, 0));
				nvgBeginPath(vg);
				nvgRect(vg, 176, 244, 44, 14);
				nvgFillPaint(vg, g);
				nvgFill(vg);
			}

			// fold color: slack cyan -> tense magenta
			float tn = s->tension;
			NVGcolor fold = nvgRGBA(
				(int) (0x48 + tn * (0xff - 0x48)),
				(int) (0xd8 - tn * (0xd8 - 0x58)),
				(int) (0xf8 - tn * (0xf8 - 0xc8)), 235);

			// glottal opening: vibratory flutter when voiced, open when breathy
			float wob = std::fabs(std::sin(2.f * (float) M_PI * s->foldPhase));
			float gap = 5.2f + ((1.f + 1.9f * wob) - 5.2f) * s->voicing;
			float jit = (s->f0 > 0.f) ? 0.5f * std::sin(2.f * (float) M_PI
				* s->foldPhase * 2.f) : 0.f;
			float cy = 244.f + jit;

			// top fold (above glottis gap)
			nvgBeginPath(vg);
			nvgMoveTo(vg, 180, cy - gap * 0.5f);
			nvgLineTo(vg, 216, cy - gap * 0.5f);
			nvgLineTo(vg, 212, cy - gap * 0.5f - 6.f);
			nvgLineTo(vg, 184, cy - gap * 0.5f - 6.f);
			nvgClosePath(vg);
			nvgFillColor(vg, fold);
			nvgFill(vg);
			// bottom fold
			nvgBeginPath(vg);
			nvgMoveTo(vg, 180, cy + gap * 0.5f);
			nvgLineTo(vg, 216, cy + gap * 0.5f);
			nvgLineTo(vg, 212, cy + gap * 0.5f + 6.f);
			nvgLineTo(vg, 184, cy + gap * 0.5f + 6.f);
			nvgClosePath(vg);
			nvgFillColor(vg, fold);
			nvgFill(vg);
		}

		// ---- face details: eye, lips sound arcs ----
		void drawFace(NVGcontext* vg, const CyberlineSagittaCore::Snapshot* s,
				float dt) {
			int n = s->n;
			// where the eye looks: the constriction, else straight ahead
			SPt look = {252.f, 70.f};
			if (s->constrPos > 0.01f && n > 4) {
				float f = arcF((int) std::fmin(s->constrPos, n - 1.f),
					n, s->noseStart);
				look = polyAt(TRACT_CP, TRACT_NCP, f);
			} else if (s->lungsFlow < -0.05f) {
				look = {272.f, 116.f};   // watching its own breath
			}
			float tx = (look.x - 226.f) * 0.06f;
			float ty = (look.y - 66.f) * 0.05f;
			tx = tx < -3.5f ? -3.5f : (tx > 3.5f ? 3.5f : tx);
			ty = ty < -2.5f ? -2.5f : (ty > 2.5f ? 2.5f : ty);
			w->pupilX += (tx - w->pupilX) * std::fmin(dt * 8.f, 1.f);
			w->pupilY += (ty - w->pupilY) * std::fmin(dt * 8.f, 1.f);

			// blink timer
			w->blinkT -= dt;
			if (w->blinkT <= 0.f) {
				w->blinkT = 2.6f + 2.4f * std::fmod(look.x * 0.013f, 1.f);
				w->blinkSquint = true;
			}
			if (w->blinkSquint && w->blinkT < 2.35f) w->blinkSquint = false;
			float eyeRy = w->blinkSquint ? 1.4f : 6.2f;

			// eye white
			nvgBeginPath(vg);
			nvgEllipse(vg, 226, 66, 9.5f, eyeRy);
			nvgFillColor(vg, nvgRGBA(0xe8, 0xec, 0xf0, 220));
			nvgFill(vg);
			// pupil
			nvgBeginPath(vg);
			nvgCircle(vg, 226 + w->pupilX, 66 + w->pupilY, 3.1f);
			nvgFillColor(vg, nvgRGB(0x10, 0x14, 0x1c));
			nvgFill(vg);
			// brow (raises with fricative effort)
			float lift = 4.f * std::fmin(s->fricative, 1.f);
			nvgBeginPath(vg);
			nvgMoveTo(vg, 216, 54 - lift);
			nvgQuadTo(vg, 226, 50 - lift, 236, 53 - lift);
			nvgStrokeWidth(vg, 1.6f);
			nvgStrokeColor(vg, nvgRGBA(0xff, 0x7a, 0x6b, 180));
			nvgStroke(vg);

			// cheek + throat puff when pressurized behind an occlusion
			float minD = 9.f;
			for (int i = s->bladeStart; i < n; i++)
				minD = std::fmin(minD, s->diam[i]);
			float puff = (s->lungsPress > 0.45f && minD < 0.25f)
				? std::fmin((s->lungsPress - 0.45f) * 2.f, 1.f) : 0.f;
			if (puff > 0.01f) {
				nvgBeginPath(vg);
				nvgEllipse(vg, 248, 134, 11.f, 8.f);
				nvgFillColor(vg, nvgRGBA(0xff, 0x88, 0x64, (int) (52.f * puff)));
				nvgFill(vg);
				nvgBeginPath(vg);
				nvgEllipse(vg, 168, 196, 9.f, 13.f);
				nvgFillColor(vg, nvgRGBA(0xff, 0x88, 0x64, (int) (40.f * puff)));
				nvgFill(vg);
			}

			// nostril
			nvgBeginPath(vg);
			nvgCircle(vg, 267, 103, 1.8f);
			nvgFillColor(vg, nvgRGBA(0x10, 0x14, 0x1c, 220));
			nvgFill(vg);

			// radiated sound arcs at the lips
			float la = std::fmin(s->lipEnv * 2.2f, 1.f);
			for (int k = 0; k < 3; k++) {
				float a = la - 0.3f * k;
				if (a <= 0.02f) break;
				nvgBeginPath(vg);
				nvgArc(vg, 258, 124, 7.f + 6.f * k, -0.5f, 0.6f, NVG_CW);
				nvgStrokeWidth(vg, 1.4f);
				nvgStrokeColor(vg, nvgRGBA(0x7e, 0xe0, 0xa8, (int) (a * 190)));
				nvgStroke(vg);
			}
			// nasal radiation
			float na = std::fmin(s->noseEnv * 2.4f, 1.f) * std::fmin(s->velum * 2.f, 1.f);
			if (na > 0.03f) {
				nvgBeginPath(vg);
				nvgArc(vg, 268, 100, 6.f, -0.5f, 0.6f, NVG_CW);
				nvgStrokeWidth(vg, 1.4f);
				nvgStrokeColor(vg, nvgRGBA(0xb0, 0xe8, 0xff, (int) (na * 190)));
				nvgStroke(vg);
			}
		}

		// ---- HUD readout column (right) ----
		void drawHud(NVGcontext* vg, const CyberlineSagittaCore::Snapshot* s) {
			auto font = getLabelFont();
			if (!font || font->handle < 0) return;
			nvgFontFaceId(vg, font->handle);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgFontSize(vg, 6.5f);

			float x = 306.f;
			float y = 120.f;
			auto label = [&](float yy, const char* txt) {
				nvgFillColor(vg, nvgRGB(0x8a, 0x96, 0xa2));
				nvgText(vg, x, yy, txt, NULL);
			};
			auto bar = [&](float yy, float v, NVGcolor col, bool center = false) {
				v = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
				nvgBeginPath(vg);
				nvgRoundedRect(vg, x + 20.f, yy - 2.f, 28.f, 4.f, 2.f);
				nvgFillColor(vg, nvgRGBA(0x2a, 0x32, 0x3c, 160));
				nvgFill(vg);
				float wv = 28.f * v;
				float x0 = center ? x + 20.f + 14.f - wv * 0.5f : x + 20.f;
				nvgBeginPath(vg);
				nvgRoundedRect(vg, x0, yy - 2.f, wv < 0.5f ? 0.5f : wv, 4.f, 2.f);
				nvgFillColor(vg, col);
				nvgFill(vg);
			};
			auto value = [&](float yy, const char* txt) {
				nvgFillColor(vg, nvgRGB(0xc8, 0xd2, 0xdc));
				nvgText(vg, x + 52.f, yy, txt, NULL);
			};

			char buf[24];
			label(y, "F0");
			if (s->f0 > 1.f) {
				std::snprintf(buf, sizeof(buf), "%.0f Hz", s->f0);
				value(y, buf);
			} else {
				value(y, " --");
			}
			y += 14.f;
			label(y, "VOI"); bar(y, s->voicing, nvgRGB(0x7e, 0xe0, 0xa8));
			y += 13.f;
			label(y, "TEN"); bar(y, s->tension, nvgRGB(0xc7, 0x9f, 0xfb));
			y += 13.f;
			label(y, "PRS"); bar(y, s->press, nvgRGB(0xff, 0x88, 0x64));
			y += 13.f;
			label(y, "VEL"); bar(y, s->velum, nvgRGB(0x48, 0xd8, 0xf8));
			y += 13.f;
			label(y, "LNG"); bar(y, s->lungsVol, nvgRGB(0x48, 0xd8, 0xf8));
			y += 13.f;
			label(y, "FLW"); bar(y, std::fabs(s->lungsFlow),
				(s->lungsFlow >= 0.f) ? nvgRGB(0x48, 0xe0, 0xff)
					: nvgRGB(0xff, 0x6a, 0x74), true);
			y += 13.f;
			label(y, "FRC"); bar(y, s->fricative, nvgRGB(0xff, 0xc4, 0x6b));
			y += 15.f;
			std::snprintf(buf, sizeof(buf), "TNG %.1f/%.1f", s->tongueX, s->tongueY);
			nvgFillColor(vg, nvgRGB(0xc8, 0xd2, 0xdc));
			nvgText(vg, x, y, buf, NULL);
			y += 12.f;
			if (s->constrPos > 0.01f) {
				std::snprintf(buf, sizeof(buf), "CNS %.1f/%.1f",
					s->constrPos, s->constrWidth);
			} else {
				std::snprintf(buf, sizeof(buf), "CNS --");
			}
			nvgText(vg, x, y, buf, NULL);
			y += 12.f;
			std::snprintf(buf, sizeof(buf), "SND %d%%",
				(int) (std::fmin(s->lipEnv * 2.2f, 1.f) * 100.f));
			nvgText(vg, x, y, buf, NULL);
		}
	};

	// ================= panel =================
	static void drawPanel(const rack::widget::Widget::DrawArgs& args, float panelW) {
		NVGcontext* vg = args.vg;
		NVGcolor bg = nvgRGB(0x12, 0x15, 0x1a);
		NVGcolor edge = nvgRGB(0x28, 0x30, 0x3a);
		NVGcolor accent = nvgRGB(0x48, 0xd8, 0xf8);

		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, panelW, rack::app::RACK_GRID_HEIGHT);
		nvgFillColor(vg, bg);
		nvgFill(vg);
		nvgStrokeWidth(vg, 1.f);
		nvgStrokeColor(vg, edge);
		nvgStroke(vg);

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

		// corner tag + labels
		auto font = getLabelFont();
		if (font && font->handle >= 0) {
			nvgFontFaceId(vg, font->handle);
			nvgFontSize(vg, 6.f);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
			nvgFillColor(vg, nvgRGBA(0x58, 0x98, 0xb0, 150));
			nvgText(vg, 10.f, 12.f, "SAGITTAL FLUORO", NULL);

			// jack labels
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
			static const char* jn[6] = {"TRACT", "LARYNX", "PRES",
				"VOL", "FLOW", "VOCD"};
			for (int i = 0; i < 6; i++) {
				nvgFillColor(vg, nvgRGB(0x8a, 0x96, 0xa2));
				nvgText(vg, 34.f + i * 60.f, 322.f, jn[i], NULL);
			}

			// knob / switch labels
			nvgFontSize(vg, 6.f);
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
			nvgFillColor(vg, nvgRGB(0x8a, 0x96, 0xa2));
			nvgText(vg, 344.f, 22.f, "GAIN", NULL);
			nvgText(vg, 344.f, 108.f, "CV", NULL);
		}

		drawPanelTitle(args, panelW, "SAGITTA");
	}

	CyberlineSagittaWidget(CyberlineSagitta* module) {
		setModule(module);
		box.size = rack::Vec(PANEL_W, rack::app::RACK_GRID_HEIGHT);

		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, rack::app::RACK_GRID_HEIGHT - 15)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, rack::app::RACK_GRID_HEIGHT - 15)));

		using rack::componentlibrary::RoundSmallBlackKnob;
		using rack::componentlibrary::PJ301MPort;

		// anatomy view first (params/jacks are added after so they draw
		// above it and receive mouse events)
		auto view = new FluoroView();
		view->w = this;
		view->box.pos = rack::Vec(0.f, 0.f);
		view->box.size = box.size;
		addChild(view);

		// params, right column
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(
			rack::Vec(344.f, 40.f), module, CyberlineSagitta::GAIN_PARAM));
		addParam(rack::createParamCentered<ModeSwitch>(
			rack::Vec(344.f, 88.f), module, CyberlineSagitta::MODE_PARAM));

		// input row
		float row = 340.f;
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(34.f, row), module, CyberlineSagitta::TRACT_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(94.f, row), module, CyberlineSagitta::LARYNX_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(154.f, row), module, CyberlineSagitta::LUNGS_P_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(214.f, row), module, CyberlineSagitta::LUNGS_V_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(274.f, row), module, CyberlineSagitta::LUNGS_F_INPUT));
		addInput(rack::createInputCentered<PJ301MPort>(rack::Vec(334.f, row), module, CyberlineSagitta::VOICED_INPUT));
	}
};

} // anonymous namespace

} // namespace cyberline

namespace cyberline {
rack::plugin::Model* modelCyberlineSagitta =
	rack::createModel<CyberlineSagitta, CyberlineSagittaWidget>("cyberline-sagitta");
} // namespace cyberline
