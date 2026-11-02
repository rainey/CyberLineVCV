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

DISTRIBUTABLES += $(wildcard LICENSE*) res

CXXFLAGS += $(DEP_INCS)

include $(RACK_DIR)/plugin.mk
