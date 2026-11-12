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
	-I$(RACK_DIR)/dep/pffft -I$(RACK_DIR)/dep/osdialog

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
#   make test          build + run the Heart anchors (bpm/stroke/ectopics/etc.)
#   make test-modules  build + run the full module-level integration tests (Lungs, Broca, Poly, etc.)
.PHONY: test
test:
	@g++ -O2 -std=c++11 -o /tmp/opencode/render_heart tests/render_heart.cpp
	@/tmp/opencode/render_heart /tmp/opencode/heart.wav

.PHONY: test-modules
test-modules:
	@RACK=$${RACK_LIB_DIR:-/tmp/opencode/rack241/Rack2Free}; \
	D=../JayBird/src/Rack/dep; \
	nice -n 10 g++ -std=c++11 -O2 -march=nehalem -DARCH_X64 -DARCH_LIN \
	  -I$$D/nanovg/src -I$$D/nanosvg/src -I$$D/glfw/include -I$$D/oui-blendish -I$$D/pffft \
	  -I../JayBird/src/Rack/include -I$$D/include \
	  -o /tmp/test_modules tests/test_modules.cpp \
	  src/CyberlineLungs.cpp src/CyberlineBroca.cpp src/CyberlineBrocaSq.cpp src/CyberlineOralChamber.cpp \
	  src/CyberlineLarynx.cpp src/CyberlineVoice.cpp src/CyberlinePtBreakout.cpp \
	  -L$$RACK -lRack -Wl,-rpath,$$RACK && /tmp/test_modules
