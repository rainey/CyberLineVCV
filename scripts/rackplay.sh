#!/bin/zsh
# Launch Rack Free with the cyberline plugin pre-installed, clearing the
# crash-flag state each time.
#
# Why: Rack pops a blocking modal ("Rack crashed during the last session")
# whenever log.txt lacks its clean-exit "END" marker — which happens after
# ANY unclean exit (killed process, closed terminal). The modal can appear
# off-screen/behind, making Rack look dead ("window opens, GUI won't load").
# This launcher removes the log first so the modal can never appear, and
# resets a saved off-screen window position.
#
# Usage: scripts/rackplay.sh
#   GUI session (interactive). Close Rack via Quit to keep state clean;
#   if you kill it instead, just run this script again.

RACK_DIR=/tmp/opencode/rack241/Rack2Free
PLAY_DIR=/tmp/opencode/rackplay
PLUGIN_SRC=$PLAY_DIR/plugin-src

set -e

# refresh plugin from the latest preview build if available
if [[ -f /tmp/opencode/rackuser-previews/plugin.so ]]; then
	mkdir -p "$PLUGIN_SRC"
	cp /tmp/opencode/rackuser-previews/plugin.so "$PLUGIN_SRC/"
	cp /storage/69/vcvprojects/cyberline/plugin.json \
		/storage/69/vcvprojects/cyberline/LICENSE "$PLUGIN_SRC/"
fi

# clear crash-flag state + off-screen window position
rm -f "$PLAY_DIR/log.txt"
python3 - "$PLAY_DIR/settings.json" <<'EOF' 2>/dev/null || true
import json, sys
p = sys.argv[1]
try:
	d = json.load(open(p))
	if d.pop('windowPos', None) is not None:
		json.dump(d, open(p, 'w'), indent=2)
except Exception:
	pass
EOF

cd "$RACK_DIR"
exec env LIBGL_ALWAYS_SOFTWARE=1 ./Rack -u "$PLAY_DIR"
