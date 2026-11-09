# cyberline

Physical-modelling synthesis for VCV Rack / Cardinal, built from patchable
atoms: a LF-glottis **Larynx**, a Kelly–Lochbaum **Oral Chamber** with
nasal branch and variable tract length (species-ready), a fused **Voice**
module, a breath-pressure **Breath** reservoir, and an elastance **Heart**
(SA-node clock, Wiggers-cycle envelopes, ECG/beats, phonocardiogram, mass
allometry from shrew to elephant). MIT licensed.

- Spec: `docs/spec.md` (v0.1 draft — the authoritative plan)
- Heart manual: `docs/heart.md` — outputs-as-instruments, knob anatomy,
  patch recipes (two-heart circulation loop, cardiorespiratory coupling),
  human-state presets, species quick reference
- Animal vocalisation guide: `docs/animal_vocalisations.md` — allometric
  scaling laws, multi-species bioacoustics (mammals, birds, marine mammals)
- Speech prosody & articulation guide: `docs/speech_prosody_and_articulation.md` —
  prosody ($F_0$ contours), timing non-isochrony, syllable structure, and patch recipes
  for realistic speech with Broca/Broca²
- Reference collection (external, not part of this repo):
  `../refs/` — papers, MIT implementations, theory notes
