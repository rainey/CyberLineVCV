# cyberline — module specification v0.1

Status: DRAFT (pre-implementation). License decision: **MIT**.
This document is the functional spec for v1 of the suite. Module code does not
exist yet; nothing here is committed behavior.

## 1. Suite overview

cyberline is a physical-modelling plugin set for VCV Rack / Cardinal, focused on
articulatory voice synthesis built from **atomic, patchable modules** rather
than one monolithic voice synthesizer.

The vocal system decomposes as:

```
[Lungs/Pressure]──CV──▶[Larynx]──audio──▶[Oral Chamber]──audio──▶ out
                          ▲    │            ▲   │      │
                   feedback│    └─ Noise Mod ┘   │      └─ Nose out
              (semi-coupled)        (fric gate)  └─ Shape scan (CV)
```

- **Semi-coupled coupling**: the Larynx accepts an audio "tract feedback"
  input; the user closes the source→filter loop *with a patch cable*. Loading
  amount is a knob. One-way (unpatched) = Pink-Trombone behavior.
- **Species support**: tract length (VTL) is a first-class parameter;
  area-function tables for human / nonhuman tracts arrive as data (human MRI
  tables from Story–Titze; deer/koala from the CT literature later).

### Design principle: no direct formant editing

Users never specify formant frequencies or formant tables directly. Formants
are an *emergent property* of the physical model: they exist only as
resonances of the current tract geometry, and every change to them must come
from perturbing model parameters (tongue position, constriction, VTL, velum,
lip termination, larynx tension/pressure). If a module exposes a knob, the
knob moves **anatomy**, never a resonance frequency. This keeps the modeling
physical (the resonances stay coupled to each other the way real tube modes
are) and makes the modules *predictable anatomical instruments* rather than
fixed filter banks.

### v1 module list

| Module slug | Type name | Function |
|---|---|---|
| `cyberline-larynx` | `CyberlineLarynx` | LF-glottis source (Fant–Liljencrants), CV-driven |
| `cyberline-oral-chamber` | `CyberlineOralChamber` | KL waveguide tract filter with tongue/velum/constriction/injection |
| `cyberline-voice` | `CyberlineVoice` | Fused Larynx+Chamber, pre-patched internally, feedback knob live |
| `cyberline-lungs` | `CyberlineLungs` | Breath-pressure reservoir; doubles as a physically-shaped envelope generator |

### Roadmap (not v1)

- `cyberline-formant-chamber` — **contingent** (see note): Klatt-style cascaded
  formant resonators with vowel *tables*. This is the one planned module that
  would expose resonance frequencies as data, violating the no-direct-formant
  principle — include only if wanted as an explicitly "not-physical" utility
  (Voder lineage), clearly separate from the anatomical modules.
- `cyberline-velar-folds` / second-source module — independent excitor for
  biphonation (koala velar folds, elephant vestibular folds precedents)
- Species table packs (deer, koala, gazelle) with published CT area functions
- Voder-style gesture module (playable CV surface)
- Flagship integrated two-mass model (Ishizaka–Flanagan/Lucero) with full
  source–filter coupling, plus **laryngeal sac** (Helmholtz branch at the
  glottal end — the whale feature; per Cazau 2016, sac inflation lowers its
  formant toward F0 and is a chaos/size-exaggeration mechanism; great apes
  have air sacs too). Whale precedent for the flagship architecture itself:
  Cazau et al. 2016 (Sci. Reports) couple a two-mass source to a time-domain
  wave-reflection tract and reproduce chaos/frequency jumps.
- Lungs: CLOSED mode ships in v1 (the closed-system air-recycling idea);
  a possible v2 extension is an explicit laryngeal-sac intermediate stage
  between exhale and refill.

## 2. Conventions

- All C++ types, globals, file-static names: `Cyberline` prefix or
  `cyberline_` prefix. Model slugs as in the table above. Rationale: 89
  plugins compile into one archive in JayBird; collisions surface only at
  final link.
- Audio levels: Rack convention, ±5 V audio. Internal DSP normalized then
  scaled; outputs clamped.
- CV ranges: 0–1 parameters use 0–10 V input rescaled by 0.1 (with
  attenuverter where noted); pitch is 1 V/octave.
- Sample-rate handling: module `process()` at 1× sampleRate; the Oral Chamber
  runs its waveguide at 2× internally (two steps per sample) as PT does.
- No file I/O, no MIDI, no windowing/glfw calls. Visualization is drawn from
  module state via Rack widgets only. Keeps Cardinal adaptation work at zero.
- Robustness: every recursive/feedback path gets (a) NaN/inf guard — if a
  sample is non-finite, reinitialize that state and continue, (b) output
  clamp. The PinkTromboneVCV port's blowups (NaN under audio-rate modulation,
  silent-state lockup) are the cautionary precedent; both failure modes get
  explicit guards, not hope.
