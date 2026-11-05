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

	p->addModel(cyberline::modelCyberlineLarynx);
	p->addModel(cyberline::modelCyberlineLungs);
	p->addModel(cyberline::modelCyberlineOralChamber);
	p->addModel(cyberline::modelCyberlineVoice);
	p->addModel(cyberline::modelCyberlineHeart);
	p->addModel(cyberline::modelCyberlineBroca);
	// Sagitta (vocal-system fluoroscopy) is implemented but unregistered
	// from the main build for now. Revisit later. To re-enable: uncomment
	// cyberline::modelCyberlineSagitta here and re-add to plugin.json.
	// p->addModel(cyberline::modelCyberlineSagitta);
	// Visual1 (audio→image DCT visualizer) is implemented but unregistered
	// for now — the first mapping was underwhelming; revisit later. To
	// re-enable: add cyberline::modelCyberlineVisual1 here AND to
	// CyberlineModels.hpp, plus a plugin.json modules entry.
	// p->addModel(cyberline::modelCyberlineVisual1);
}
