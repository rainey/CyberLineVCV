// cyberline — plugin registration.
// NOTE (JayBird): this file is EXCLUDED from the Cardinal-side build
// (filtered out of PLUGIN_FILES); Cardinal's generated plugins-jaybird.cpp
// defines pluginInstance__cyberline and dispatches init itself.

#include <rack.hpp>
#include "CyberlineModels.hpp"
#include "CyberlineLarynx.hpp"

rack::Plugin* pluginInstance;

void init(rack::Plugin* p) {
	pluginInstance = p;

	cyberline::CyberlineQuality::init(p);   // load the global DSP quality (default LOW)

	p->addModel(cyberline::modelCyberlineLarynx);
	p->addModel(cyberline::modelCyberlineLungs);
	p->addModel(cyberline::modelCyberlineOralChamber);
	p->addModel(cyberline::modelCyberlineVoice);
	p->addModel(cyberline::modelCyberlineHeart);
	p->addModel(cyberline::modelCyberlineBroca);
	p->addModel(cyberline::modelCyberlineBrocaSq);
	// Sagitta (vocal-system fluoroscopy) is implemented but unregistered
	// from the main build for now. Revisit later. To re-enable: uncomment
	// cyberline::modelCyberlineSagitta here and re-add to plugin.json.
	// p->addModel(cyberline::modelCyberlineSagitta);
	// GOTCHA: any patch saved while a model was registered still holds a
	// module instance for it; once unregistered, Rack's patch loader pops
	// a blocking "Could not find module" modal before the main loop (the
	// window opens black and ignores input). scripts/rackplay.sh prunes
	// stale instances from the test user dir; other saved patches need
	// the instance removed by hand before they will load.
	// Visual1 (audio→image DCT visualizer) is implemented but unregistered
	// for now — the first mapping was underwhelming; revisit later. To
	// re-enable: add cyberline::modelCyberlineVisual1 here AND to
	// CyberlineModels.hpp, plus a plugin.json modules entry.
	// p->addModel(cyberline::modelCyberlineVisual1);
}