- **Signed-CV convention ("FLOW")**: suite-level interface standard — when a
  cyberline output is signed, **the sign carries physical meaning**
  (direction/state of the modeled system), never "inverted value." Primary
  case: Lungs FLOW (+ = inhale, − = exhale, magnitude = flow intensity).
  Where it reappears later (e.g. forward/backward wave components if the
  Chamber ever exposes taps), the same rule applies.
  - Interop with foreign modules is via three documented transforms:
    rectify (sign → gate), zero-cross trigger (sign reversal → breath-boundary
    clock), and bipolar use (sign as polarity on crossfaders/panners/filter
    morph inputs). All three go in the manual.
  - Panel requirement: signed outputs carry a "±" glyph and a sign LED
    (color-coded per direction); any suite input accepting signed CV
    documents its behavior on negative values (e.g. Larynx PRESSURE goes
    silent on negative = physiologically correct, no phonation while
    inhaling; rectify explicitly to model inhaled/circular phonation).
  - Every signed output also has a plain unipolar/gate sibling where it makes
    sense (Lungs PHASE) for logic-friendly use.

## 3. `cyberline-larynx` — Larynx

Fant–Liljencrants glottal flow derivative generator (the PT Glottis math,
reimplemented from the 1985 paper, plus guards and modular control).

### Inputs

| # | Name | Range | Notes |
|---|---|---|---|
| L1 | F0 (V/OCT) | 1 V/oct | base pitch; knob is octave offset |
| L2 | TENSION | 0–10 V → 0–1 | fold tension → `Rd`; register feel (chest↔falsetto) |
| L3 | PRESSURE | 0–10 V → 0–1 | subglottal pressure; loudness + optional pitch coupling |
| L4 | FEEDBACK | audio | tract loading; see FEEDBACK GAIN |
| L5 | DRIFT CV | ±5 V | adds to internal drift noise |

### Outputs

| # | Name | Notes |
|---|---|---|
| O1 | VOICED | audio, LF pulse + aspiration |
| O2 | NOISE MOD | CV (0–5 V) — the glottal-open-phase noise gate (`getNoiseModulator`); patch into chamber fricative for voicing-synced frication |

### Parameters

