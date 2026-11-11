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
| `cyberline-heart` | `CyberlineHeart` | Elastance cardiac pump: intrinsic clock, Wiggers envelopes, ECG/beats, phonocardiogram, mass allometry |
| `cyberline-broca` | `CyberlineBroca` | "Broca" (Broca-Brika) articulatory cortex & phoneme generator: vowel A/B morphing, consonant onset/manner engine, and polyphonic Tract/Larynx buses |
| `cyberline-broca-sq` | `CyberlineBrocaSq` | "Broca²" dual phoneme Turing machine: two rotating vowel/consonant registers with independent lengths & mutation probability, free/clock/dual-phasor clocking, Broca-compatible poly buses |
| `cyberline-visual1` | `CyberlineVisual1` | Audio→image visualizer: FFT band envelopes drive 8×8 DCT coefficients of a rendered image; perturb mode loads an image and lets the spectrum deform it (unregistered — see §5e) |
| `cyberline-sagitta` | `CyberlineSagitta` | Vocal-system fluoroscopy: sagittal anatomy of the lungs→larynx→tract pipeline driven by the CV buses and a live waveguide replica (§5f) |

### Roadmap (not v1)

- `cyberline-broca-pad` / 2D IPA Vowel-Pad & Voder gesture controller — alternative playable 2D X/Y pad + phoneme bank surface (revisit after `cyberline-broca` vowel/consonant morph workflow).

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
  Sole exception: the DSP-quality global writes/reads one line
  (`low=0|1`) in `quality.json` inside the plugin's user directory at
  plugin init / menu change — a config file, not data I/O.
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
- **Denormal protection**: the vocal modules (Larynx, Oral Chamber, Voice,
  Lungs) open a `cyberline::DenormalGuard` (RAII MXCSR FTZ + DAZ, no-op on
  non-x86) at the top of `process()`, and `CyberlineTract` hard-zeroes the
  damped delay lines + meters after 64 consecutive sub-threshold steps
  (~-300 dB floor). Rationale: decay-to-silence in the waveguide otherwise
  traps the x86 FPU into microcode assists for every junction operation —
  measured ~60x CPU (2.4 µs vs 41 ns per sample) and buffer-overrun pops
  after phonemes.
- **DSP quality (context menu)**: a plugin-wide global with per-module
  overrides, managed from every cyberline module's context menu under
  *DSP quality*:
  - *DSP quality (all modules)*: High (full fidelity) / Low (fast
    approximations). Applies to all modules; the default is **Low**. The
    global persists in `quality.json` (one line, `low=0|1`) inside the
    plugin's user directory — the suite's only file I/O, one line written
    on change (spec §2 "No file I/O" carve-out: a config file, not data
    I/O; Cardinal-safe because `asset::plugin()` resolves inside the
    plugin bundle).
  - *This module (override)* on the quality-bearing modules (Larynx,
    Oral Chamber, Voice): hidden `QUALITY_PARAM` — 0 = High pinned,
    1 = Low pinned, 2 = Follow global (default). Saves with the patch and
    participates in undo. The other modules (Lungs, Heart, Broca) show
    only the global section.
  - LOW adds only perceptually-transparent cost savers:
    - Tract: reflection-coefficient refresh on a 16-sample cadence with a
      16-sample fade (instead of a fresh target + 1-sample crossfade every
      sample while articulating) — ~90% fewer O(n) divisions during motion,
      ~18% total tract cost at 17.5 cm VTL.
    - Glottis: the LF waveform's exponentials become per-sample multiplies
      (per-period re-derived step factors, exact within float) and the
      aspiration-noise gate sine uses a Bhaskara-I approximation (max error
      ~0.0016, inaudible in a noise-gate modulation) — ~20% glottis cost.
  The always-on fixes (denormal protection, 5 ms VTL CV slew + waveguide
  state resampling on section-count changes, transient-decay
  precomputation, and the settled-shape reshape early-out — while every
  diameter sits bit-exactly on its unchanged target the two O(n) reshape
  loops are a provable no-op and are skipped; ~15% off idle/static tract
  cost in both modes) apply in both modes and do not alter the sound.

## 3. `cyberline-larynx` — Larynx

Fant–Liljencrants glottal flow derivative generator (the PT Glottis math,
reimplemented from the 1985 paper, plus guards and modular control).

### Inputs

