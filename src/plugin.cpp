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
}