| Knob | Default | Range | Notes |
|---|---|---|---|
| TENSION | 0.6 | 0–1 | |
| PRESSURE | 0.8 | 0–1 | replaces PT's `intensity` envelope; smoothed internally |
| FOLD LENGTH | 14 mm | 2–45 mm | vocal-fold length; allometric F0 scaling per Charlton/Fletcher mammal law (`log10 F0 = −1.43·log10 L(mm) + 4.01` at zero tension), plus mass/dim timbre effect (thicker folds = darker). Human ≈ 13–15 mm, giraffe 41 mm; keeps DRIFT/creak behavior physical at big sizes |
| FREQ OCT | −2…+2 | octave offset for L1 | F0 clamped 20–2000 Hz after CV; below 20 Hz the LF period grows unbounded (PT silent-state bug) |
| DRIFT | 0.3 | 0–1 | simplex wobble on F0 and tenseness (PT's `autoWobble`/vibrato noise terms), depth scaled |
| VIBRATO RATE | 6 Hz | 0–12 | sinusoid vibrato depth fixed small; CV of pitch comes from patching |
| FEEDBACK GAIN | 0 | 0–1 | gain on L4 injected into the glottal driving pressure (semi-coupling) |
| P-PITCH COUPLE | 0.3 | 0–1 | how much PRESSURE bends F0 (+2–3 Hz per cmH₂O in vivo; scaled to taste) |
| OUT LEVEL | — | — | |

### DSP notes

- Per-period state: `Rd` clamp [0.5, 2.7]; `Ra, Rk, Rg, Ta, Tp, Te, epsilon,
  shift, Delta, omega, E0, alpha` exactly per LF-paper derivation (PT code
  mirrors it; we reimplement from the paper).
- Period-quantized parameter uptake is **kept** (frequency/tenseness change at
  period boundaries, crossfaded by `lambda`) — it is part of the natural
  glissando feel.
- Aspiration = `intensity·(1−√tenseness)·noiseModulator·white`, noise gated to
  open phase. Internal white noise source (no external input needed).
- FEEDBACK injection: add `feedbackGain·feedbackSample` into the effective
  driving pressure `Ps` path (pre-LF), smoothed by a 1-pole to avoid HF
  instability; NaN guard on the loop.
- Guards: F0 clamp; on non-finite sample → reset period state, zero pulse,
  continue.

## 4. `cyberline-oral-chamber` — Oral Chamber

Kelly–Lochbaum waveguide (44-section default, variable with VTL), nasal
branch, turbulence injection, transient pops, external-source injection.

### Inputs

| # | Name | Range | Notes |
|---|---|---|---|
| C1 | SOURCE | audio | the excitation (usually Larynx VOICED); injected at glottal end |
| C2 | INJECT | audio | second source, injected at INJECT POS |
| C3 | TONGUE X | 0–10 V → position | blade→tip index |
| C4 | TONGUE Y | 0–10 V → diameter | constriction depth |
| C5 | VEUM (VELUM) | 0–10 V → 0–1 | nasal branch opening |
| C6 | CONSTR POS | 0–10 V → position | explicit constriction (teeth/tongue-tip) |
| C7 | CONSTR WIDTH | 0–10 V → width | |
| C8 | FRICATIVE | 0–10 V → amount | noise gain injected at constriction |
| C9 | INJECT POS | 0–10 V → position | where C2 enters the tube |

### Outputs

| # | Name | Notes |
|---|---|---|
| O1 | LIPS | audio, mouth radiation |
| O2 | NOSE | audio, nasal radiation (separate — keep nasal character patchable) |
| O3 | SHAPE | CV scan of per-section amplitude (one section per sample, cycling; 44-sample period) — scope/visualization |
| O4 | PROBE P | CV, pressure at probe position (PROBE knob) |

### Parameters

| Knob | Default | Notes |
|---|---|---|
| TRACT LENGTH (VTL) | 17.5 cm | section count `n = round(VTL · 2·SR / c)`; range ~6–60 cm (human→deer/koala); recomputed on change, state preserved per-section where possible |
| TONGUE X / TONGUE Y | mid | base offsets for C3/C4 |
| VELUM | 0.01 | closed-ish default |
| CONSTR POS/WIDTH/FRIC | — | base offsets for C6/C7/C8 |
| MUSCLE SPEED | 15 cm/s | articulator inertia (`movementSpeed`); the speech-vs-sweep knob |
| WALL DAMPING | 0.999 | per-step loss, both branches (PT nose was undamped — fixed here) |
| LIP REFLECT | −0.85 | radiation-ish termination |
| GLOTTAL REFLECT | 0.75 | termination at source end |
| TRANSIENTS | on | stop-consonant pops on obstruction-clear |
| TABLE | human | area-function table preset (v1: neutral+vowel tilts from published tables; packs later) |
| PROBE POS | — | for O4 |

### DSP notes

- Waveguide per the KL model: per-section scattering with
  `w = r·(R[i−1]+L[i])`, reflection coefficients crossfaded old↔new across
  each 2× step pair (`lambda`) — no clicks when CV moves the tongue.
- Nasal branch: fixed-ish shape from PT (raised profile), length scales with
  VTL; three-way junction math as in PT.
- Turbulence: `thinness = clamp(8·(0.7−d), 0, 1) · clamp(30·(d−0.3), 0, 1)`
  gate at constriction index, split across fractional neighbors, injected
  into both R and L. Noise source internal; optionally voice-synced by
  patching Larynx NOISE MOD → FRICATIVE input (user's choice, not hardcoded).
- Constriction/velum handling: **no negative-diameter UI overloading**
  (PT encodes velum-opening and "stop touches" as negative diameters). All
  parameters explicit.
- INJECT: additive into R/L at fractional index (linear split), DC-coupled by
  default. Sweeping INJECT POS by CV = source travels the tube (syrinx/
  gurgle effects). NaN-guarded.
- Meters/visualization: deterministic update cadence (every N samples), from
  the per-section amplitude arrays; drives the panel drawing and O3 scan.

## 5. `cyberline-voice` — Voice (fused)

Larynx + Chamber on one panel, internally pre-patched; all the important
controls surface as knobs/CV jacks; FEEDBACK GAIN knob closes the loop
internally (no external cable needed). Also exposes Larynx VOICED and both
radiation outputs so it can still feed external effects. Effectively a
mini-flagship; shares the exact DSP classes with the atoms (no duplicate
implementations).

Extra knob unique to Voice: none in v1 beyond convenience layout — keeps it
honest. (Larynx height / integrated-coupling parameters belong to the v2
flagship.)

## 5b. `cyberline-lungs` — Breath (reservoir / envelope generator)

Models a respiratory reservoir: tidal volume filled by inhale, drained by
exhale; output pressure = elastic recoil + muscular effort. Doubles as a
general-purpose envelope generator whose curves come from the physics rather
than from per-stage curve menus.

### Inputs

| # | Name | Range | Notes |
|---|---|---|---|
| G1 | TRIGGER | edge | starts one breath cycle (mode TRIG) |
| G2 | GATE | 0/1 | inhale while high (pedal-style, all modes) |
| G3 | DEPTH CV | 0–10 V → 0–1 | modulates tidal volume |

### Outputs

| # | Name | Notes |
|---|---|---|
| O1 | PRESSURE | 0–10 V; main output, patches directly into Larynx PRESSURE |
| O2 | VOLUME | CV 0–1, current lung volume (the "fuel gauge") |
| O3 | FLOW | signed CV: + during inhale, − during exhale; patchable (noise gates, filters, whistles) |
| O4 | PHASE | gate, high during exhale (voiced air) |

### Parameters

| Knob/switch | Default | Notes |
|---|---|---|
| MODE | CYCLE | CYCLE (free breathing), TRIG (one-shot envelope), CLOSED (air retained after exhale — whale-style recycling) |
| RATE | 12/min | breathing rate in CYCLE mode |
| DEPTH | 0.7 | tidal volume; in TRIG/CLOSED also sets envelope sustain *duration* (energy-limited) |
| INHALE | 0.4 s | time to fill |
| MAX FLOW | — | inhale speed ceiling (calm ↔ gasp) |
| EFFORT | 0.5 | 0 = pure elastic recoil decay (exponential), 1 = actively flattened plateau (speech-like sustain) until volume is spent |

### DSP notes

- Two state integrators: volume `V` (driven by flow) and the pressure shaping
  described by recoil + effort. Exhale pressure ~
  `k·V·recoil + effort·regulated(V)`; the exact shaping is implemented to
  satisfy the two anchor behaviors: exponential recoil decay at EFFORT 0,
  flat plateau until air runs out at EFFORT 1.
- **State floor = 0 V, no explicit residual volume.** Physiologically, lungs
  stop at residual volume (~20% of capacity), but the module does NOT model
  that as a floor constant: the state range is 0..1 and 0 V *is* the floor.
  The audible effect of the residual floor — the end-of-phrase exponential
  taper — is preserved instead through the drain law: exhale drive ∝
  remaining volume (recoil term), so the last stretch drains slowly and
  fades. Renormalizing avoids a hidden discontinuity in the state space
  (VOLUME output would otherwise stall at ~2 V for no user-facing reason).
  Real residual-volume *numbers* only matter for a future species/capacity
  pass (scaling DEPTH/capacity), not for the floor mechanics.
- **Energy-limited sustain:** envelope duration scales with DEPTH; in CLOSED
  mode, subsequent triggers start from current V — durations shrink until the
  system refills (patch VOLUME to logic to sequence "breath management").
- Retrigger during exhale: cut to a short catch-breath (no artificial
  smoothing; the physics provides the click-free shape).
- Outputs are sample-rate-smoothed CV (block-rate physics, per-sample output
  interpolation). NaN guards per suite conventions.
- Non-feature (deliberate): no per-stage linear/log curve menus; curve
  character is EFFORT/DEPTH physics.

## 6. Panels

Simple, programmatically drawn (SVG generated by a small script in the repo):
consistent grid, one accent color per module, labels + signal-type glyphs.
No artwork license axis; artwork_license follows code (MIT) in the JayBird
manifest.

## 7. JayBird integration checklist (for the pin side)

1. Repo lives here; JayBird pins it: `git submodule add <url> plugins/cyberline`
   (URL decided when pushed; ask before any push).
2. `plugins/Makefile`: `PLUGIN_FILES += $(filter-out
   cyberline/src/plugin.cpp,$(wildcard cyberline/src/*.cpp))` — registration
   file excluded; Cardinal's generated `plugins-jaybird.cpp` dispatches init.
3. `plugins/plugins.cpp`: `StaticPluginLoader spl(p, "cyberline", ...)` —
   directory name, not slug.
4. `python3 utils/jaybird/gen_manifest.py` then curate:
   `artwork_license: MIT`, `phone_home: false` (verify by grep — no network
   code by design).
5. Verify in order: `buildability.py --filter cyberline --force --keep` →
   `linkcheck.py` → `nix build .#test --cores 2` (session rule: cores ≤ 2,
   niced).
6. Commit style: kernel-style, minimal.

## 8. Provenance / license rules

- MIT tree. Implement from papers (Fant/Liljencrants 1985; KL 1962 via
  Julius Smith's free text; Story–Titze tables; deer/koala CT papers).
- MIT references (pink-trombone-cpp, sndkit tract/glottis, voc): adaptation
  permitted with copyright notice retained.
- GPL code (VocalTractLab, gnuspeech, PinkTromboneVCV) and the unlicensed
  `2mass.cpp` gist: eyes-only, never copied in.
- Local reference collection: `/storage/69/vcvprojects/refs/` (outside the
  plugin tree, not part of this repo).
