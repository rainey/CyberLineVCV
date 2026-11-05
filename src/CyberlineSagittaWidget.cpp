// CyberlineSagittaWidget — programmatic panel (no SVG assets). 26 HP.
// Vocal-system fluoroscopy: sagittal anatomical display of the
// lungs -> larynx -> vocal tract pipeline in the suite's cyberpunk
// neon fluoroscopy aesthetic, driven by live telemetry snapshot:
//
//   - Fluoroscopy CRT viewport with phosphor raster & corner reticles
//   - Anatomical cranium, facial profile, cervical vertebrae (C1-C6)
//   - Kelly-Lochbaum waveguide replica tongue profile, palate & velum
//   - Laryngeal thyroid/cricoid cartilage & vibrating vocal cords
//   - Trachea with cartilage rings, carina bifurcation & bronchial stems
//   - Anatomical lung lobes & dynamic diaphragm dome
//   - Airflow ionization particles & lip radiation wavefronts
//   - Telemetry sidebar with digital bar gauges & parameter readouts

#include "CyberlineSagitta.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>
#include <chrono>
#include <cmath>
#include <cstdio>

namespace cyberline {

// ---- polyline toolkit ----
struct SPt { float x, y; };

static float polyLen(const SPt* p, int n) {
	float L = 0.f;
	for (int i = 1; i < n; i++) {
		float dx = p[i].x - p[i - 1].x, dy = p[i].y - p[i - 1].y;
		L += std::sqrt(dx * dx + dy * dy);
	}
	return L;
}

static float polyVertexFrac(const SPt* p, int n, int idx) {
	if (idx <= 0) return 0.f;
	if (idx >= n - 1) return 1.f;
	return polyLen(p, idx + 1) / polyLen(p, n);
}

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

static SPt polyTangent(const SPt* p, int n, float s) {
	float e = 0.01f;
	SPt a = polyAt(p, n, s - e < 0.f ? 0.f : s - e);
	SPt b = polyAt(p, n, s + e > 1.f ? 1.f : s + e);
	float dx = b.x - a.x, dy = b.y - a.y;
	float L = std::sqrt(dx * dx + dy * dy);
	if (L < 1e-5f) return {1.f, 0.f};
	return {dx / L, dy / L};
}

static SPt polyNormal(const SPt* p, int n, float s) {
	SPt t = polyTangent(p, n, s);
	return {t.y, -t.x};
}

// ---- anatomical control paths (panel pixel space) ----

// Oral tract centerline: glottis (t=0) -> lips (t=1). Vertex 6 is velopharynx bend.
static const SPt TRACT_CP[] = {
	{174, 226}, {170, 208}, {164, 186}, {158, 164}, {154, 144},
	{152, 130}, {154, 118}, {166, 112}, {184, 108}, {204, 107},
	{224, 109}, {238, 113}, {244, 115},
};
static const int TRACT_NCP = (int) (sizeof(TRACT_CP) / sizeof(SPt));
static const int TRACT_BEND_VTX = 6;

// Nasal branch: velum junction (= bend vertex) -> nostril exit.
static const SPt NOSE_CP[] = {
	{154, 118}, {158, 102}, {170, 90}, {192, 82}, {214, 78},
	{230, 80}, {240, 86}, {246, 92},
};
static const int NOSE_NCP = (int) (sizeof(NOSE_CP) / sizeof(SPt));

// Trachea: cricoid cartilage -> carina bifurcation (drawn downward).
static const SPt TRACHEA_CP[] = {
	{174, 237}, {170, 250}, {164, 262},
};
static const int TRACHEA_NCP = (int) (sizeof(TRACHEA_CP) / sizeof(SPt));

// Airflow particle routes: lungs -> trachea -> out oral/nasal
static const SPt AIR_ORAL[] = {
	{120, 280}, {140, 272}, {164, 262}, {170, 250}, {174, 237},
	{174, 226}, {170, 208}, {164, 186}, {158, 164}, {154, 144},
	{152, 130}, {154, 118}, {166, 112}, {184, 108}, {204, 107},
	{224, 109}, {238, 113}, {244, 115}, {260, 115},
};
static const int AIR_ORAL_N = (int) (sizeof(AIR_ORAL) / sizeof(SPt));

static const SPt AIR_NOSE[] = {
	{120, 280}, {140, 272}, {164, 262}, {170, 250}, {174, 237},
	{174, 226}, {170, 208}, {164, 186}, {158, 164}, {154, 144},
	{152, 130}, {154, 118}, {158, 102}, {170, 90}, {192, 82},
	{214, 78}, {230, 80}, {240, 86}, {246, 92}, {258, 92},
};
static const int AIR_NOSE_N = (int) (sizeof(AIR_NOSE) / sizeof(SPt));

// Anatomically proportioned facial profile & cranium (sagittal fluoroscopy)
static const SPt HEAD_CP[] = {
	// Posterior torso / neck
	{48, 282}, {48, 250}, {44, 210}, {42, 160}, {46, 114},
	// Calvaria / skull
	{60, 72}, {92, 42}, {136, 28}, {180, 32}, {212, 48},
	// Facial profile
	{226, 66},  // glabella / brow
	{232, 78},  // nasion
	{246, 92},  // tip of nose
	{236, 100}, // columella / subnasale
	{242, 108}, // upper lip / labrale superius
	{244, 115}, // oral fissure / mouth
	{240, 122}, // lower lip / labrale inferius
	{232, 134}, // mentolabial sulcus
	{224, 148}, // chin / pogonion
	{204, 168}, // submental
	{196, 188}, // suprahyoid
	{206, 216}, // thyroid prominence (Adam's apple)
	{198, 234}, // cricoid level
	{190, 252}, // suprasternal notch
	{186, 282}, // anterior chest wall
};
static const int HEAD_NCP = (int) (sizeof(HEAD_CP) / sizeof(SPt));

// Mandible / jawbone outline
static const SPt MANDIBLE_CP[] = {
	{222, 146}, {196, 156}, {166, 156}, {158, 140}, {162, 122},
};
static const int MANDIBLE_NCP = (int) (sizeof(MANDIBLE_CP) / sizeof(SPt));

namespace {

struct CyberlineSagittaWidget;

// Browser-preview core: neutral anatomy when unpatched
static CyberlineSagittaCore& browserCore() {
	static CyberlineSagittaCore core;
	return core;
}

struct CyberlineSagittaWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 26 * 15.f; // 390 px (26 HP)