| # | Name | Range | Notes |
|---|---|---|---|
| L1 | F0 (V/OCT) | 1 V/oct | base pitch; knob is octave offset |
| L2 | TENS / POLY | 0–10 V → 0–1 | fold tension → `Rd`; register feel (chest↔falsetto). Accepts 3-channel poly cable from Broca (Ch 0: Tension offset, Ch 1: Pressure scale when unpatched, Ch 2: Voicing) |
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
| TENS CV | 1.0 | 0–1 | attenuator trimmer for TENSION CV input |
| PRESSURE | 0.8 | 0–1 | replaces PT's `intensity` envelope; smoothed internally |
| FOLD LENGTH | 14 mm | 2–45 mm | vocal-fold length; allometric F0 scaling per Charlton/Fletcher mammal law (`log10 F0 = −1.43·log10 L(mm) + 4.01` at zero tension), plus mass/dim timbre effect (thicker folds = darker). Human ≈ 13–15 mm, giraffe 41 mm; keeps DRIFT/creak behavior physical at big sizes. **Size range** (knob/module right-click): Full 2–45 mm (as shipped) · Small 2–6 · Medium 5–12 · Large 10–20 · XL 16–45 · Human 3–19 mm — see the band table below; the choice saves with the patch |
| FREQ OCT | −2…+2 | octave offset for L1 (encoder-style knob that snaps to whole octaves; legacy fractional saved values are still rounded by the engine) | F0 clamped 20–2000 Hz after CV; below 20 Hz the LF period grows unbounded (PT silent-state bug) |
| DRIFT | 0.3 | 0–1 | simplex wobble on F0 and tenseness (PT's `autoWobble`/vibrato noise terms), depth scaled |
| VIBRATO RATE | 6 Hz | 0–12 | sinusoid vibrato depth fixed small; CV of pitch comes from patching |
| FEEDBACK GAIN | 0 | 0–1 | gain on L4 injected into the glottal driving pressure (semi-coupling) |
| P-PITCH COUPLE | 0.3 | 0–1 | how much PRESSURE bends F0 (+2–3 Hz per cmH₂O in vivo; scaled to taste) |
| OUT LEVEL | — | — | |

### Size bands (FOLDLEN / VTL presets)

The Larynx `FOLD LENGTH`, Voice `FOLD mm` + `VTL cm`, and OralChamber
`TRACT LENGTH` knobs carry a right-click **Size range** submenu. Selecting a
band narrows the knob's range (and thus its travel) to that animal-size class,
clamps the current value in, and saves the choice with the patch. Full always
equals the shipped range. Voice applies the band to both anatomy knobs at
once. F0 spans below follow the suite's pitch law at tension 0.6.

| Band | FOLDLEN | ≈F0 | VTL | Example animals |
|---|---|---|---|---|
| **Small** | 2–6 mm | 790 Hz–3.8 kHz | 6–12 cm | Mouse (~2 mm), bat, rat (~3 mm), guinea pig, kitten, small dog (chihuahua). Rodent/bird tracts sit at or below the 6 cm floor — pitch them with `OCT` instead |
| **Medium** | 5–12 mm | 293 Hz–1.0 kHz | 10–18 cm | Cat (purr recipe), rabbit, small/medium dog, human child, marmoset; adult-female human at the top edge (11.5 mm moan recipe) |
| **Large** | 10–20 mm | 141–380 Hz | 15–28 cm | Adult humans (female 11.5 mm / male 17 mm recipes), chimpanzee, medium/large dog, young deer |
| **XL** | 16–45 mm | 44–200 Hz | 25–60 cm | Deep baritone (~19–20 mm), sea lion, big dog (great dane), big cats (lion/tiger ~25–33 mm), horse (~30 mm), red-deer stag roar; elephant (without trunk) sits at the VTL ceiling — the recipe's nasal-trunk radiation extends beyond |
| **Human** | 3–19 mm | 152 Hz–2.1 kHz | 7–22 cm | Human newborn (3 mm — cry realism via `OCT −2`, ≈530 Hz), child, adult female, deep bass |

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
| C3 | TNGX / POLY | 0–10 V → position | blade→tip index. Accepts 6- to 9-channel poly cable from Broca (Ch 0: TNG X, Ch 1: TNG Y, Ch 2: VELUM, Ch 3: C.POS, Ch 4: C.WIDTH, Ch 5: FRIC) |
| C4 | TONGUE Y | 0–10 V → diameter | constriction depth (quadratic curve: close, mid, open) |
| C5 | VELUM | 0–10 V → 0–1 | nasal branch opening |
| C6 | CONSTR POS | 0–10 V → position | explicit constriction (teeth/tongue-tip) |
| C7 | CONSTR WIDTH | 0–10 V → width | constriction aperture width |
| C8 | FRICATIVE | 0–10 V → amount | noise gain injected at constriction |
| C9 | INJECT POS | 0–10 V → position | where C2 enters the tube |
| C10 | VTL CV | 0–10 V | modulates vocal tract length (+5.4 cm/V) |

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
| TRACT LENGTH (VTL) | 17.5 cm | section count `n = round(VTL · 2·SR / c)`; range ~6–60 cm (human→deer/koala); recomputed on change, state preserved per-section where possible. **Size range** (knob/module right-click): Full 6–60 cm · Small 6–12 · Medium 10–18 · Large 15–28 · XL 25–60 · Human 7–22 cm — see the band table below; the choice saves with the patch. **The VTL CV hard clamp follows the selected band** (Human ⇒ CV cannot push past 22 cm or below 7 cm), so a narrowed band is a real commitment, not just knob ergonomics; at the Full band the physical 6–60 span applies as before |
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

### Polyphonic Integration
- **TNGX / POLY:** When fed a 9-channel polyphonic cable from `cyberline-broca`'s
  `TRACT POLY` output, Voice auto-maps all 9 parameters across both the tract
  filter and glottal source:
  - Ch 0: Tongue X offset (-5..+5 V)
  - Ch 1: Tongue Y offset (-5..+5 V)
  - Ch 2: Velum opening (0..10 V)
  - Ch 3: Constriction Position (0..10 V)
  - Ch 4: Constriction Width offset (-5..+5 V)
  - Ch 5: Fricative turbulence (0..10 V)
  - Ch 6: Vocal fold tension offset
  - Ch 7: Glottal pressure envelope (0..10 V)
  - Ch 8: Voicing amount (0..10 V: 10 V modal vibration, 0 V voiceless aspiration)
- **TENS / POLY:** Accepts a 3-channel poly cable from Broca's `LARYNX POLY`
  (Ch 0: Tension, Ch 1: Pressure, Ch 2: Voicing).

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
| G1 | TRIGGER | edge | starts one breath (one-shot envelope: inhale runs to DEPTH, exhale begins automatically — no sustained level needed); retrigger mid-exhale = catch-breath |
| G2 | GATE | 0/1 | breath pedal (all modes except PHASOR): rising edge starts a breath; held high = inhale to DEPTH and HOLD (cuts an in-progress exhale); release = exhale from current volume. **In PHASOR mode this input is the phase CV** (0–10 V → 0–1) |
| G3 | DEPTH CV | 0–10 V → 0–1 | modulates tidal volume |
| G4 | SYNC | edge | hard cycle reset (trigger): restarts the breath cycle from its start per the START polarity — Inhale first zeroes volume and begins a fresh inhale from 0 V; Exhale first tops up to full and releases the exhale on the beat |

### Outputs

| # | Name | Notes |
|---|---|---|
| O1 | PRESSURE | 0–10 V (or -5..+10 V with suction); main output, patches directly into Larynx PRESSURE |
| O2 | VOLUME | CV 0–1, current lung volume (the "fuel gauge") |
| O3 | FLOW | signed CV: + during inhale, − during exhale; patchable (noise gates, filters, whistles) |
| O4 | PHASE | gate, high during exhale (voiced air) |

### Parameters

| Knob/switch | Default | Notes |
|---|---|---|
| MODE | CYCLE | CYCLE (free breathing), TRIG (one-shot envelope), CLOSED (air retained after exhale — whale-style recycling), PHASOR (phase-driven; see below) |
| RATE | 12/min | breathing rate in CYCLE mode; scales the baseline expiratory phrase duration |
| DEPTH | 0.7 | tidal volume; in TRIG/CLOSED also sets envelope sustain *duration* (energy-limited) |
| INHALE | 0.4 s | time to fill (quick singer catch-breath to deep yawn; 0.05–5 s). **Drag is tapered** (multiplicative — equal knob travel = equal ratio change), so the short-inhale end (0.05–0.5 s) owns ~half the travel instead of ~9%. **Inhale max** (knob/module right-click): Full 5 s (as shipped) / 2 s / 1 s / 0.5 s — narrows the knob's range (and travel) to your working band; the current value is clamped in, and the choice saves with the patch. In CYCLE mode it is referenced to 12 bpm (5 s period) and scales with the breath period (`inhale = period × INHALE/5 s`, capped at 45%), so the I:E ratio holds when RATE changes; TRIG/CLOSED use absolute seconds; in PHASOR the knob maps to the cycle share (no 45% cap — see below). **The narrowed max applies everywhere the raw seconds are read** — in PHASOR the reachable ratio span shrinks with it (0.5 s max ⇒ 1–10% split) |
| MAX FLOW | 0.4 | airway conductance ceiling; adjusts exhale venting from conserved sustain (0) to rapid puff (1) |
| EFFORT | 0.65 | muscular regulation / appoggio (0 = pure exponential elastic recoil decay, 1 = regulated plateau sustain) |
| INH MUTE | Mute | 2-position switch: Mute (0 V during inhale) vs Suction (-P / negative pressure during inhale gasp) |
| START | Inhale first | 2-position cycle-start polarity. **Inhale first (physio)**: the cycle begins with the silent fill, the voiced exhale follows (default; classic reservoir behavior). **Exhale first (musical)**: the cycle boundary fires the sound — the reservoir is topped up instantly and the exhale begins on the beat, then the inhale recharges at the end of the cycle and holds at the top. Affects CYCLE auto-starts, SYNC resets, and TRIG one-shots (a trigger re-releases a full envelope on the beat); GATE pedal and CLOSED rationing semantics are unchanged; in PHASOR the mapping mirrors so the drain begins at phase 0 (the I:E split still follows the INHALE knob) |

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
- **CYCLE free-run:** RATE sets the breath period (60/RATE seconds) as a
  *floor* — after a breath completes, the module rests until the period
  elapses, then tops up and exhales again, no trigger needed. When the
  natural physics (inhale time + drain law) is slower than the period, the
  natural duration wins; the drain is never accelerated to hit RATE.
- **PHASOR mode (phase-driven):** the GATE input becomes the phase CV
  (0–10 V → 0–1 of one breath cycle). The external phasor owns the *period*;
  the INHALE knob still owns the *ratio*: the inhale occupies
  `period × INHALE/5 s` of the cycle (same 12-bpm reference as CYCLE mode),
  so a 5 s phasor reproduces the CYCLE-mode inhale time exactly and faster
  phasors scale it proportionally. PHASOR is stateless (volume is a pure
  function of the phase — nothing can overrun), so the CYCLE-mode 45% cap
  does not apply: the knob's span maps to 1–98% of the cycle, allowing
  inverse-ratio (inhale-dominant) shapes. The drain shape
  follows EFFORT (exponential at 0, linear plateau at 1); MAX FLOW and RATE
  are ignored. FLOW reports the actual dV/dt (including phase jumps), so its
  magnitude scales with phasor speed. Accepts any unipolar 0–10 V ramp;
  bipolar sources should be offset/attenuated first (e.g. Fundamental 8vert).
  Phase is continuous, so the module tracks external acceleration, deceler-
  ation, and reversal (reversal re-inhales — FLOW flips positive).
- **Trigger vs gate semantics:** TRIGGER is a pure one-shot — a short clock
  pulse suffices; the attack (inhale) runs to DEPTH and the exhale (release)
  begins on its own (attack completion never depends on the trigger still
  being high). GATE is a pedal: while high, the lungs inhale toward DEPTH
  and hold; a high gate also cuts an in-progress exhale (catch-breath), and
  on release the breath is let out from the current volume. Retriggering
  mid-exhale always cuts to a fresh inhale (no artificial smoothing; the
  physics provides the click-free shape).
- Outputs are sample-rate-smoothed CV (block-rate physics, per-sample output
  interpolation). NaN guards per suite conventions.
- Non-feature (deliberate): no per-stage linear/log curve menus; curve
  character is EFFORT/DEPTH physics.

## 5c. `cyberline-heart` — Heart (status: IMPLEMENTED)

Time-varying elastance cardiac pump (Suga–Sagawa family): atrium → ventricle
→ arterial Windkessel. The Wiggers cycle, Frank–Starling law, S1/S2 sounds
and murmurs are all emergent from four rules — no per-phase envelopes:

1. `P = E(t)·V` per chamber; `E(t)` is a normalized two-exponential
   contraction twitch (fast rise `r`, slow decay `d`, both ∝ beat period)
   fired by the activation event.
2. Valves are pressure comparisons: inflow open iff `P_atrium > P_ventricle`,
   outflow open iff `P_ventricle > P_artery`. Valve-closure edges fire S1/S2.
3. The artery is a Windkessel reservoir: fills during ejection, exponential
   runoff toward the diastolic baseline between beats.
4. Activation: the SA node free-runs at the intrinsic rate; an external
   pacer overdrives it (each trigger resets the SA clock); when pacing
   stops the intrinsic rhythm returns (escape beats). No mode switch.

The heart is an intrinsic LFO — that is the physical justification for the
module also being a musical clock/ENVELOPE source: the same beat that
schedules the ECG shapes every other output.

### Inputs

| # | Name | Range | Notes |
|---|---|---|---|
| H1 | PACING | trigger | external pacer spike; overdrives the SA node; also the one-shot trigger |
| H2 | FILL CV | 0–10 V | added to the FILL knob (venous return / preload) |
| H3 | DRIVE CV | −5..+5 V | added to the DRIVE knob (autonomic tone) |
| H4 | AFTERLOAD CV | 0–10 V | added to the AFTERLOAD knob |
| H5 | SYNC | trigger | hard cycle reset (trigger): instantaneously resets the cardiac activation clock |

### Outputs

| # | Name | Notes |
|---|---|---|
| O1 | VOLUME | 0–10 V, ventricular volume: slow fill → isovolumetric freeze → ejection decay → freeze (main envelope) |
| O2 | PRESSURE | 0–10 V, ventricular pressure: climbs while volume is frozen (IVC) — the complementary envelope |
| O3 | FLOW | signed ±5 V, suite FLOW convention: **+ filling, − ejection** (matches Lungs: inflow +, outflow −). ± glyph + sign LED rule applies |
| O4 | OUTFLOW | 0–5 V unipolar aortic flow pulse; tracks instantaneous flow (integral over the beat = stroke volume). **The series-link jack**: Heart A OUTFLOW → Heart B FILL CV (optionally through a Lungs VOLUME as the pulmonary reservoir) closes the circulation loop — the two hearts self-balance over beats via Frank–Starling, no hidden rules |
| O5 | ARTERIAL | 0–10 V Windkessel staircase: rises during ejection, exponential runoff — "sustain-then-decay" envelope |
| O6 | ECG | bipolar ±5 V, stylized P-QRS-T (amplitude ratios R≈3·T≈6·P); generated **per sample** from event ages (control-rate interpolation would quantize the QRS at high rates). Widths scale with beat period. Stylized-human waveform by design; species ECG tables are a v2 idea |
| O7 | BEAT | 5 V trigger pulse at ventricular activation (QRS) — the master clock |
| O8 | ATRIA | 5 V trigger pulse at atrial activation (P wave) — BEAT's swing partner, offset by AV DELAY |
| O9 | SOUND | ±5 V audio, phonocardiogram: S1 (AV valve closure, low), S2 (aortic closure, higher/shorter), S3 (rapid passive fill, compliant hearts), S4 (atrial kick into a stiff ventricle), plus murmur noise gated by \|flow\| (stenosis) or pressure-behind-a-leaking-valve (regurgitation). Thump pitch ∝ M^(−1/3) — mass knob = drum size, physically |

### Parameters

| Knob | Default | Range | Notes |
|---|---|---|---|
| HEART MASS | 0.74 (70 kg) | log 5 g–2000 kg | allometric master: rate ∝ M^−0.25 (mouse ~600 bpm, elephant ~30), stroke volume ∝ M, AV delay ∝ M^+0.25, S1/S2 pitch ∝ M^−1/3. Display in kg. Human = 70 kg reference; systemic pressure is size-invariant, so PRESSURE/ARTERIAL ranges do not rescale |
| RATE | 72 | 30–220 bpm | intrinsic SA rate at human anatomy; mass/autonomy/HRV multiply it; final clamp 25–1000 bpm (32-sample physics block is the ~1000 bpm wall) |
| AV DELAY | 0.15 s | 0.06–0.25 | PR interval — the built-in swing between ATRIA and BEAT; mass-scaled, auto-clamped ≤30% of the current cycle (sympathetic conduction speedup — protects the atrial kick at high rates) |
| FILL | 0.5 | 0–1 | venous return (preload) — Frank–Starling: more fill → harder beat, emergent |
| DRIVE | 0 | −1..+1 | autonomic tone: + sympathetic (rate + contractility up, τ ≈ 3 s lag), − vagal (rate down, fast τ ≈ 0.35 s). The asymmetry IS the physiology |
| AFTERLOAD | 0.4 | 0–1 | arterial resistance: slows ejection, raises the arterial baseline |
| CONTRACTILITY | 0.6 | 0–1 | twitch amplitude |
| COMPLIANCE | 0.5 | 0–1 | ventricular wall stiffness inverse: low = stiff (small fills, S4), high = soft (big fills, S3) |
| VALVE | 0 | −1..+1 | bipolar: − stenotic (tight valves, systolic/diastolic flow murmurs), + regurgitant (leaky closed valves, backflow volume loss — arterial diastolic pressure drops, audible) |
| HRV | 0.15 | 0–1 | heart-rate variability: bounded per-beat rate wander (1/f-ish), recomputed each beat — perturbs the interval length only, nothing integrates; exactly periodic at 0 |
| IRRITABILITY | 0 | 0–1 | off at 0; above 0, occasional premature ectopic beat (SA timer NOT reset; a refractory window blocks stacked ectopics and drops the too-early sinus impulse → compensatory pause emerges) with a stronger post-ectopic beat (extra filling → Starling accent, free musical accent) |
| SOUND LEVEL | 0.7 | 0–1 | phonocardiogram output level |

### DSP notes

- State: three integrators (atrium, ventricle, artery) + valve states +
  event ages `tA/tV`. State floor 0, no hidden constants. NaN guard:
  reinitialize and continue (suite §2).
- Physics at 32-sample cadence; VOLUME/PRESSURE/FLOW/OUTFLOW/ARTERIAL
  interpolated per sample; ECG/phonocardiogram/triggers generated per
  sample. Pacing edge detection is per sample (short triggers must not
  fall between physics ticks).
- Systole fraction ≈ 0.30 of the cycle and shrinks as rate rises (Bazett-like),
  emergent from the twitch time-constants being ∝ period.
- Rates near 1000 bpm (hummingbird) work but are the practical ceiling:
  a 60 ms cycle is ~85 physics ticks — fine, documented as the wall.
- Rate-units bug note (fixed during implementation): the rate clamp is in
  bpm, not Hz — the harness catches it (a Hz-clamped heart beats at
  16.7 bpm regardless of knobs).

### States & presets

Human states are DRIVE/FILL/CONTRACTILITY configurations (rate *emerges* —
presets of the causes, not the effects), shipped as Rack context-menu
presets: deep sleep (~50 bpm, strong vagal, high HRV), relaxed (~65),
attentive (~85), light exercise (~110), heavy exercise (~170: strong
sympathetic, FILL + CONTRACTILITY maxed, AFTERLOAD dropped). Presets
configure organs; patches configure bodies — the two-heart loop and
Heart↔Lungs cardiorespiratory coupling (Lungs FLOW → DRIVE: respiratory
sinus arrhythmia, physically real) are documented cable recipes, not
preset data.

### Dual-Heart Circulation Loop & Frank–Starling Synchronization

The Frank–Starling law dictates that stroke volume increases with preload: when
greater venous return fills the ventricle, the next systolic contraction is
proportionately stronger and ejects more volume. In `cyberline-heart`, this law
is fully emergent from the time-varying elastance physics ($P = E(t) \cdot V$).

To model a complete closed circulatory loop (Right Heart / pulmonary circuit
and Left Heart / systemic circuit) and observe self-balancing Frank–Starling
dynamics between two hearts:

```
[Heart A: Right Ventricle]
    OUTFLOW (O4) ────────────▶ FILL CV (H2) [Heart B: Left Ventricle]
    FILL CV (H2) ◀──────────── OUTFLOW (O4)
```

1. **Patching the Loop:**
   - Connect **Heart A `OUTFLOW`** to **Heart B `FILL CV`**.
   - Connect **Heart B `OUTFLOW`** to **Heart A `FILL CV`**.
   - *(Optional pulmonary bed buffering)*: Route one leg through `cyberline-lungs`
     (`OUTFLOW` → `DEPTH CV`, `VOLUME` → `FILL CV`) to model thoracic pump
     modulation of venous return during breathing.
2. **Observing on Scope (`FILL LVL` / `OUTFLOW` / `PRESSURE`):**
   - **Baseline:** Set both hearts to ~72 bpm, `FILL` ~0.3–0.4, and turn `HRV` to
     0 so intrinsic rate wander does not mask the hemodynamic transients. The
     two hearts alternate ejections and establish a stable reciprocal rhythm.
   - **Preload Transient:** Increase Heart A's `FILL` knob. On the next beat,
     Heart A's end-diastolic `FILL LVL` and systolic `OUTFLOW` swell. When that
     enlarged bolus arrives at Heart B's `FILL CV`, Heart B's ventricle stretches,
     triggering a compensatory increase in Heart B's next stroke volume. Within
     1–3 beats, the circulating volume equilibrates across both pumps.
   - **Afterload Resistance:** Increase Heart A's `AFTERLOAD`. Heart A ejects less
     volume against the higher arterial pressure, starving Heart B's venous
     return. Heart B's contractions naturally weaken until both sides settle into
     a new throttled equilibrium.
   - **Asynchronous Drift:** Setting mismatched rates (e.g. 70 vs 76 bpm) creates
     rhythmic swelling and ebbing of stroke volumes (Starling accents) as ejection
     pulses drift into and out of alignment with the opposing ventricle's filling
     window.

### Non-features (deliberate)

- No per-phase envelope menus — the Wiggers shapes come from the physics.
- No four-chamber split in v1: one generalized pump; the two-heart loop
  patch stands in for the pulmonary/systemic split (see OUTFLOW).
- Reptile shunt, insect dorsal vessel (peristaltic, separate module),
  species ECG tables, named arrhythmias (AFib/PVC/heart-block): parked v2.

## 5d. `cyberline-broca` — Broca (Broca-Brika Articulatory Cortex)

Articulatory motor cortex & phoneme generator. Orchestrates anatomical targets
for the vocal tract filter and glottal source by morphing between cardinal
vowels and triggering consonant onsets across two polyphonic buses (`TRACT POLY`
and `LARYNX POLY`).

```
           ┌────────────────────── cyberline-broca ───────────────────────┐
           │                                                              │
[TRIG]────▶│ Syllable Onset Generator                                     │
[SYNC]────▶│ (Consonant closure ──▶ Plosive burst ──▶ Vowel release)      │
           │                                                              │
           │  • 12 Cardinal Vowels (A/B interpolation via V.MORPH)        │
           │  • 21 Consonant Targets (Stops, Fricatives, Nasals, Glides)  │
           │                                                              │
           └──────┬────────────────────────┬─────────────┬────────────┬───┘
                  │ 9-channel              │ 3-channel   │            │
                  ▼                        ▼             ▼            ▼
             [TRACT POLY]            [LARYNX POLY]    [CV.ENV]     [BURST]
                  │                        │
                  ├─▶ Oral Chamber         └─▶ Larynx
                  │   (Ch 1-6 tract)           (Ch 1-3 source)
                  ▼
                Voice
                (Ch 1-9 single cable)
```

### Parameters

| Knob | Default | Range | Notes |
|---|---|---|---|
| VOWEL A | EE (/i/) | 0–11 | selects primary vowel target |
| V.MORPH | 0.0 | 0–1 | smooth continuous crossfade from Vowel A to Vowel B |
| VOWEL B | OO (/u/) | 0–11 | selects secondary vowel target |
| CONSONANT | S (/s/) | 0–20 | selects consonant target across 4 manner classes |
| C/V MORPH | 0.0 | 0–1 | consonant vs vowel mix (0 = vowel dominant, 1 = consonant held) |
| ONSET | 0.08 s | 0.01–0.5 s | consonant attack / release transition duration |

### Inputs

| # | Name | Range | Notes |
|---|---|---|---|
| B1 | SYNC | trigger | hard cycle reset: resets syllable cycle and cyclical scan step to 0 |
| B2 | TRIG | trigger | fires consonant onset → vowel release; when SYNC is patched and VOWEL CV unpatched, steps cyclically through vowel table |
| B3 | VOWEL A CV | ±5 V / 0–10 V | CV scan through the 12 cardinal vowels |
| B4 | V.MRPH CV | 0–10 V | CV modulation of VOWEL A/B morph position |
| B5 | VOWEL B CV | ±5 V / 0–10 V | CV scan through the secondary vowel target |
| B6 | CONS CV | ±5 V / 0–10 V | CV scan through the 21 consonants |
| B7 | C/V CV | 0–10 V | CV modulation of consonant/vowel balance |

### Outputs

| # | Name | Notes |
|---|---|---|
| O1 | TRACT POLY | 9-channel polyphonic bus: drives `cyberline-oral-chamber` (Ch 1–6) or `cyberline-voice` (Ch 1–9) |
| O2 | LARYNX POLY | 3-channel polyphonic bus: drives `cyberline-larynx` (Ch 1–3) |
| O3 | CV.ENV | mono 0–10 V CV envelope reflecting current consonant-to-vowel blend |
| O4 | BURST | mono 0–10 V gate firing during plosive release and frication |

### Polyphonic Bus Channel Layout

#### `TRACT POLY` (9 Channels)
- **Ch 0: Tongue X** (-5..+5 V offset relative to 5.0 neutral).
- **Ch 1: Tongue Y** (-5..+5 V offset relative to 5.0 neutral).
- **Ch 2: Velum** (0..10 V nasal port opening; 10 V on /m/, /n/, /ŋ/).
- **Ch 3: Constriction Pos** (0..10 V; teeth, alveolar ridge, palate, velum).
- **Ch 4: Constriction Width** (-5..+5 V offset relative to 5.0 neutral; narrows to -5 V for stop closure).
- **Ch 5: Fricative** (0..10 V turbulence injection level).
- **Ch 6: Tension** (0 V offset; preserves receiver's knob setting).
- **Ch 7: Pressure** (0..10 V glottal driving pressure envelope).
- **Ch 8: Voicing** (10 V voiced modal vibration, 0 V voiceless aspiration).

#### `LARYNX POLY` (3 Channels)
- **Ch 0: Tension** (0 V offset).
- **Ch 1: Pressure** (0..10 V envelope).
- **Ch 2: Voicing** (10 V voiced, 0 V voiceless).

### Cardinal Vowel Table (12 Vowels)

| Code | IPA | Tongue X | Tongue Y | Lip Width | Lip Pos | Description |
|---|---|---|---|---|---|---|
| EE | /i/ | 8.5 | 2.1 | 10.0 | 0.0 | Close front unrounded (fleece) |
| IH | /ɪ/ | 7.5 | 3.5 | 10.0 | 0.0 | Near-close near-front (kit) |
| EH | /ɛ/ | 6.8 | 5.0 | 10.0 | 0.0 | Open-mid front unrounded (dress) |
| AE | /æ/ | 6.2 | 7.5 | 10.0 | 0.0 | Near-open front unrounded (trap) |
| AH | /ɑ/ | 3.5 | 9.0 | 10.0 | 0.0 | Open back unrounded (palm / father) |
| AW | /ɔ/ | 2.8 | 6.8 | 6.5 | 9.5 | Open-mid back rounded (thought) |
| OH | /o/ | 2.5 | 4.5 | 6.0 | 9.5 | Close-mid back rounded (goat) |
| OO | /u/ | 3.0 | 2.1 | 5.5 | 9.5 | Close back rounded (goose) |
| UH | /ʊ/ | 4.2 | 3.8 | 7.0 | 9.5 | Near-close near-back rounded (foot) |
| ER | /ɜːr/| 5.0 | 4.2 | 9.0 | 0.0 | Open-mid central rhotacized (nurse) |
| OE | /œ/ | 6.5 | 5.2 | 6.5 | 9.5 | Open-mid front rounded (French 'peur') |
| UE | /y/ | 8.2 | 2.2 | 5.8 | 9.5 | Close front rounded (German 'über') |

### Consonant Targets & Manners (21 Consonants)

- **Stops:** `/p/` (bilabial), `/b/` (voiced bilabial), `/t/` (alveolar), `/d/` (voiced alveolar), `/k/` (velar), `/g/` (voiced velar), `/ʔ/` (glottal stop).
  - Feature: Complete constriction closure (`constrWidth = 0`), fast slew (~4 ms) into plosive release, plosive `BURST` gate active. Voiceless stops cut `voicing` to 0.
- **Fricatives:** `/f/` (labiodental), `/v/` (voiced labiodental), `/θ/` (dental 'th'), `/s/` (alveolar), `/z/` (voiced alveolar), `/ʃ/` ('sh'), `/h/` (glottal fricative).
  - Feature: Narrow constriction aperture, high `fricative` turbulence injection, plosive/frication `BURST` output active. Voiceless fricatives drop `voicing` to 0, sustaining modal breath turbulence without fold pulses.
- **Nasals:** `/m/` (bilabial), `/n/` (alveolar), `/ŋ/` (velar 'ng').
  - Feature: Oral closure combined with full `velum` port opening (1.0), radiating resonant sound through the nasal tract.
- **Glides / Approximants:** `/w/` (labiovelar), `/l/` (lateral), `/r/` (retroflex), `/j/` (palatal 'y').
  - Feature: Open constriction aperture, full modal voicing, smooth vocalic transitions.

## 5d-bis. `cyberline-broca-sq` — Broca² (dual phoneme Turing machine)

Two rotating phoneme registers — a **vowel register** (Broca's 12 vowels) and a
**consonant register** (Broca's 21 consonants, index 0 = NONE/rest) — scanned
like a shift-register "Turing machine": each step the read pair fires Broca's
consonant→vowel onset gesture, and each register independently rolls its
mutation die (`MUT-V` / `MUT-C` + P CVs); on a hit, the slot just read is
rewritten with a random phoneme before playback continues, so loops gradually
erode into new material. Independent lengths (1–16, `V LEN` / `C LEN` + shared
`LEN` CV) make the pair polymetric.

- **Clocking (MODE switch):**
  - **FREE:** internal `RATE` clock (0.5–20 steps/s), shared step counter.
  - **CLOCK:** external trigger advances the shared step; falls back to the internal clock when unpatched.
  - **PHASOR:** independent `V-PH` / `C-PH` phase CVs (0–10 V unipolar ramp convention, same as Breath PHASOR). Each register's position is a *pure function* of its phase (`floor(phase·len)`) — stateless, so forward and backward phasors both scan (backward reads the register in reverse), and fast jumps *snap* to the new step with the normal morph old→new. Unpatched phasor follows the other's; both unpatched → internal `RATE` phasor. A register sitting inside one step makes no gesture (silence by phase).
- **Mutation:** per step event, write-current-slot; consonant rolls may land on NONE (rests → open-vowel syllables). Context menu: *Initialize registers* (musical CV default fill) / *Randomize registers*. `RESET` zeroes both heads without touching register contents.
- **MORPH:** articulator slew between consecutive phonemes — 0 % = Broca's crisp defaults (~4.5 ms stops / ~14 ms vowels), 100 % = heavy blend (half-formed speech).
- **Display:** 4×4 register grid; each cell shows the slot's vowel code (top, cyan lane) and consonant code (bottom, amber lane, dim dash = NONE). Cyan/amber head markers move together in FREE/CLOCK and independently in PHASOR. Slots past a register's length dim that lane; slots past both are near-dark (unplayed).
- **I/O:** `TRACT POLY` (9-ch) and `LARYNX POLY` (3-ch) use *identical channel maps to Broca* — a drop-in swap in the Larynx/OralChamber/Voice chain. Plus `CV.ENV`, `BURST`, `GATE` (syllable gesture; fires on every step including rests), `VOW` / `CONS` index CVs (0–10 V over each table).

## 5e. `cyberline-visual1` — Visual1 (audio→image visualizer)

Not an anatomical module — a standalone experiment in direct
frequency-domain-to-frequency-domain mapping. The image is treated as a
grid of 8×8 JPEG-style DCT blocks; the audio FFT magnitude spectrum
(63 log-spaced bands, 35 Hz–16 kHz) is injected into the blocks' 2D DCT
coefficients by **zigzag position** (audio band *k* → AC coefficient at
zigzag slot *k*+1), then every block is inverse-DCTed into RGBA pixels
and blitted as a NanoVG texture (`nvgCreateImageRGBA`/`nvgUpdateImage`).

### Inputs

| # | Name | Range | Notes |
|---|---|---|---|
| V1 | L | audio | left/mono analysis input |
| V2 | R | audio | right analysis input; unpatched follows L (mono renders full image) |

### Outputs

| # | Name | Notes |
|---|---|---|
| V1 | FRAME | 5 V trigger pulse (~2 ms) each time a new image frame lands (~86 Hz at 44.1 k, 512-sample hop) — clock for slaving other modules to the visualizer's frame cadence |
| V2 | ACT | 0–10 V CV of the mean band envelope (loudness proxy) |

### Parameters

| Knob | Default | Range | Notes |
|---|---|---|---|
| GAIN | 0.5 | 0–1 | input sensitivity (maps gain 3..40 onto FFT magnitudes; dB window −66..−18) |
| SPREAD | 0.65 | 0–1 | how deep into the 63-slot zigzag the spectrum reaches ("spectral spread" of the image) |
| DECAY | 0.4 | 0–1 | band envelope release (τ ≈ 50 ms–4 s) — phosphor-like persistence of spectral events |
| MODE | MOSAIC | 3-pos | MOSAIC (all blocks share the spectrum, palette-tinted DCT texture), PHASE (deterministic per-block band offsets + ±1 signs: the same spectrum scrambles into evolving mosaics), PERTURB (see below) |
| STEREO | SUM | 3-pos | SUM (average L+R spectra), INTLV (even zigzag slots ← L, odd ← R), SPLIT (left image half ← L, right ← R; also tints the halves cyan/coral) |

### PERTURB mode

Loads an image (context menu: *Load image* → bundled `res/visual1-demo.jpg`
or any user JPEG/PNG via the host file dialog — decoded once with
stb_image, never at DSP time), level-shifts it to ±1 and forward-DCTs it
into coefficient space. Every rendered frame then multiplies each block's
AC coefficients by `1 + 1.2·band_env(slot)` plus a small hashed additive
shimmer (`0.3·env`) so even low-texture photos visibly ripple. Silence
returns the original image exactly (factor 1): the picture "breathes"
with the music and relaxes when it stops.

### DSP notes

- 2048-sample Hann window, 512-sample hop, own radix-2 FFT (rack-free
  core, `tests/render_visual1.cpp` verifies DCT round-trip, band
  placement, decay, and perturb round-trip headlessly — PPM dumps in
  /tmp for eyeballing).
- Analysis runs on hop boundaries inside `processSample()`; the widget
  thread only blits the latest complete frame (double-buffered, atomic
  index — no locks in the audio path).
- 256×192 output = 768 blocks × 64 coefficients ≈ 12M MACs per frame at
  ~86 Hz — comfortably real-time.
- Band placement is bin-quantized at low frequencies (44.1 k bins are
  ~21.5 Hz wide; the log band grid below ~650 Hz shares bins). The test
  harness documents the expected containment band, not the Hz-exact one.
- NaN guards on all spectrum paths; DC coefficient carries overall
  loudness so the image never collapses to black while audio plays.


## 5f. `cyberline-sagitta` — Sagitta (vocal-system fluoroscopy)

STATUS: implemented but unregistered from the browser (graphics revisit
later); code and tests kept in tree. Note: saved patches containing a
Sagitta instance fail to load (blocking modal) until the instance is
removed or the model is re-registered.

Display-only side-view ("sagittal") anatomical diagram of the
lungs → larynx → oral chamber pipeline. Not in the audio path: it taps
the same CV contracts the real consumers use, in parallel:

| Input | Contract |
|---|---|
| TRACT | 6–9 ch poly: TNGX, TNGY, VEL, CPOS, CWID, FRIC[, TENS, PRESS, VOIC] |
| LARYNX | 3 ch poly: TENS, PRESS, VOIC (used when TRACT carries < 9 ch) |
| PRES / VOL / FLOW | Lungs PRESSURE (−5..+10 V), VOLUME (0..10 V), FLOW (signed ±5 V) |
| VOCD | Larynx VOICED audio — drives the replica source + F0 tracking |

The CV *mode* switch selects BUS (Broca's sink-default-offset encoding —
tongue/constriction channels are offsets around the 5.0/5.0/0/5.0/0
knobs, velum/fricative absolute 0-10 V) or DIRECT (channels are absolute
0-10 V parameters). GAIN scales the voiced input.

### What it runs

`CyberlineSagittaCore` (rack-free, like the other cores) decodes the CVs
exactly as Oral Chamber / Larynx do, then feeds a **replica
`CyberlineTract` waveguide** (fixed 17.5 cm geometry) with the voiced
audio as glottal source. Consequences:

- the drawn tongue profile, velum flap and constriction are the model's
  actual per-section diameters — the constriction ring marks the true
  narrowest section of the waveguide, not a knob icon;
- the amber band inside the cavity is the tract's own sound-pressure
  meters (`amplitude[]`), so the glow literally is the wave traveling
  toward the lips;
- the nasal branch is shaped by the decoded velum, with its own meters
  glowing during nasals;
- F0 for the fold-wobble animation is measured from the patched voiced
  audio (hysteresis zero-crossing tracker), tension colors the folds
  cyan → magenta, subglottal pressure glows under them;
- Lungs VOLUME inflates the lungs + diaphragm, FLOW drives sign-colored
  airflow particles along the trachea (cyan inhale, coral exhale; nasal
  route when the velum opens).

### Exaggerations

Big cartoon eye that glances toward the constriction (or its own breath),
brow raise with fricative effort, cheek/throat puff under pressure behind
an occlusion, blink cycle. HUD column with F0 / VOI / TEN / PRS / VEL /
LNG / FLW / FRC bars and TNG/CNS/SND readouts.

### DSP notes

- Snapshot double-buffer + atomic publish at a 32-sample control cadence;
  the widget thread only reads completed snapshots (no locks in the
  audio path).
- Section→screen mapping is piecewise with a knot at the velopharynx
  bend (`noseStart` ≈ 73% of the tract) — the ascending pharynx IS most
  of the model tract, and the drawing respects that.
- `tests/render_sagitta.cpp` verifies the decode (BUS/DIRECT), velum,
  constriction placement, F0 tracking, lungs mapping and finite-state
  under garbage headlessly (7 checks).


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

## 9. Bioacoustics & Animal Vocalisation

Because `cyberline-larynx` models vocal fold physics with mammal allometric scaling (`FOLDLEN`: 2–45 mm, $\log_{10}(F_0) = -1.43 \cdot \log_{10}(L) + 4.01$) and `cyberline-oral-chamber` models acoustic wave propagation with scalable tube geometry (`VTL`: 6–60 cm), the suite naturally synthesizes a wide taxonomy of bioacoustic vocalisations:
- **Terrestrial Mammals:** Cat 24–28 Hz purring tremor and vowel glides (meow); elephant 15–25 Hz infrasonic rumbles (nasal trunk radiation) and high-pressure trumpets; primate cyclic pant-hoots and jaw chattering; human non-speech vocalisations in both adult registers (female moan — breathy low-tension phonation with strain-coupled pitch rise; male chest groan with creak/falsetto registers).
- **Avian / Bird Calls:** Songbird agile chirps and trills (2–5 kHz downward sweeps); dove/owl wooden coos (closed beak, elongated tracheal resonance, 6 Hz tremolo); crow/gull raucous squawks (feedback back-pressure chaos).
- **Marine Mammals:** Humpback whale wandering frequency-modulated glides (massive laryngeal sac resonance); dolphin high-frequency signature whistles (3–8 kHz FM sweeps); California sea lion staccato guttural barks (2.5 Hz cadence with husky throat growl).

See [`docs/animal_vocalisations.md`](animal_vocalisations.md) for full patch diagrams, parameter charts, and modulation guides.

