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
#   modal recovery dialog); removed on exit.
# - Screenshots are copied to /tmp/opencode/rackuser-previews/cyberline/
#   (stable path, overwritten each run) so they outlive the scratch dir.
# - Rack 2 scans the OS-arch plugin dir (plugins-lin-x64), NOT plugins-v2.

zoom=${1:-1}
set -e
cd "$(dirname "$0")/.."

RACK_DIR_BIN=/tmp/opencode/rack241/Rack2Free
RACK_BIN=$RACK_DIR_BIN/Rack
OUT_DIR=/tmp/opencode/rackuser-previews

# Fresh user dir every run: a polluted dir (e.g. crash flags from a previous
# GPU failure) makes Rack block on a modal crash-recovery dialog before
# screenshotting, which hangs headless runs.
USER_DIR=$(mktemp -d /tmp/opencode/rackuser.XXXXXX)
trap 'rm -rf "$USER_DIR"' EXIT

# 1. link plugin.so WITHOUT -lRack (symbols resolve at host load time).
# Glob, not a hand-list: -shared tolerates missing objects silently, and a
# stale hand-list turns "new module not linked" into "no screenshots yet".
nice -n 10 g++ -o plugin.so build/src/*.cpp.o \
	-shared -static-libstdc++ -static-libgcc

# 2. install into the OS-arch plugin dir Rack 2 actually scans
PLUGIN_DIR=$USER_DIR/plugins-lin-x64/cyberline
mkdir -p "$PLUGIN_DIR"
cp plugin.json plugin.so LICENSE "$PLUGIN_DIR/"

# 3. screenshot mode (renders every installed module).
# `|| true`: a Rack crash/timeout must not abort before the epilogue reports
# where any partial output was copied.
cd "$RACK_DIR_BIN"
LIBGL_ALWAYS_SOFTWARE=1 nice -n 10 timeout 300 "$RACK_BIN" -u $USER_DIR -t $zoom || true

# 4. publish screenshots + the fresh plugin.so at a stable path.
# Publishing plugin.so matters: the scratch dir is deleted on exit, so this
# is the only surviving copy of the exact binary that rendered these PNGs
# (e.g. for copying into an interactive-GUI user dir).
rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"
cp plugin.so "$OUT_DIR/"
if [[ -d $USER_DIR/screenshots ]]; then
	cp -r "$USER_DIR/screenshots/." "$OUT_DIR/"
fi

echo "---"
ls -la "$OUT_DIR/cyberline/" 2>/dev/null || echo "no screenshots yet"