	struct Particle {
		float s = 0.f;
		float spd = 0.25f;
		int route = 0; // 0 oral, 1 nasal
		bool alive = false;
	};
	static constexpr int NP = 48;
	Particle parts[NP];
	float spawnAcc = 0.f;
	std::chrono::steady_clock::time_point lastDraw;
	bool lastDrawInit = false;

	// 2-position snap switch (BUS / DIRECT)
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
				pq->setValue(std::round(oldValue) == 0.f ? 1.f : 0.f);
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
			nvgFillColor(vg, nvgRGB(0x08, 0x0c, 0x12));
			nvgFill(vg);
			nvgStrokeWidth(vg, 1.f);
			nvgStrokeColor(vg, nvgRGB(0x28, 0x36, 0x48));
			nvgStroke(vg);

			// handle
			float hy = 4.f + (1.f - v) * (h - 12.f);
			nvgBeginPath(vg);
			nvgCircle(vg, w * 0.5f, hy + 3.f, 4.6f);
			nvgFillColor(vg, nvgRGB(0xd0, 0xd8, 0xe0));
			nvgFill(vg);
			nvgStrokeColor(vg, nvgRGB(0x48, 0xd8, 0xf8));
			nvgStrokeWidth(vg, 1.2f);
			nvgStroke(vg);
		}
	};

	// ================= fluoroscopy anatomy view =================
	struct FluoroView : rack::widget::Widget {
		CyberlineSagittaWidget* w = nullptr;
		float bendFrac = 0.6f;

		void begin() {
			bendFrac = polyVertexFrac(TRACT_CP, TRACT_NCP, TRACT_BEND_VTX);
		}

		float arcF(int i, int n, int noseStart) const {
			if (noseStart <= 0 || noseStart >= n - 1)
				return 0.96f * (float) i / (float) (n - 1);
			if (i <= noseStart)
				return 0.96f * bendFrac * (float) i / (float) noseStart;
			return 0.96f * (bendFrac + (1.f - bendFrac)
				* (float) (i - noseStart) / (float) (n - 1 - noseStart));
		}

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
			CyberlineSagitta* module = dynamic_cast<CyberlineSagitta*>(w->getModule());
			const CyberlineSagittaCore::Snapshot* s = module
				? module->core.snapshot()
				: browserCore().snapshot();
			if (!s || !s->alive)
				return;
			begin();

			auto now = std::chrono::steady_clock::now();
			float dt = 1.f / 60.f;
			if (w->lastDrawInit) {
				dt = std::chrono::duration<float>(now - w->lastDraw).count();
				dt = dt < 0.f ? 0.f : (dt > 0.1f ? 0.1f : dt);
			}
			w->lastDraw = now;
			w->lastDrawInit = true;

			const float EX = 11.5f; // px per cm
			float sx = 12.f, sy = 14.f, sw = 282.f, sh = 288.f;

			drawScreenBackdrop(vg, sx, sy, sw, sh);

			nvgSave(vg);
			nvgScissor(vg, sx, sy, sw, sh);
			drawBones(vg);
			drawHeadSoftTissue(vg);
			drawLungs(vg, s);
			drawTrachea(vg);
			drawOralCavity(vg, s, EX);
			drawNasalBranch(vg, s);
			drawLarynx(vg, s);
			drawProfileStroke(vg);
			drawAirParticles(vg, s, dt);
			drawSoundRadiation(vg, s);
			nvgResetScissor(vg);
			nvgRestore(vg);

			drawHud(vg, s);
		}

		// CRT viewport glass & medical fluoroscopy grid
		void drawScreenBackdrop(NVGcontext* vg, float sx, float sy, float sw, float sh) {
			// Dark CRT phosphor glass
			nvgBeginPath(vg);
			nvgRoundedRect(vg, sx, sy, sw, sh, 6.f);
			NVGpaint crtGrad = nvgLinearGradient(vg, sx, sy, sx + sw, sy + sh,
				nvgRGB(0x06, 0x09, 0x0e), nvgRGB(0x0a, 0x0e, 0x16));
			nvgFillPaint(vg, crtGrad);
			nvgFill(vg);

			// Fluoroscopy coordinate raster grid
			nvgBeginPath(vg);
			for (float gx = sx + 28.f; gx < sx + sw - 6.f; gx += 28.f) {
				nvgMoveTo(vg, gx, sy + 4.f);
				nvgLineTo(vg, gx, sy + sh - 4.f);
			}
			for (float gy = sy + 28.f; gy < sy + sh - 6.f; gy += 28.f) {
				nvgMoveTo(vg, sx + 4.f, gy);
				nvgLineTo(vg, sx + sw - 4.f, gy);
			}
			nvgStrokeWidth(vg, 0.5f);
			nvgStrokeColor(vg, nvgRGBA(0x24, 0x44, 0x5a, 24));
			nvgStroke(vg);

			// Corner registration reticles
			auto drawReticle = [&](float rx, float ry, float dx, float dy) {
				nvgBeginPath(vg);
				nvgMoveTo(vg, rx + dx * 8.f, ry);
				nvgLineTo(vg, rx, ry);
				nvgLineTo(vg, rx, ry + dy * 8.f);
				nvgStrokeWidth(vg, 1.f);
				nvgStrokeColor(vg, nvgRGBA(0x48, 0xd8, 0xf8, 70));
				nvgStroke(vg);
			};
			drawReticle(sx + 5.f, sy + 5.f, 1.f, 1.f);
			drawReticle(sx + sw - 5.f, sy + 5.f, -1.f, 1.f);
			drawReticle(sx + 5.f, sy + sh - 5.f, 1.f, -1.f);
			drawReticle(sx + sw - 5.f, sy + sh - 5.f, -1.f, -1.f);

			// Double neon CRT bezel
			nvgBeginPath(vg);
			nvgRoundedRect(vg, sx, sy, sw, sh, 6.f);
			strokeNeon(vg, nvgRGBA(0x38, 0x88, 0xb8, 120), 1.0f, 2.6f, 25);

			// Screen header metadata
			auto font = getLabelFont();
			if (font && font->handle >= 0) {
				nvgFontFaceId(vg, font->handle);
				nvgFontSize(vg, 6.2f);
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
				nvgFillColor(vg, nvgRGBA(0x48, 0xd8, 0xf8, 160));
				nvgText(vg, sx + 10.f, sy + 8.f, "FLUOROSCOPY // SAGITTAL X-RAY", NULL);
				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
				nvgFillColor(vg, nvgRGBA(0x7e, 0xe0, 0xa8, 140));
				nvgText(vg, sx + sw - 10.f, sy + 8.f, "17.5cm VTL REPLICA", NULL);
			}
		}

		// Anatomical skull & cervical vertebrae (fluoroscopy bone density)
		void drawBones(NVGcontext* vg) {
			// Cranial vault bone glow
			nvgBeginPath(vg);
			nvgMoveTo(vg, 60, 160);
			nvgBezierTo(vg, 58, 96, 88, 52, 136, 40);
			nvgBezierTo(vg, 178, 44, 204, 56, 212, 68);
			nvgStrokeWidth(vg, 1.8f);
			nvgStrokeColor(vg, nvgRGBA(0x3a, 0x68, 0x86, 95));
			nvgStroke(vg);

			// Anatomical orbit socket (bony orbit contour)
			nvgBeginPath(vg);
			nvgEllipse(vg, 206, 74, 10.f, 7.5f);
			nvgStrokeWidth(vg, 1.1f);
			nvgStrokeColor(vg, nvgRGBA(0x3a, 0x68, 0x86, 110));
			nvgStroke(vg);

			// Hard palate bone shelf (maxilla - roof of mouth)
			nvgBeginPath(vg);
			nvgMoveTo(vg, 160, 98);
			nvgQuadTo(vg, 180, 90, 210, 88);
			nvgLineTo(vg, 234, 90);
			nvgStrokeWidth(vg, 1.8f);
			nvgStrokeColor(vg, nvgRGBA(0x60, 0xa0, 0xc8, 130));
			nvgStroke(vg);

			// Mandible (jawbone)
			nvgBeginPath(vg);
			nvgMoveTo(vg, MANDIBLE_CP[0].x, MANDIBLE_CP[0].y);
			for (int i = 1; i < MANDIBLE_NCP; i++)
				nvgLineTo(vg, MANDIBLE_CP[i].x, MANDIBLE_CP[i].y);
			nvgStrokeWidth(vg, 1.2f);
			nvgStrokeColor(vg, nvgRGBA(0x3a, 0x68, 0x86, 95));
			nvgStroke(vg);

			// Cervical spine vertebrae column (C1-C7)
			for (int c = 0; c < 7; c++) {
				float vy = 140.f + c * 15.f;
				float vx = 110.f + (c * 1.6f);
				nvgBeginPath(vg);
				nvgRoundedRect(vg, vx, vy, 13.5f, 10.f, 2.f);
				nvgFillColor(vg, nvgRGBA(0x1a, 0x30, 0x44, 90));
				nvgFill(vg);
				nvgStrokeWidth(vg, 1.0f);
				nvgStrokeColor(vg, nvgRGBA(0x42, 0x76, 0x9a, 125));
				nvgStroke(vg);
			}

			// Clavicle / first rib arch hint
			nvgBeginPath(vg);
			nvgMoveTo(vg, 116, 254);
			nvgQuadTo(vg, 152, 256, 188, 252);
			nvgStrokeWidth(vg, 1.2f);
			nvgStrokeColor(vg, nvgRGBA(0x3a, 0x68, 0x86, 90));
			nvgStroke(vg);
		}

		// Soft tissue facial profile
		void drawHeadSoftTissue(NVGcontext* vg) {
			nvgBeginPath(vg);
			nvgMoveTo(vg, HEAD_CP[0].x, HEAD_CP[0].y);
			for (int i = 1; i < HEAD_NCP; i++)
				nvgLineTo(vg, HEAD_CP[i].x, HEAD_CP[i].y);
			nvgClosePath(vg);
			nvgFillColor(vg, nvgRGBA(0x0e, 0x16, 0x22, 175));
			nvgFill(vg);
		}

		void drawProfileStroke(NVGcontext* vg) {
			nvgBeginPath(vg);
			nvgMoveTo(vg, HEAD_CP[1].x, HEAD_CP[1].y);
			for (int i = 2; i < HEAD_NCP - 1; i++)
				nvgLineTo(vg, HEAD_CP[i].x, HEAD_CP[i].y);
			strokeNeon(vg, nvgRGBA(0x3e, 0xa6, 0xd0, 160), 1.1f, 2.8f, 22);
		}

		// Contoured lungs + dynamic diaphragm dome
		void drawLungs(NVGcontext* vg, const CyberlineSagittaCore::Snapshot* s) {
			float vol = s->lungsVol;

			// Left lung lobe (posterior)
			nvgBeginPath(vg);
			nvgMoveTo(vg, 108, 256);
			nvgBezierTo(vg, 90, 260, 78, 272, 80, 286);
			nvgQuadTo(vg, 110, 290 - 3.f * vol, 142, 284);
			nvgBezierTo(vg, 134, 272, 122, 260, 108, 256);
			nvgFillColor(vg, nvgRGBA(0x28, 0x78, 0x9e, (int) (35 + 45 * vol)));
			nvgFill(vg);
			nvgStrokeWidth(vg, 1.2f);
			nvgStrokeColor(vg, nvgRGBA(0x48, 0xd8, 0xf8, (int) (100 + 80 * vol)));
			nvgStroke(vg);

			// Right lung lobe (anterior)
			nvgBeginPath(vg);
			nvgMoveTo(vg, 154, 258);
			nvgBezierTo(vg, 170, 262, 184, 272, 182, 286);
			nvgQuadTo(vg, 164, 291 - 4.f * vol, 144, 285);
			nvgBezierTo(vg, 146, 273, 149, 263, 154, 258);
			nvgFillColor(vg, nvgRGBA(0x28, 0x78, 0x9e, (int) (40 + 50 * vol)));
			nvgFill(vg);
			nvgStrokeWidth(vg, 1.2f);
			nvgStrokeColor(vg, nvgRGBA(0x48, 0xd8, 0xf8, (int) (110 + 90 * vol)));
			nvgStroke(vg);

			// Rib cage arch hints
			for (int r = 0; r < 3; r++) {
				float ry = 266.f + r * 7.5f;
				nvgBeginPath(vg);
				nvgMoveTo(vg, 76, ry);
				nvgQuadTo(vg, 130, ry + 2.5f, 184, ry);
				nvgStrokeWidth(vg, 0.9f);
				nvgStrokeColor(vg, nvgRGBA(0x32, 0x58, 0x74, 75));
				nvgStroke(vg);
			}

			// Diaphragm muscular dome
			float dip = 287.f + 5.f * vol;
			nvgBeginPath(vg);
			nvgMoveTo(vg, 76, 287);
			nvgQuadTo(vg, 131, dip, 186, 287);
			strokeNeon(vg, nvgRGBA(0x7e, 0xe0, 0xa8, 160), 1.2f, 2.8f, 30);
		}

		// Trachea with cartilage rings & carina
		void drawTrachea(NVGcontext* vg) {
			// Main tube
			nvgBeginPath(vg);
			nvgMoveTo(vg, TRACHEA_CP[0].x, TRACHEA_CP[0].y);
			for (int i = 1; i < TRACHEA_NCP; i++)
				nvgLineTo(vg, TRACHEA_CP[i].x, TRACHEA_CP[i].y);
			nvgStrokeWidth(vg, 6.0f);
			nvgStrokeColor(vg, nvgRGBA(0x08, 0x12, 0x1a, 240));
			nvgLineCap(vg, NVG_ROUND);
			nvgStroke(vg);

			nvgBeginPath(vg);
			nvgMoveTo(vg, TRACHEA_CP[0].x, TRACHEA_CP[0].y);
			for (int i = 1; i < TRACHEA_NCP; i++)
				nvgLineTo(vg, TRACHEA_CP[i].x, TRACHEA_CP[i].y);
			nvgStrokeWidth(vg, 1.1f);
			nvgStrokeColor(vg, nvgRGBA(0x50, 0xa0, 0xc4, 180));
			nvgStroke(vg);

			// Cartilage rings
			for (float f = 0.2f; f < 0.95f; f += 0.28f) {
				SPt p = polyAt(TRACHEA_CP, TRACHEA_NCP, f);
				SPt nrm = polyNormal(TRACHEA_CP, TRACHEA_NCP, f);
				nvgBeginPath(vg);
				nvgMoveTo(vg, p.x - nrm.x * 3.2f, p.y - nrm.y * 3.2f);
				nvgLineTo(vg, p.x + nrm.x * 3.2f, p.y + nrm.y * 3.2f);
				nvgStrokeWidth(vg, 1.1f);
				nvgStrokeColor(vg, nvgRGBA(0x70, 0xc0, 0xe0, 140));
				nvgStroke(vg);
			}

			// Carina bifurcation into bronchial branches
			nvgBeginPath(vg);
			nvgMoveTo(vg, 164, 262);
			nvgLineTo(vg, 138, 274);
			nvgMoveTo(vg, 164, 262);
			nvgLineTo(vg, 174, 274);
			nvgStrokeWidth(vg, 1.2f);
			nvgStrokeColor(vg, nvgRGBA(0x50, 0xa0, 0xc4, 160));
			nvgStroke(vg);
		}

		// Nasal branch (shaped by velum)
		void drawNasalBranch(NVGcontext* vg, const CyberlineSagittaCore::Snapshot* s) {
			const float EXN = 8.5f;
			int nl = s->noseLength;
			if (nl < 2) return;

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

			// Cavity fill
			nvgBeginPath(vg);
			nvgMoveTo(vg, up[0].x, up[0].y);
			for (int i = 1; i < nl; i++) nvgLineTo(vg, up[i].x, up[i].y);
			for (int i = nl - 1; i >= 0; i--) nvgLineTo(vg, dn[i].x, dn[i].y);
			nvgClosePath(vg);
			nvgFillColor(vg, nvgRGBA(0x06, 0x09, 0x0e, 230));
			nvgFill(vg);

			// Nasal cavity walls
			nvgBeginPath(vg);
			nvgMoveTo(vg, up[0].x, up[0].y);
			for (int i = 1; i < nl; i++) nvgLineTo(vg, up[i].x, up[i].y);
			nvgStrokeWidth(vg, 1.1f);
			nvgStrokeColor(vg, nvgRGBA(0x82, 0xb8, 0xd4, 160));
			nvgStroke(vg);

			nvgBeginPath(vg);
			nvgMoveTo(vg, dn[0].x, dn[0].y);
			for (int i = 1; i < nl; i++) nvgLineTo(vg, dn[i].x, dn[i].y);
			nvgStroke(vg);

			// Nasal sound pressure glow
			for (int i = 0; i < nl; i++) {
				float a = std::fmin(s->noseAmp[i] * 3.4f, 0.85f);
				if (a < 0.02f) continue;
				float f = (float) i / (float) (nl - 1);
				SPt c = polyAt(NOSE_CP, NOSE_NCP, f);
				nvgBeginPath(vg);
				nvgCircle(vg, c.x, c.y, 2.5f + 5.5f * a);
				nvgFillColor(vg, nvgRGBA(0xb0, 0xe8, 0xff, (int) (a * 240)));
				nvgFill(vg);
			}

			// Velum muscular flap: closed seals against pharynx, open swings down
			float vel = s->velum;
			SPt j = NOSE_CP[0];
			float hang = 4.f + 11.f * vel;
			float ang = -2.3f + 1.6f * vel;
			SPt tip = {j.x + std::cos(ang) * hang, j.y - std::sin(ang) * hang};
			nvgBeginPath(vg);
			nvgMoveTo(vg, j.x, j.y);
			nvgLineTo(vg, tip.x, tip.y);
			strokeNeon(vg, nvgRGBA(0xc7, 0x9f, 0xfb, 200), 1.5f, 3.2f, 40);
		}

		// Oral tract: Kelly-Lochbaum waveguide replica tongue profile
		void drawOralCavity(NVGcontext* vg, const CyberlineSagittaCore::Snapshot* s, float EX) {
			int n = s->n;
			if (n < 8) return;
			static const float PHARYNX_WALL = 12.f;
			static const float PALATE = 20.f;
			static const float FLOORH = 26.f;

			SPt cl[TRACT_SECTIONS_MAX];
			SPt nrm[TRACT_SECTIONS_MAX];
			SPt up[TRACT_SECTIONS_MAX];
			SPt tg[TRACT_SECTIONS_MAX];

			for (int i = 0; i < n; i++) {
				float f = arcF(i, n, s->noseStart);
				cl[i] = polyAt(TRACT_CP, TRACT_NCP, f);
				nrm[i] = polyNormal(TRACT_CP, TRACT_NCP, f);

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

			// Tongue body muscular tissue fill
			nvgBeginPath(vg);
			nvgMoveTo(vg, tg[0].x, tg[0].y);
			for (int i = 1; i < n; i++) nvgLineTo(vg, tg[i].x, tg[i].y);
			for (int i = n - 1; i >= 0; i--) {
				SPt fl = {cl[i].x - nrm[i].x * FLOORH, cl[i].y - nrm[i].y * FLOORH};
				nvgLineTo(vg, fl.x, fl.y);
			}
			nvgClosePath(vg);
			nvgFillColor(vg, nvgRGBA(0xb0, 0x3e, 0x56, 75));
			nvgFill(vg);

			// Tongue surface dorsal profile line
			nvgBeginPath(vg);
			nvgMoveTo(vg, tg[0].x, tg[0].y);
			for (int i = 1; i < n; i++) nvgLineTo(vg, tg[i].x, tg[i].y);
			strokeNeon(vg, nvgRGBA(0xff, 0x62, 0x84, 220), 1.35f, 3.4f, 35);

			// Cavity air lumen fill (dark vacuum)
			nvgBeginPath(vg);
			nvgMoveTo(vg, up[0].x, up[0].y);
			for (int i = 1; i < n; i++) nvgLineTo(vg, up[i].x, up[i].y);
			for (int i = n - 1; i >= 0; i--) nvgLineTo(vg, tg[i].x, tg[i].y);
			nvgClosePath(vg);
			nvgFillColor(vg, nvgRGBA(0x06, 0x08, 0x0d, 235));
			nvgFill(vg);

			// Palate / posterior pharynx contour
			nvgBeginPath(vg);
			nvgMoveTo(vg, up[0].x, up[0].y);
			for (int i = 1; i < n; i++) nvgLineTo(vg, up[i].x, up[i].y);
			nvgStrokeWidth(vg, 1.3f);
			nvgStrokeColor(vg, nvgRGBA(0xc8, 0xd4, 0xe0, 180));
			nvgStroke(vg);

			// Waveguide standing wave plasma glow
			{
				nvgBeginPath(vg);
				nvgMoveTo(vg, cl[0].x, cl[0].y);
				for (int i = 1; i < n; i++) nvgLineTo(vg, cl[i].x, cl[i].y);
				nvgStrokeWidth(vg, 6.f);
				nvgLineCap(vg, NVG_ROUND);
				nvgLineJoin(vg, NVG_ROUND);
				nvgStrokeColor(vg, nvgRGBA(0xff, 0xd0, 0x68, 32));
				nvgStroke(vg);

				for (int i = 0; i < n; i++) {
					float a = std::fmin(s->amp[i] * 3.5f, 0.9f);
					if (a < 0.04f) continue;
					nvgBeginPath(vg);
					nvgCircle(vg, cl[i].x, cl[i].y, 1.6f + 4.8f * a);
					nvgFillColor(vg, nvgRGBA(0xff, 0xe8, 0xb0, (int) (a * 200)));
					nvgFill(vg);
				}
			}

			// Constriction targeting reticle & turbulence sparks
			float minD = 9.f;
			int minI = -1;
			for (int i = s->bladeStart; i < n; i++) {
				if (s->diam[i] < minD) {
					minD = s->diam[i];
					minI = i;
				}
			}
			if (minI >= 0 && minD < 1.0f && s->constrPos > 0.01f) {
				SPt c = cl[minI];
				nvgBeginPath(vg);
				nvgCircle(vg, c.x, c.y, 3.5f + 2.f * minD);
				nvgStrokeWidth(vg, 1.3f);
				nvgStrokeColor(vg, nvgRGBA(0xff, 0xc4, 0x6b, 200));
				nvgStroke(vg);

				if (s->fricative > 0.12f) {
					float tsec = nowSec();
					for (int k = 0; k < 6; k++) {
						float hx = std::fmod(0.7371f * (k * 13 + minI * 7) + tsec * 3.f, 1.f);
						float hy = std::fmod(0.5219f * (k * 29 + minI * 3) + tsec * 5.f, 1.f);
						float ox = (hx - 0.5f) * 12.f;
						float oy = (hy - 0.5f) * 12.f;
						nvgBeginPath(vg);
						nvgCircle(vg, c.x + ox, c.y + oy, 0.9f);
						nvgFillColor(vg, nvgRGBA(0xff, 0xe8, 0xaa, (int) (115.f * s->fricative)));
						nvgFill(vg);
					}
				}
			}
		}

		// Larynx: thyroid cartilage wireframe + vocal folds
		void drawLarynx(NVGcontext* vg, const CyberlineSagittaCore::Snapshot* s) {
			float lx = 158.f, ly = 215.f, lw = 36.f, lh = 22.f;

			// Laryngeal thyroid / cricoid cartilage wireframe
			nvgBeginPath(vg);
			nvgRoundedRect(vg, lx, ly, lw, lh, 3.5f);
			nvgFillColor(vg, nvgRGBA(0x0c, 0x10, 0x18, 220));
			nvgFill(vg);
			nvgStrokeWidth(vg, 1.2f);
			nvgStrokeColor(vg, nvgRGBA(0xc7, 0x9f, 0xfb, 150));
			nvgStroke(vg);

			// Subglottal driving pressure glow
			float glow = std::fmin(s->press * (0.35f + 0.65f * s->voicedEnv), 1.f);
			if (glow > 0.02f) {
				NVGpaint g = nvgRadialGradient(vg, lx + lw * 0.5f, ly + lh - 3.f, 2.f, 12.f,
					nvgRGBA(0xff, 0x88, 0x64, (int) (glow * 140)),
					nvgRGBA(0xff, 0x88, 0x64, 0));
				nvgBeginPath(vg);
				nvgRect(vg, lx + 2.f, ly + lh * 0.5f, lw - 4.f, lh * 0.5f);
				nvgFillPaint(vg, g);
				nvgFill(vg);
			}

			// Vocal fold color: slack cyan -> tense magenta
			float tn = s->tension;
			NVGcolor fold = nvgRGBA(
				(int) (0x48 + tn * (0xff - 0x48)),
				(int) (0xd8 - tn * (0xd8 - 0x58)),
				(int) (0xf8 - tn * (0xf8 - 0xc8)), 240);

			// Vocal fold mucosal wave flutter
			float wob = std::fabs(std::sin(2.f * (float) M_PI * s->foldPhase));
			float gap = 4.2f + ((0.7f + 1.5f * wob) - 4.2f) * s->voicing;
			float cy = ly + lh * 0.5f;

			// Anterior fold
			nvgBeginPath(vg);
			nvgMoveTo(vg, lx + 5.f, cy - gap * 0.5f);
			nvgLineTo(vg, lx + lw - 5.f, cy - gap * 0.5f);
			nvgLineTo(vg, lx + lw - 8.f, cy - gap * 0.5f - 3.8f);
			nvgLineTo(vg, lx + 8.f, cy - gap * 0.5f - 3.8f);
			nvgClosePath(vg);
			nvgFillColor(vg, fold);
			nvgFill(vg);

			// Posterior fold
			nvgBeginPath(vg);
			nvgMoveTo(vg, lx + 5.f, cy + gap * 0.5f);
			nvgLineTo(vg, lx + lw - 5.f, cy + gap * 0.5f);
			nvgLineTo(vg, lx + lw - 8.f, cy + gap * 0.5f + 3.8f);
			nvgLineTo(vg, lx + 8.f, cy + gap * 0.5f + 3.8f);
			nvgClosePath(vg);
			nvgFillColor(vg, fold);
			nvgFill(vg);
		}

		// Airflow particles (cyan inhale, coral exhale)
		void drawAirParticles(NVGcontext* vg, const CyberlineSagittaCore::Snapshot* s, float dt) {
			float flow = s->lungsFlow;
			bool exhale = flow < -0.04f;
			bool inhale = flow > 0.04f;
			float rate = 16.f * std::fmin(std::fabs(flow), 1.f);
			bool nasalOpen = s->velum > 0.25f;

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

				SPt pos = (p.route == 1)
					? polyAt(AIR_NOSE, AIR_NOSE_N, p.s)
					: polyAt(AIR_ORAL, AIR_ORAL_N, p.s);

				NVGcolor c = (p.spd > 0.f)
					? nvgRGBA(0xff, 0x7a, 0x6b, 200)
					: nvgRGBA(0x48, 0xd8, 0xf8, 200);

				nvgBeginPath(vg);
				nvgCircle(vg, pos.x, pos.y, 1.4f);
				nvgFillColor(vg, c);
				nvgFill(vg);
			}
		}

		// Sound radiation wavefronts at lips
		void drawSoundRadiation(NVGcontext* vg, const CyberlineSagittaCore::Snapshot* s) {
			float la = std::fmin(s->lipEnv * 2.4f, 1.f);
			for (int k = 0; k < 3; k++) {
				float a = la - 0.28f * k;
				if (a <= 0.02f) break;
				nvgBeginPath(vg);
				nvgArc(vg, 248, 116, 6.f + 5.5f * k, -0.6f, 0.6f, NVG_CW);
				nvgStrokeWidth(vg, 1.3f);
				nvgStrokeColor(vg, nvgRGBA(0x7e, 0xe0, 0xa8, (int) (a * 190)));
				nvgStroke(vg);
			}
		}

		// Telemetry readout sidebar (right column)
		void drawHud(NVGcontext* vg, const CyberlineSagittaCore::Snapshot* s) {
			float tx = 300.f, ty = 14.f, tw = 78.f, th = 288.f;

			// Telemetry enclosure
			nvgBeginPath(vg);
			nvgRoundedRect(vg, tx, ty, tw, th, 6.f);
			nvgFillColor(vg, nvgRGB(0x09, 0x0c, 0x12));
			nvgFill(vg);
			strokeNeon(vg, nvgRGBA(0x5a, 0x48, 0x7c, 110), 0.9f, 2.4f, 20);

			auto font = getLabelFont();
			if (!font || font->handle < 0) return;
			nvgFontFaceId(vg, font->handle);

			// Title
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
			nvgFontSize(vg, 6.6f);
			nvgFillColor(vg, nvgRGB(0xc7, 0x9f, 0xfb));
			nvgText(vg, tx + tw * 0.5f, ty + 6.f, "[ TELEMETRY ]", NULL);

			// Knob & switch labels
			nvgFontSize(vg, 6.2f);
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
			nvgFillColor(vg, nvgRGB(0x8a, 0x96, 0xa2));
			nvgText(vg, tx + tw * 0.5f, 57.f, "GAIN", NULL);
			nvgText(vg, tx + tw * 0.5f, 103.f, "MODE", NULL);

			// Bar meters
			float x = tx + 8.f;
			float y = 120.f;

			auto label = [&](float yy, const char* txt) {
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
				nvgFontSize(vg, 6.4f);
				nvgFillColor(vg, nvgRGB(0x8a, 0x96, 0xa2));
				nvgText(vg, x, yy, txt, NULL);
			};

			auto bar = [&](float yy, float v, NVGcolor col, bool center = false) {
				v = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
				nvgBeginPath(vg);
				nvgRoundedRect(vg, x + 19.f, yy - 2.f, 26.f, 4.f, 2.f);
				nvgFillColor(vg, nvgRGBA(0x20, 0x28, 0x32, 180));
				nvgFill(vg);
				float wv = 26.f * v;
				float x0 = center ? x + 19.f + 13.f - wv * 0.5f : x + 19.f;
				nvgBeginPath(vg);
				nvgRoundedRect(vg, x0, yy - 2.f, wv < 0.5f ? 0.5f : wv, 4.f, 2.f);
				nvgFillColor(vg, col);
				nvgFill(vg);
			};

			auto value = [&](float yy, const char* txt) {
				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
				nvgFontSize(vg, 6.4f);
				nvgFillColor(vg, nvgRGB(0xc8, 0xd2, 0xdc));
				nvgText(vg, tx + tw - 8.f, yy, txt, NULL);
			};

			char buf[24];
			label(y, "F0");
			if (s->f0 > 1.f) {
				std::snprintf(buf, sizeof(buf), "%.0fHz", s->f0);
				value(y, buf);
			} else {
				value(y, "--");
			}

			y += 13.5f; label(y, "VOI"); bar(y, s->voicing, nvgRGB(0x7e, 0xe0, 0xa8));
			y += 12.5f; label(y, "TEN"); bar(y, s->tension, nvgRGB(0xc7, 0x9f, 0xfb));
			y += 12.5f; label(y, "PRS"); bar(y, s->press, nvgRGB(0xff, 0x88, 0x64));
			y += 12.5f; label(y, "VEL"); bar(y, s->velum, nvgRGB(0x48, 0xd8, 0xf8));
			y += 12.5f; label(y, "LNG"); bar(y, s->lungsVol, nvgRGB(0x48, 0xd8, 0xf8));
			y += 12.5f; label(y, "FLW"); bar(y, std::fabs(s->lungsFlow),
				(s->lungsFlow >= 0.f) ? nvgRGB(0x48, 0xe0, 0xff) : nvgRGB(0xff, 0x6a, 0x74), true);
			y += 12.5f; label(y, "FRC"); bar(y, s->fricative, nvgRGB(0xff, 0xc4, 0x6b));

			y += 15.f;
			std::snprintf(buf, sizeof(buf), "TNG %.1f/%.1f", s->tongueX, s->tongueY);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
			nvgFillColor(vg, nvgRGB(0xb8, 0xc4, 0xd0));
			nvgText(vg, x, y, buf, NULL);

			y += 12.f;
			if (s->constrPos > 0.01f) {
				std::snprintf(buf, sizeof(buf), "CNS %.1f/%.1f", s->constrPos, s->constrWidth);
			} else {
				std::snprintf(buf, sizeof(buf), "CNS --");
			}
			nvgText(vg, x, y, buf, NULL);

			y += 12.f;
			std::snprintf(buf, sizeof(buf), "SND %d%%",
				(int) (std::fmin(s->lipEnv * 2.4f, 1.f) * 100.f));
			nvgText(vg, x, y, buf, NULL);
		}
	};

	// ================= module panel & background =================
	static void drawPanel(const rack::widget::Widget::DrawArgs& args, float panelW) {
		NVGcontext* vg = args.vg;
		NVGcolor bg = nvgRGB(0x0c, 0x0f, 0x14);
		NVGcolor edge = nvgRGB(0x20, 0x28, 0x34);
		NVGcolor accent = nvgRGB(0x48, 0xd8, 0xf8);

		// Deep dark Cyberline background
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, panelW, rack::app::RACK_GRID_HEIGHT);
		nvgFillColor(vg, bg);
		nvgFill(vg);
		nvgStrokeWidth(vg, 1.f);
		nvgStrokeColor(vg, edge);
		nvgStroke(vg);

		// Top cyan neon accent bar
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, panelW, 4.5f);
		nvgFillColor(vg, accent);
		nvgFill(vg);

		nvgBeginPath(vg);
		nvgMoveTo(vg, 0, 5.5f);
		nvgLineTo(vg, panelW, 5.5f);
		nvgStrokeWidth(vg, 0.8f);
		nvgStrokeColor(vg, nvgRGBA(0x48, 0xd8, 0xf8, 110));
		nvgStroke(vg);

		auto font = getLabelFont();
		if (font && font->handle >= 0) {
			nvgFontFaceId(vg, font->handle);

			// Jack labels & color accents
			nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_BOTTOM);
			static const char* jn[6] = {"TRACT", "LARYNX", "PRES", "VOL", "FLOW", "VOICED"};
			static const NVGcolor jc[6] = {
				nvgRGB(0xc7, 0x9f, 0xfb), // TRACT (violet)
				nvgRGB(0xff, 0x7a, 0x6b), // LARYNX (coral)
				nvgRGB(0xf0, 0x88, 0x98), // PRES (rose)
				nvgRGB(0x48, 0xd8, 0xf8), // VOL (cyan)
				nvgRGB(0xf8, 0xb8, 0x68), // FLOW (amber)
				nvgRGB(0xe8, 0x8c, 0xd0)  // VOICED (magenta)
			};

			float jx[6] = {34.f, 94.f, 154.f, 214.f, 274.f, 334.f};
			float jy = 322.f;

			for (int i = 0; i < 6; i++) {
				nvgFillColor(vg, jc[i]);
				nvgText(vg, jx[i], jy, jn[i], NULL);

				// Port accent ring
				nvgBeginPath(vg);
				nvgCircle(vg, jx[i], 340.f, 13.5f);
				nvgStrokeWidth(vg, 0.8f);
				nvgStrokeColor(vg, nvgRGBA(jc[i].r * 255, jc[i].g * 255, jc[i].b * 255, 75));
				nvgStroke(vg);
			}
		}

		drawPanelTitle(args, panelW, "SAGITTA");
	}

	void draw(const DrawArgs& args) override {
		drawPanel(args, PANEL_W);
		ModuleWidget::draw(args);
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

		// Anatomy view
		auto view = new FluoroView();
		view->w = this;
		view->box.pos = rack::Vec(0.f, 0.f);
		view->box.size = box.size;
		addChild(view);

		// Parameters (right column)
		addParam(rack::createParamCentered<RoundSmallBlackKnob>(
			rack::Vec(339.f, 42.f), module, CyberlineSagitta::GAIN_PARAM));
		addParam(rack::createParamCentered<ModeSwitch>(
			rack::Vec(339.f, 86.f), module, CyberlineSagitta::MODE_PARAM));

		// Input jacks row
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
