// CyberlineMidiMapWidget — programmatic 26 HP hollow neon vector panel
// for "MIDI MAP". House styling shared with the other cyberline panels:
// flat dark plate, coral neon top bar, jack boxes, neon port rings.
// Top: MIDI driver/device/channel display (standard Rack MidiDisplay) +
// the SPEC strip (click to switch synth spec / load a JSON spec file).
// Center: a 4x4 grid of CV-IN cells; each cell shows its assigned
// parameter name + CC/NRPN tag; click a cell to pick its parameter from
// a grouped menu. The module transmits MIDI itself — there are no CV
// outputs; the jacks are all inputs.

#include "CyberlineMidiMap.hpp"
#include "CyberlineModels.hpp"
#include <rack.hpp>
#include <osdialog.h>
#include <fstream>
#include <map>

namespace cyberline {

// Truncate a parameter name to fit a cell label.
static std::string truncateCellLabel(const std::string& name) {
	return name.size() > 30 ? name.substr(0, 29) + "\xE2\x80\xA6" : name; // ellipsis
}

struct CyberlineMidiMapWidget : rack::app::ModuleWidget {
	static constexpr float PANEL_W = 26 * 15.f; // 390 px (26 HP)
	static constexpr float GRID_X = 8.f;
	static constexpr float GRID_Y = 78.f;
	static constexpr float ROW_H = 66.f; // cell height + gap
	static constexpr float CELL_H = 62.f;

