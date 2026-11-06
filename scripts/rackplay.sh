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

set -e

# refresh plugin from the latest preview build if available.
# NOTE: Rack scans plugins-lin-x64/<slug>/ (plugin-src/ is ignored), so
# install directly into the scanned location.
if [[ -f /tmp/opencode/rackuser-previews/plugin.so ]]; then
	PLUGIN_DIR=$PLAY_DIR/plugins-lin-x64/cyberline
	mkdir -p "$PLUGIN_DIR"
	cp /tmp/opencode/rackuser-previews/plugin.so "$PLUGIN_DIR/"
	cp /storage/69/vcvprojects/cyberline/plugin.json \
		/storage/69/vcvprojects/cyberline/LICENSE "$PLUGIN_DIR/"
	# factory presets (right-click menu scans presets/<model slug>/)
	rm -rf "$PLUGIN_DIR/presets"
	cp -r /storage/69/vcvprojects/cyberline/presets "$PLUGIN_DIR/"
fi

# clear crash-flag state + off-screen window position
rm -f "$PLAY_DIR/log.txt"

# Prune autosave modules whose (plugin, model) is no longer registered by
# any installed plugin or by Core (e.g. a module that was parked /
# unregistered from plugin.json). Without this, Rack's patch loader logs
# "Cannot load model: Could not find module <plugin>/<model>" and then
# blocks startup behind a modal osdialog warning (patch.cpp warningLog)
# before the main loop runs — the window stays black and ignores input,
# and the process must be killed externally. Cables to pruned modules go
# with them. The pre-prune patch is preserved once as patch.json.pre-prune.
RACK_DIR=/tmp/opencode/rack241/Rack2Free python3 - "$PLAY_DIR" <<'EOF' 2>/dev/null || true
import json, glob, os, sys

def norm(s):
	return ''.join(c for c in (s or '') if c.isalnum() or c in '-_')

play = sys.argv[1]
models = {}   # normalized plugin slug -> set of normalized model slugs
rack_dir = os.environ.get('RACK_DIR', '/tmp/opencode/rack241/Rack2Free')
for pj in [os.path.join(rack_dir, 'Core.json')] + \
		glob.glob(play + '/plugins-lin-x64/*/plugin.json'):
	try:
		d = json.load(open(pj))
		slug = norm(d.get('slug'))
		if not slug:
			continue
		models.setdefault(slug, set()).update(
			norm(m.get('slug')) for m in d.get('modules', []))
	except Exception:
		pass

ppath = play + '/autosave/patch.json'
try:
	d = json.load(open(ppath))
except Exception:
	raise SystemExit

mods = d.get('modules', [])
kept, dropped = [], []
for m in mods:
	p, model = norm(m.get('plugin')), norm(m.get('model'))
	if p in models and model in models[p]:
		kept.append(m)
	else:
		dropped.append('%s/%s' % (p, model))
if kept == mods:
	raise SystemExit

try:
	os.replace(ppath, ppath + '.pre-prune')
except Exception:
	pass
d['modules'] = kept
ids = {m.get('id') for m in kept}
d['cables'] = [c for c in d.get('cables', [])
	if c.get('outputModuleId') in ids and c.get('inputModuleId') in ids]
json.dump(d, open(ppath, 'w'), indent=1)
sys.stderr.write('pruned unregistered modules: %s\n' % ', '.join(dropped))
EOF

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
