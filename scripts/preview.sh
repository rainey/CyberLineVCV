#!/bin/zsh
# cyberline — local panel preview WITHOUT JayBird/Cardinal.
#
# Builds plugin.so with unresolved Rack symbols (standard Rack plugin
# convention — the host binary provides libRack symbols at load), then runs
# the official Rack Free 2.4.1 binary in screenshot mode to render every
# module panel to PNG.
#
# Usage: scripts/preview.sh [zoom]
#   zoom 1 -> 380px-tall PNGs (default)
# Requires: the Rack Free binary unzipped at /tmp/opencode/rack241/Rack2Free
# (RackFree-2.4.1-lin-x64.zip from vcvrack.com/downloads).
#
# Notes:
# - Software GL (LIBGL_ALWAYS_SOFTWARE=1) is forced: the screenshot path is
#   slow under llvmpipe, so expect ~1-2 minutes. On shared machines this is
#   also politer than GPU contention.
# - Fresh user dir every run (crash-flag pollution hangs headless runs on a
#   modal recovery dialog).
# - Screenshots land in <userdir>/screenshots/cyberline/ (path echoed at end).
# - Rack 2 scans the OS-arch plugin dir (plugins-lin-x64), NOT plugins-v2.

zoom=${1:-1}
set -e
cd "$(dirname "$0")/.."

RACK_BIN=/tmp/opencode/rack241/Rack2Free/Rack
# Fresh user dir every run: a polluted dir (e.g. crash flags from a previous
# GPU failure) makes Rack block on a modal crash-recovery dialog before
# screenshotting, which hangs headless runs.
USER_DIR=$(mktemp -d /tmp/opencode/rackuser.XXXXXX)

# 1. link plugin.so WITHOUT -lRack (symbols resolve at host load time)
nice -n 10 g++ -o plugin.so \
	build/src/CyberlineLarynx.cpp.o build/src/CyberlineLarynxWidget.cpp.o \
	build/src/CyberlineLungs.cpp.o build/src/CyberlineLungsWidget.cpp.o \
	build/src/CyberlineOralChamber.cpp.o build/src/CyberlineOralChamberWidget.cpp.o \
	build/src/CyberlineVoice.cpp.o build/src/CyberlineVoiceWidget.cpp.o \
	build/src/plugin.cpp.o \
	-shared -static-libstdc++ -static-libgcc

# 2. install into the OS-arch plugin dir Rack 2 actually scans
PLUGIN_DIR=$USER_DIR/plugins-lin-x64/cyberline
mkdir -p "$PLUGIN_DIR"
cp plugin.json plugin.so LICENSE "$PLUGIN_DIR/"

# 3. screenshot mode (renders every installed module)
cd /tmp/opencode/rack241/Rack2Free
LIBGL_ALWAYS_SOFTWARE=1 nice -n 10 timeout 300 ./Rack -u $USER_DIR -t $zoom

echo "---"
ls -la $USER_DIR/screenshots/cyberline/ 2>/dev/null || echo "no screenshots yet"