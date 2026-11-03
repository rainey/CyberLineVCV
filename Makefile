# Standalone Rack-plugin build (for local iteration).
# The JayBird/Cardinal-side integration does NOT use this Makefile; see
# docs/spec.md §7.
# Point RACK_DIR at a Rack source tree (or SDK) with plugin.mk.

RACK_DIR ?= ../JayBird/src/Rack

FLAGS +=
CFLAGS +=
CXXFLAGS +=

# Rack submodule's dep/ headers are not pre-installed into dep/include here;
# add the needed include dirs explicitly (nanovg, nanosvg, glfw, blendish,
# pffft). Only affects compilation — linking happens in the JayBird/Cardinal
# build, not here.
DEP_INCS = -I$(RACK_DIR)/dep/nanovg/src -I$(RACK_DIR)/dep/nanosvg/src \
	-I$(RACK_DIR)/dep/glfw/include -I$(RACK_DIR)/dep/oui-blendish \
	-I$(RACK_DIR)/dep/pffft

LDFLAGS +=

SOURCES += $(wildcard src/*.cpp)

DISTRIBUTABLES += $(wildcard LICENSE*) res presets

CXXFLAGS += $(DEP_INCS)

include $(RACK_DIR)/plugin.mk

# ---- quick test-in-Rack loop ----
#   make rack     build + install into the local Rack play dir + launch GUI
#
# The plugin.mk link fails in this tree by design (libRack is provided by
# the host binary at load time), so: let make compile the objects, tolerate
# the expected link error, then link host-resolved exactly like
# scripts/preview.sh does, publish to the previews location that
# scripts/rackplay.sh installs from, and launch Rack (which also deploys
# presets/ and clears the crash-flag state).
.PHONY: rack
rack:
	@$(MAKE) -j4 2>&1 | grep -Ev 'cannot find -lRack|collect2:|^g\+\+ |^mkdir|Error [0-9]|\*\*\* \[' || true
	@g++ -o plugin.so build/src/*.cpp.o -shared -static-libstdc++ -static-libgcc
	@mkdir -p /tmp/opencode/rackuser-previews
	@cp plugin.so /tmp/opencode/rackuser-previews/
	@./scripts/rackplay.sh

# ---- harness quick-run ----
#   make test     build + run the Heart anchors (bpm/stroke/ectopics/etc.)
.PHONY: test
test:
	@g++ -O2 -std=c++11 -o /tmp/opencode/render_heart tests/render_heart.cpp
	@/tmp/opencode/render_heart /tmp/opencode/heart.wav