	CyberlineMidiMapWidget(CyberlineMidiMap* module) {
		setModule(module);
		box.size = rack::Vec(PANEL_W, rack::app::RACK_GRID_HEIGHT);

		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, 0)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(15, rack::app::RACK_GRID_HEIGHT - 15)));
		addChild(rack::createWidget<rack::componentlibrary::ScrewSilver>(rack::Vec(box.size.x - 30, rack::app::RACK_GRID_HEIGHT - 15)));

		using rack::componentlibrary::PJ301MPort;

		// MIDI display (driver/device/channel), top-left. Standard Rack
		// MidiDisplay: click rows to pick driver/device/channel.
		auto* midiDisplay = rack::createWidget<rack::app::MidiDisplay>(rack::Vec(GRID_X, 10.f));
		midiDisplay->box.size = rack::Vec(160.f, 24.f);
		midiDisplay->setMidiPort(module ? &module->midiOutput : NULL);
		addChild(midiDisplay);

		// SPEC strip, top-right; clicking opens the spec chooser.
		auto* strip = rack::createWidget<SpecStrip>(rack::Vec(PANEL_W - 190.f, 10.f));
		strip->box.size = rack::Vec(182.f, 24.f);
		strip->module = module;
		addChild(strip);

		// 4x4 grid of cell widgets (click-to-assign + hover highlight)
		// with the 16 IN jacks as children above it.
		auto* grid = rack::createWidget<MidiMapGrid>(rack::Vec(GRID_X, GRID_Y));
		grid->box.size = rack::Vec(PANEL_W - GRID_X * 2, ROW_H * 4 - 4.f);
		grid->module = module;
		grid->widget = this;
		addChild(grid);

		for (int r = 0; r < 4; r++) {
			for (int c = 0; c < 4; c++) {
				int cell = r * 4 + c;
				float cx = GRID_X + c * (PANEL_W - GRID_X * 2) / 4.f;
				float cy = GRID_Y + r * ROW_H;
				// IN jack: bottom-left of the cell
				rack::math::Vec inPos(cx + 24.f, cy + CELL_H - 18.f);
				addInput(rack::createInputCentered<PJ301MPort>(inPos, module, CyberlineMidiMap::CELL0_INPUT + cell));
			}
		}
	}

	// Build the parameter-assign menu for one cell: one submenu per group
	// (in spec order), entries "name" with rightText "CC n"/"NRPN m:l".
	void buildCellMenu(rack::ui::Menu* menu, int cell) {
		auto* m = dynamic_cast<CyberlineMidiMap*>(module);
		if (!m)
			return;
		const MidiMapSpec* s = m->spec();
		menu->addChild(rack::createMenuLabel(rack::string::uppercase(s->synth)));
		menu->addChild(new rack::ui::MenuSeparator);

		// Group in stable order of first appearance
		std::vector<std::string> groupOrder;
		std::map<std::string, std::vector<int>> groups;
		for (int i = 0; i < (int) s->params.size(); i++) {
			const std::string& g = s->params[i].group.empty() ? std::string("General") : s->params[i].group;
			if (groups.find(g) == groups.end())
				groupOrder.push_back(g);
			groups[g].push_back(i);
		}

		int current = (int) std::lround(m->params[CyberlineMidiMap::CELL0_PARAM + cell].getValue());
		for (const auto& g : groupOrder) {
			auto& idxs = groups[g];
			// Small groups go flat on the menu; larger ones get submenus.
			if (idxs.size() <= 4) {
				for (int i : idxs) {
					const MidiMapParam* p = &s->params[i];
					menu->addChild(rack::createCheckMenuItem(p->name,
						p->tag(),
						[=]() { return i == current; },
						[=]() { setCellTarget(cell, i); }));
				}
				menu->addChild(new rack::ui::MenuSeparator);
			} else {
				struct GroupItem : rack::ui::MenuItem {
					CyberlineMidiMapWidget* w;
					int cell;
					std::vector<int> idxs;
					const MidiMapSpec* spec;
					int current;
					rack::ui::Menu* createChildMenu() override {
						auto* sub = rack::createMenu();
						for (int i : idxs) {
							const MidiMapParam* p = &spec->params[i];
							sub->addChild(rack::createCheckMenuItem(p->name,
								p->tag(),
								[=]() { return i == current; },
								[=]() { w->setCellTarget(cell, i); }));
						}
						return sub;
					}
				};
				auto* item = rack::createMenuItem<GroupItem>(g + " \xE2\x96\xB8", rack::string::f("(%d)", (int) idxs.size()));
				item->w = this;
				item->cell = cell;
				item->idxs = idxs;
				item->spec = s;
				item->current = current;
				menu->addChild(item);
			}
		}
	}

	void setCellTarget(int cell, int index) {
		auto* m = dynamic_cast<CyberlineMidiMap*>(module);
		if (!m)
			return;
		float oldV = m->params[CyberlineMidiMap::CELL0_PARAM + cell].getValue();
		if (oldV == (float) index)
			return;
		m->params[CyberlineMidiMap::CELL0_PARAM + cell].setValue((float) index);
		auto* h = new rack::history::ParamChange;
		h->name = "assign cell";
		h->moduleId = module->id;
		h->paramId = CyberlineMidiMap::CELL0_PARAM + cell;
		h->oldValue = oldV;
		h->newValue = (float) index;
		APP->history->push(h);
	}

	// ---- Grid widget (hover highlight + click-to-assign + drawing) ----
	struct MidiMapGrid : rack::widget::Widget {
		CyberlineMidiMap* module = NULL;
		CyberlineMidiMapWidget* widget = NULL;
		int hoverCell = -1;

		int cellAt(rack::math::Vec p) {
			float cw = box.size.x / 4.f;
			float chh = box.size.y / 4.f;
			int col = (int) std::floor(p.x / cw);
			int row = (int) std::floor(p.y / chh);
			if (col < 0 || col > 3 || row < 0 || row > 3)
				return -1;
			return row * 4 + col;
		}

		void onHover(const rack::event::Hover& e) override {
			Widget::onHover(e);
			hoverCell = cellAt(e.pos);
		}

		void onLeave(const rack::event::Leave& e) override {
			Widget::onLeave(e);
			hoverCell = -1;
		}

		void onButton(const rack::event::Button& e) override {
			Widget::onButton(e);
			if (e.button != GLFW_MOUSE_BUTTON_LEFT)
				return;
			if (e.action == GLFW_PRESS && module && widget) {
				int cell = cellAt(e.pos);
				if (cell < 0)
					return;
				auto* menu = rack::createMenu();
				widget->buildCellMenu(menu, cell);
				e.consume(this);
			}
		}

		void draw(const DrawArgs& args) override {
			NVGcontext* vg = args.vg;
			auto font = getLabelFont();
			if (font && font->handle >= 0)
				nvgFontFaceId(vg, font->handle);
			// Screenshot mode / fresh widgets have no module: fall back to
			// the built-in Blofeld spec so cells still show real labels.
			static MidiMapSpec fallbackSpec;
			static bool fallbackInit = false;
			if (!fallbackInit) {
				fallbackSpec = makeBlofeldSpec();
				fallbackInit = true;
			}
			const MidiMapSpec* s = module ? module->spec() : &fallbackSpec;
			float cw = box.size.x / 4.f;

			for (int cell = 0; cell < 16; cell++) {
				int row = cell / 4, col = cell % 4;
				float x0 = col * cw, y0 = row * ROW_H;
				// Cell plate
				nvgBeginPath(vg);
				nvgRoundedRect(vg, x0, y0, cw - 3.f, CELL_H, 3.f);
				nvgFillColor(vg, hoverCell == cell ? nvgRGBA(0x22, 0x28, 0x36, 220)
					: nvgRGBA(0x0b, 0x0d, 0x13, 150));
				nvgFill(vg);
				nvgStrokeWidth(vg, 0.9f);
				nvgStrokeColor(vg, hoverCell == cell ? nvgRGBA(0x9b, 0x6d, 0xff, 190)
					: nvgRGBA(0x2e, 0x34, 0x40, 200));
				nvgStroke(vg);

				int paramIdx = module
					? (int) std::lround(module->params[CyberlineMidiMap::CELL0_PARAM + cell].getValue())
					: std::min(cell, (int) s->params.size() - 1);
				const MidiMapParam* p = s->paramAt(paramIdx);
				if (!p)
					continue;

				// Param label (up to two lines, wrapped on spaces)
				nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
				nvgFontSize(vg, 7.2f);
				nvgFillColor(vg, nvgRGB(0xd8, 0xde, 0xe4));
				{
					std::string label = truncateCellLabel(p->name);
					float labelW = cw - 3.f - 12.f;
					char line1[32] = "", line2[32] = "";
					wrapLabel(label.c_str(), line1, line2, labelW, vg);
					nvgText(vg, x0 + 6.f, y0 + 5.f, line1, NULL);
					if (line2[0])
						nvgText(vg, x0 + 6.f, y0 + 13.f, line2, NULL);
				}

				// CC/NRPN tag + polarity hint (cyan +- = centered/bipolar)
				nvgFontSize(vg, 6.4f);
				nvgFillColor(vg, p->centered ? nvgRGB(0x88, 0xe4, 0xfc) : nvgRGB(0xf8, 0xc0, 0x78));
				std::string tag = p->tag() + (p->centered ? " \xC2\xB1" : "");
				nvgText(vg, x0 + 6.f, y0 + 26.f, tag.c_str(), NULL);

				// Range hint (documented value span)
				nvgFontSize(vg, 5.4f);
				nvgFillColor(vg, nvgRGB(0x6a, 0x74, 0x80));
				char rangeBuf[24];
				if (p->min == 0.f && p->max == 127.f)
					std::snprintf(rangeBuf, sizeof(rangeBuf), "0-127");
				else
					std::snprintf(rangeBuf, sizeof(rangeBuf), "%g-%g", p->min, p->max);
				nvgText(vg, x0 + 6.f, y0 + 34.f, rangeBuf, NULL);

				// IN label beside the jack
				nvgFontSize(vg, 6.f);
				nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
				nvgFillColor(vg, nvgRGB(0x8a, 0x94, 0xa0));
				nvgText(vg, x0 + 24.f - 15.5f, y0 + CELL_H - 26.f, "IN", NULL);

				// Activity bar (right edge): last transmitted 7-bit value
				float v = 0.f;
				if (module) {
					int k = p->key();
					auto it = module->midiOutput.lastValues.find(k);
					if (it != module->midiOutput.lastValues.end())
						v = it->second / 127.f;
				}float barH = (CELL_H - 14.f) * v;
				nvgBeginPath(vg);
				nvgRect(vg, x0 + cw - 9.f, y0 + CELL_H - 7.f - barH, 2.5f, barH);
				nvgFillColor(vg, nvgRGB(0x58, 0xe0, 0xa8));
				nvgFill(vg);
				// bar track
				nvgBeginPath(vg);
				nvgRect(vg, x0 + cw - 9.f, y0 + 7.f, 2.5f, CELL_H - 14.f);
				nvgStrokeWidth(vg, 0.6f);
				nvgStrokeColor(vg, nvgRGBA(0x58, 0xe0, 0xa8, 60));
				nvgStroke(vg);
			}
		}

		// Greedy two-line wrap using measured text widths.
		static void wrapLabel(const char* text, char* line1, char* line2, float maxW, NVGcontext* vg) {
			line1[0] = '\0';
			line2[0] = '\0';
			std::string words = text;
			std::string cur, l1, l2;
			bool line2Started = false;
			size_t pos = 0;
			while (pos <= words.size()) {
				size_t sp = words.find(' ', pos);
				std::string word = (sp == std::string::npos) ? words.substr(pos) : words.substr(pos, sp - pos);
				std::string trial = cur.empty() ? word : cur + " " + word;
				float w = nvgTextBounds(vg, 0, 0, trial.c_str(), NULL, NULL);
				if (w > maxW && !cur.empty()) {
					if (!line2Started) {
						l1 = cur;
						line2Started = true;
						cur = word;
					} else {
						break; // too long for two lines
					}
				} else {
					cur = trial;
				}
				if (sp == std::string::npos)
					break;
				pos = sp + 1;
			}
			if (!line2Started)
				l1 = cur;
			else
				l2 = cur;
			std::snprintf(line1, 32, "%s", l1.c_str());
			std::snprintf(line2, 32, "%s", l2.c_str());
		}
	};

	// ---- Spec strip (top-right): synth name + click menu ----
	struct SpecStrip : rack::widget::Widget {
		CyberlineMidiMap* module = NULL;

		void onButton(const rack::event::Button& e) override {
			Widget::onButton(e);
			if (e.button != GLFW_MOUSE_BUTTON_LEFT)
				return;
			if (e.action == GLFW_PRESS && module) {
				auto* menu = rack::createMenu();
				buildSpecMenu(menu);
				e.consume(this);
			}
		}

		void buildSpecMenu(rack::ui::Menu* menu) {
			auto* m = module;
			menu->addChild(rack::createMenuLabel("Synth spec"));
			menu->addChild(new rack::ui::MenuSeparator);
			struct SpecItem : rack::ui::MenuItem {
				CyberlineMidiMap* m;
				int index;
				void onAction(const rack::widget::Widget::ActionEvent& e) override {
					MenuItem::onAction(e);
					m->setSpec(index, true);
				}
			};
			auto addSpec = [&](int index, const char* name) {
				auto* item = rack::createMenuItem<SpecItem>(name, "");
				item->m = m;
				item->index = index;
				item->rightText = (m->specIndex == index) ? "\xE2\x9C\x93" : "";
				menu->addChild(item);
			};
			addSpec(CyberlineMidiMap::SPEC_BLOFELD, "Waldorf Blofeld");
			addSpec(CyberlineMidiMap::SPEC_ULTRANOVA, "Novation UltraNova");
			addSpec(CyberlineMidiMap::SPEC_GENERIC, "Generic CC 0-127");
			if (m->specIndex == CyberlineMidiMap::SPEC_LOADED) {
				auto* item = rack::createMenuItem<SpecItem>(m->loadedPath.empty() ? "(loaded spec)" : m->loadedPath, "\xE2\x9C\x93");
				item->m = m;
				item->index = CyberlineMidiMap::SPEC_LOADED;
				item->disabled = true; // shown for state; re-load via JSON action
				menu->addChild(item);
			}
			menu->addChild(new rack::ui::MenuSeparator);
			// Load a spec JSON from disk (new synths need no code — see
			// parseMidiMapSpecJson for the schema).
			menu->addChild(rack::createMenuItem("Load spec JSON\xE2\x80\xA6", "",
				[=]() {
					osdialog_filters* filters = osdialog_filters_parse("JSON:json");
					DEFER({ osdialog_filters_free(filters); });
					char* path = osdialog_file(OSDIALOG_OPEN, NULL, NULL, filters);
					if (!path)
						return;
					DEFER({ std::free(path); });
					std::ifstream f(path, std::ios::binary);
					if (!f.good())
						return;
					std::string text((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
					std::string err;
					if (!m->loadSpecJson(text.c_str(), path, &err)) {
						// Surface the parse error in the Rack log; the menu
						// simply stays closed on failure.
						WARN("MIDI Map spec load failed (%s): %s", path, err.c_str());
					}
				}));
			menu->addChild(rack::createMenuLabel("  spec JSON: {synth, params:["));
			menu->addChild(rack::createMenuLabel("  {cc | nrpn:\"m:l\", name,"));
			menu->addChild(rack::createMenuLabel("  min, max, centered, group}]}"));
		}

		void draw(const DrawArgs& args) override {
			NVGcontext* vg = args.vg;
			auto font = getLabelFont();
			if (font && font->handle >= 0)
				nvgFontFaceId(vg, font->handle);
			const MidiMapSpec* s = module ? module->spec() : NULL;			// Strip plate
			nvgBeginPath(vg);
			nvgRoundedRect(vg, 0, 0, box.size.x, box.size.y, 3.f);
			nvgFillColor(vg, nvgRGBA(0x0b, 0x0d, 0x13, 150));
			nvgFill(vg);
			nvgStrokeWidth(vg, 0.9f);
			nvgStrokeColor(vg, nvgRGBA(0x2e, 0x34, 0x40, 200));
			nvgStroke(vg);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_BOTTOM);
			nvgFontSize(vg, 5.8f);
			nvgFillColor(vg, nvgRGB(0x6a, 0x74, 0x80));
			nvgText(vg, 8.f, 11.f, "SPEC", NULL);
			nvgFontSize(vg, 8.5f);
			nvgFillColor(vg, nvgRGB(0xd8, 0xde, 0xe4));
			// Screenshot mode / fresh widgets: fall back to the Blofeld name.
			static std::string fallbackName = makeBlofeldSpec().synth;
			std::string name = s ? s->synth : fallbackName;
			if (name.size() > 26)
				name = name.substr(0, 25) + "\xE2\x80\xA6";
			nvgText(vg, 8.f, 20.f, name.c_str(), NULL);
			nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
			nvgFontSize(vg, 7.f);
			nvgFillColor(vg, nvgRGB(0x9b, 0x6d, 0xff));
			nvgText(vg, box.size.x - 8.f, 20.f, "\xE2\x96\xBE", NULL);
		}
	};

	void appendContextMenu(rack::ui::Menu* menu) override {
		ModuleWidget::appendContextMenu(menu);
		auto* m = dynamic_cast<CyberlineMidiMap*>(module);
		if (!m)
			return;
		// Full MIDI driver/device/channel config, also available here.
		menu->addChild(new rack::ui::MenuSeparator);
		rack::app::appendMidiMenu(menu, &m->midiOutput);
		menu->addChild(new rack::ui::MenuSeparator);
		menu->addChild(rack::createMenuItem("Resend all cell values", "",
			[=]() { m->resendAll(); }));
		menu->addChild(rack::createMenuLabel("  IN: standard 0..+10 V,"));
		menu->addChild(rack::createMenuLabel("  centered (\xC2\xB1) -5..+5 V"));
		menu->addChild(rack::createMenuLabel("  unpatched IN = range midpoint"));
		menu->addChild(rack::createMenuLabel("  sends on change, 200 Hz max"));
	}

	void draw(const DrawArgs& args) override {
		drawPanel(args, PANEL_W);
		ModuleWidget::draw(args);
		drawCpuMeterText(args, dynamic_cast<CyberlineMidiMap*>(module));
	}

	// ---- Panel ----
	static void drawPanel(const rack::widget::Widget::DrawArgs& args, float panelW) {
		NVGcontext* vg = args.vg;
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, panelW, rack::app::RACK_GRID_HEIGHT);
		nvgFillColor(vg, nvgRGB(0x13, 0x14, 0x1a));
		nvgFill(vg);
		nvgStrokeWidth(vg, 1.f);
		nvgStrokeColor(vg, nvgRGB(0x2a, 0x2e, 0x38));
		nvgStroke(vg);
		// Top neon bar + hollow cyberline sub-trace (house signature)
		nvgBeginPath(vg);
		nvgRect(vg, 0, 0, panelW, 5);
		nvgFillColor(vg, nvgRGB(0xf0, 0x5c, 0x64));
		nvgFill(vg);
		nvgBeginPath(vg);
		nvgMoveTo(vg, 0, 7.5f);
		nvgLineTo(vg, panelW, 7.5f);
		nvgStrokeWidth(vg, 0.8f);
		nvgStrokeColor(vg, nvgRGBA(0xf0, 0x5c, 0x64, 130));
		nvgStroke(vg);
		// Grid backdrop trace (violet, very dim)
		nvgBeginPath(vg);
		nvgRect(vg, GRID_X - 2.f, GRID_Y - 2.f, panelW - (GRID_X - 2.f) * 2,
			ROW_H * 4 - 4.f + 4.f);
		nvgStrokeWidth(vg, 0.7f);
		nvgStrokeColor(vg, nvgRGBA(0x8e, 0x62, 0xf8, 70));
		nvgStroke(vg);
		// Convention hint (under the midi/spec strip)
		auto font = getLabelFont();
		if (font && font->handle >= 0) {
			nvgFontFaceId(vg, font->handle);
			nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
			nvgFontSize(vg, 5.6f);
			nvgFillColor(vg, nvgRGB(0x6a, 0x74, 0x80));
			nvgText(vg, GRID_X, 40.f, "click cell to assign \xC2\xB7 cyan \xC2\xB1 = -5..+5 V in \xC2\xB7 bar = sent value", NULL);
		}
		drawPanelTitle(args, panelW, "MIDI MAP");
	}
};

} // namespace cyberline

namespace cyberline {
rack::plugin::Model* modelCyberlineMidiMap =
	rack::createModel<CyberlineMidiMap, CyberlineMidiMapWidget>("cyberline-midimap");
} // namespace cyberline
