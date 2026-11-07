# Animal Vocalisation & Bioacoustic Patch Guide

The `cyberline` voice synthesis pipeline—[`CyberlineLungs`](../src/CyberlineLungs.hpp), [`CyberlineLarynx`](../src/CyberlineLarynx.hpp), and [`CyberlineOralChamber`](../src/CyberlineOralChamber.hpp)—is built on mammalian acoustic physics, allometric scaling, and Kelly-Lochbaum waveguide dynamics. Rather than relying on static formant filters or pre-recorded samples, sounds are synthesized from physical models of the respiratory tract, vibrating membranes, and acoustic scattering waveguides.

> [!NOTE]
> These recipes are **untested starting points**. Pitch values were computed from the Larynx's real pitch law: `F0 = 10^(4.01 − 1.43·log10(FOLDLEN_mm)) × 2^(PITCH_V + OCT) × sqrt(TENSION / 0.6)`, plus up to `PCOUPLE × 30 Hz` from pressure. Expect to tune by ear.

This guide details how to configure and modulate the modules to simulate vocalisations across the mammalian range, including terrestrial mammals (cats, elephants, primates, and humans — including non-speech human vocalisations), birds (songbirds, doves/owls, corvids), and marine mammals (whales, dolphins, sea lions).

---

## 1. Bioacoustic Principles in Cyberline

### Allometric Scaling of the Glottal Source
In mammalian biology, fundamental frequency ($F_0$) scales inversely with vocal fold length ($L_{\text{fold}}$) according to the interspecific mammalian allometric power law:

$$\log_{10}(F_0) = -1.43 \cdot \log_{10}(L_{\text{fold}}) + 4.01$$

This equation is implemented directly in [`CyberlineLarynx`](../src/CyberlineLarynx.hpp) via the **`FOLDLEN`** parameter:
Base F0 below is at `OCT 0`, tension 0.6, no pitch CV; each `OCT` step or 1 V of `PITCH` doubles it:
- **2 mm – 4 mm:** Songbirds, dolphins, rodents ($F_0 \approx 1400\text{–}3800\text{ Hz}$).
- **7 mm – 10 mm:** Cats, primates, doves, crows ($F_0 \approx 380\text{–}630\text{ Hz}$).
- **12 mm – 18 mm:** Humans, sea lions, wolves ($F_0 \approx 160\text{–}290\text{ Hz}$).
- **30 mm – 45 mm:** Large carnivores, elephants, whales ($F_0 \approx 44\text{–}79\text{ Hz}$; use `OCT` to go lower or higher).

### Formant Scaling via Vocal Tract Length (VTL)
Formant frequencies $F_n$ are determined by the quarter-wave resonance frequencies of the vocal tract tube:

$$F_n = \frac{(2n - 1) \cdot c}{4 \cdot \text{VTL}}$$

where $c \approx 35000\text{ cm/s}$ is the speed of sound in warm moist air. [`CyberlineOralChamber`](../src/CyberlineOralChamber.hpp) supports a **`VTL`** range from **6 cm** up to **60 cm** (further extensible with $\pm 10\text{ V}$ CV at $5.4\text{ cm/V}$):
- **6 cm – 9 cm:** High, bright infant/small animal formants ($F_1 \sim 1000\text{–}1400\text{ Hz}$, $F_2 \sim 3000\text{–}4500\text{ Hz}$).
- **12 cm – 18 cm:** Medium mammals, canines, primates, humans ($F_1 \sim 250\text{–}850\text{ Hz}$, $F_2 \sim 800\text{–}2400\text{ Hz}$).
- **45 cm – 60 cm:** Giant mammal cavities, baleen whale laryngeal sacs, elephant trunks ($F_1 \sim 150\text{ Hz}$, dense formant clustering).

### Nasal Shunt (Velum)
Opening the velum (**`VELUM`** knob or CV) routes acoustic energy into the secondary waveguide branch. This is essential for:
- Nasal snorts, whines, and closed-mouth hums.
- Elephant trunk acoustics (`NOSE_OUTPUT`).
- Cat purrs and mouth-closed murmurs.

---

## 2. Modulation Needs: Internal vs. External

- **Internal to Cyberline:**
  - `CyberlineLungs` provides the natural respiratory pressure, airflow direction, and chest volume.
  - `CyberlineLarynx` provides pitch-pressure coupling (`PCOUPLE`), organic pitch drift (`DRIFT`), and vibrato (`VIBRATE`).
- **When External Modulation is Recommended:**
  - **Audio-rate / High-rate LFOs (20–30 Hz):** Required for active neuromuscular laryngeal tremor (such as a cat's purr).
  - **Function Generators / Envelopes:** Needed for fast ballistic sweeps (chirps, whistles, screeches, sudden trumpets).
  - **Stepped Random / S&H:** Adds natural biological variation across consecutive calls.

---

## 3. Terrestrial Mammal Patch Recipes

### 1. Cat (*Felis catus*)

Cats possess two primary phonation mechanisms: vocal fold vibration with dynamic vocal tract filtering (meow/hiss) and intrinsic laryngeal muscle oscillation (purr).

```
                          [ 24-28 Hz LFO ]
                                 │
                                 ▼ (VCA gated by Lungs FLOW)
  [ Lungs ] ──PRESSURE──▶ [ Larynx ] ──VOICED──▶ [ Oral Chamber ] ──▶ LIPS / NOSE
             ──VOLUME───▶  (FOLDLEN: 7.5mm)       (VTL: 8cm)
```

#### A. Purr (20–28 Hz Neuromuscular Tremor)
A cat's purr is produced by rapid twitching of the laryngeal muscles alternating between opening and closing the glottis during both inhalation and exhalation.

* **Module Settings:**
  * **`CyberlineLarynx`:** `FOLDLEN`: `7.5 mm` (≈400 Hz carrier at tension 0.3), `OCT`: `0`, `TENSION`: `0.30`, `DRIFT`: `0.35`, `PCOUPLE`: `0.2`.
  * **`CyberlineOralChamber`:** `VTL`: `8.0 cm`, `TONGUEX`: `4.0`, `TONGUEY`: `7.5` (mouth mostly closed), `VELUM`: `0.4`.
* **Patch:**
  1. Patch an external LFO set to a sine or rectified wave at **24–28 Hz** into a VCA.
  2. Modulate the VCA level using `CyberlineLungs` `VOLUME` or `std::abs(FLOW)` so the purr continues during both inhale and exhale.
  3. Route the pulsed 25 Hz signal into `Larynx` **`PRESSURE`** and **`TENSION`** inputs.
  4. Connect `Larynx` `VOICED` to `Oral Chamber` `SOURCE`.
  5. Mix both `LIPS_OUTPUT` and `NOSE_OUTPUT`.

#### B. Meow (Acoustic Vowel Glide /m-i-a-u/)
* **Module Settings:** `CyberlineLarynx` `FOLDLEN` = `7.5 mm`, `OCT` = `0`, `TENSION` = `0.55` (≈550 Hz base). `CyberlineOralChamber` `VTL` = `8.5 cm`.
* **Patch:**
  1. Trigger an attack-decay envelope (Attack ~80 ms, Decay ~450 ms).
  2. Patch the envelope into `Larynx` **`PITCH`** (+0.6 V peak: ≈830 Hz falling to ≈550 Hz).
  3. Patch the envelope into `Oral Chamber` **`TONGUEX`** configured to sweep from front vowel (high voltage $\approx 8\text{ V}$, /i/) to back vowel (low voltage $\approx 2\text{ V}$, /u/).
  4. Patch an inverted short decay envelope into **`VELUM`** so the call begins with a closed-mouth nasal hum (/m-/) before releasing into the vowel.

#### C. Hiss
* **Module Settings:** Turn `Larynx` `PRESSURE` to 0. In `Oral Chamber`: patch an **external white-noise source** (gated by a `CyberlineLungs` `FLOW`/`PRESSURE` envelope) into **`INJECT`**. Set `CONSTRPOS` to `8.0` (palatal/dental), `CONSTRW` to `0.8` (tight constriction), and raise `FRIC` to `0.8`.

---

### 2. Elephant (*Loxodonta*)

Elephants produce both low-frequency infrasonic rumbles and piercing trumpet blasts.

```
  [ Lungs ] ──PRESSURE──▶ [ Larynx ] ──VOICED──▶ [ Oral Chamber ] ──▶ NOSE (Trunk)
  (Slow rate)              (FOLDLEN: 45mm)        (VTL: 55-60cm)
                           (OCT: -1)              (VELUM: 1.0)
```

#### A. Infrasonic / Sub-bass Rumble (15–30 Hz)
Elephants communicate over long distances using heavy, large vocal folds vibrating at infrasonic rates through their elongated vocal and nasal tract.

* **Module Settings:**
  * **`CyberlineLarynx`:** `FOLDLEN`: `45 mm` (maximum fold length), `OCT`: `−1`, `TENSION`: `0.45` (≈19 Hz), `PCOUPLE`: `0`, `DRIFT`: `0.5`.
  * **`CyberlineOralChamber`:** `VTL`: `58.0 cm` (maximum tract elongation), `VELUM`: `1.0` (fully open to the nasal branch / trunk), `WALL`: `0.99` (extra wall damping).
* **Patch:**
  1. Leave `PITCH` near 0 V (±0.3 V of slow modulation sweeps ≈16–24 Hz). Don't add a large negative bias — it drops below 10 Hz.
  2. Drive `Larynx` **`PRESSURE`** with a slow swell from `CyberlineLungs` `PRESSURE` (BPM set to 4–6 bpm).
  3. Listen exclusively from **`NOSE_OUTPUT`** (the acoustic termination of the trunk).
  4. The result is a deep, sub-bass seismic vibration.

#### B. Trumpet Call (High-Pressure Blast)
* **Module Settings:** `CyberlineLarynx` `FOLDLEN` = `30 mm`, `OCT` = `+2`, `TENSION` = `0.85` (≈375 Hz base), `PCOUPLE` = `0.5`. `CyberlineOralChamber` `VTL` = `50 cm`, `VELUM` = `0.9`.
* **Patch:**
  1. Fire a high-amplitude envelope (0 to 10 V, fast attack, 600 ms decay) into `Larynx` **`PRESSURE`**.
  2. Send the same envelope, scaled to about +0.6 V, into `Larynx` **`PITCH`** so the call sweeps ≈375 → 570 Hz. (`PCOUPLE` adds only up to 30 Hz, so it can't do this alone.)
  3. The elongated nasal tract acts as a resonant brass-like column, terminating at `NOSE_OUTPUT`.

---

### 3. Primate / Chimpanzee (*Pan troglodytes*)

Primate calls combine fast rhythmic respiratory bursts (pant-hoots) with flexible tongue/jaw shaping.

```
  [ Lungs: 45 bpm ] ──PRESSURE/FLOW──▶ [ Larynx: 9mm ] ──▶ [ Oral Chamber: 11cm ] ──▶ LIPS
                                                                    ▲
  [ 8 Hz LFO ] ─────────────────────────────────────────────────────┘ (TONGUEY: chatter)
```

#### A. Pant-Hoot Sequence
* **Module Settings:** `CyberlineLarynx` `FOLDLEN`: `9.0 mm` (≈380 Hz at tension 0.45), `TENSION`: `0.45`, `DRIFT`: `0.4`. `CyberlineOralChamber` `VTL`: `11.0 cm`, `TONGUEX`: `3.0`, `TONGUEY`: `4.0` (open round "oo" shape).
* **Patch:**
  1. Clock `CyberlineLungs` to a rapid tempo (35–50 bpm).
  2. Route `Lungs` `PRESSURE` into `Larynx` `PRESSURE`.
  3. Over a sequence of 6–10 respiratory cycles, use a sequencer or slow ramp generator to gradually increase `Larynx` `PITCH` (+1 to +2 octaves), `Larynx` `TENSION` (0.45 $\to$ 0.90), and `Oral Chamber` mouth opening.
  4. The sound transitions from warm panting into a scream (≈380 Hz rising to ≈1.5 kHz with +2 octaves). Inhale noise is not generated by the Larynx; add external noise into `INJECT` if wanted.

#### B. Alarm Screech / Lip-Smacking Chatter
* **Patch:** Set `Larynx` `PITCH` to about +1 V (≈770 Hz with the 9 mm setting). Patch a 7–10 Hz triangle or square LFO into `Oral Chamber` **`TONGUEY`** and **`CONSTRW`** to simulate rapid rhythmic jaw/lip chattering. Introduce slight noise into `INJECT` with `FRIC` = `0.4`.

---

### 4. Human (*Homo sapiens*) — Adult Female Voiced Exhale (Moan)

The human voice sits squarely in the suite's design range (12–18 mm folds,
17 cm tract), but the adult female register favors the short end of both
distributions. A sustained non-speech vocalisation ("moan") is physically a
*breathiness + strain* phenomenon, not a phoneme: low-enough tension that the
aspiration term is audible, a slightly open velum that softens the vowel, and
a pitch contour coupled to the emptying reservoir — the sound climbs as the
breath runs out.

```
  [ Lungs: 6 bpm, TRIG/CYCLE ] ──PRESSURE──▶ [ Larynx: 11.5mm ] ──VOICED──▶ [ Oral Chamber: 15.5cm ] ──▶ LIPS + NOSE
              ──VOLUME──(invert 0.08x)──▶ PITCH      (TENSION 0.4)            (VELUM 0.1, slow TONGUEY)
              ──VOLUME──(att. 1.5V)─────▶ TONGUEY
```

#### A. Core Moan
* **Module Settings:**
  * **`CyberlineLungs`:** `RATE`: `6/min` (CYCLE) or TRIG mode for one-shot calls; `DEPTH`: `0.85`, `INHALE`: `0.3 s` (quick catch-breath), `MAXFLOW`: `0.3` (conserved air → long drain), `EFFORT`: `0.75` (plateau sustain tapering at the end), `INH MUTE`: `Mute` (silent fill). With the **`START`** switch set to **Exhale first**, the sound fires on the cycle boundary and the silent inhale recharges afterward — useful for clock-synced calls; keep **Inhale first** for the physiological gasp-before-release shape.
  * **`CyberlineLarynx`:** `FOLDLEN`: `11.5 mm` (≈215 Hz at tension 0.6; female anatomy), `TENSION`: `0.4` — the key knob: chest voice, but low enough that `intensity·(1−√tension)` aspiration is audible (0.3 → whisper-adjacent, 0.55 → fuller/cleaner), `OCT`: `0` (place base F0 ≈ F#3–A3), `DRIFT`: `0.4`, `VIBRATE`: `4.5 Hz` (slow, not singer vibrato), `PCOUPLE`: `0.45` (loud passages bend sharp — the intensity/moan interaction), `PRESSURE` knob: `0.15` (the Lungs envelope owns the dynamics).
  * **`CyberlineOralChamber`:** `VTL`: `15.5 cm` (shorter than the 17.5 cm male-neutral default → formants up, brighter color), `TONGUEX`: `-1.0`, `TONGUEY`: low (open "ah"), `VELUM`: `0.1` — a slightly open nasal branch is what separates a soft vulnerable vowel from speech; push to `0.3` for closed "mmm/nnn" intensities, `WALL`: default.
* **Patch:**
  1. `Lungs` `PRESSURE` → `Larynx` `PRESSURE` (the loudness swell).
  2. `Lungs` `VOLUME` → **inverting attenuverter** → `Larynx` `PITCH`, inverted and scaled to ≈ `0.08×` (≈0.8 V at empty): pitch *rises* as the lungs drain (strain coupling) — the signature contour of the moan.
  3. `Lungs` `VOLUME` → attenuator (`1.5 V` full-scale) → `Oral Chamber` `TONGUEY`: the vowel drifts "ah → uh/eh" as the breath empties. Free micro-detail, no extra module.
  4. `Larynx` `VOICED` → `Oral Chamber` `SOURCE`; take `LIPS` plus ≈20% `NOSE` in the mix.
  5. Optional: slow LFO (0.03–0.05 Hz) → `TONGUEX` for vowel wandering during long calls.
* **Variants:**
  * **Gasp onset:** set `INH MUTE` to `Suction` and fold a scaled copy of `PRESSURE` (the negative inhale lobe) quietly into the output mix — you hear the sharp intake before each exhale.
  * **With `CyberlineVoice`:** same recipe, but set its `FEEDBACK GAIN` ≈ `0.2` — tract loading on the glottis adds naturalness with no extra cable.
  * **With `CyberlineBroca`:** `VOWEL A` = /a/, `VOWEL B` = /o/ or /u/, `V.MORPH` ≈ `0.35` (CV it for wandering), `TRACT POLY` → chamber `TNGX/POLY`, `LARYNX POLY` → larynx `TENS/POLY`; modulating the voicing channel with `Lungs` `PHASE` gives breathy-onset vowels automatically.
  * **Heartbeat timing:** `CyberlineHeart` `BEAT` → `Lungs` `TRIGGER` (RATE ≈ 70, `DRIVE` −0.4, `HRV` up): calls timed to the pulse, irregularly.
* **Finishing (external):** short plate reverb; no fast compression (the source is already close-mic'd); gentle lowpass ≈ 6–8 kHz. Tuning order when it sounds wrong: male → lower `FOLDLEN` + `VTL` + `TENSION`; too clear/speechy → lower `TENSION`, raise `VELUM` slightly.

---

## 4. Avian / Bird Call Patch Recipes

Birds produce sound via a **syrinx** located at the base of the trachea rather than a larynx at the top. The entire trachea functions as a resonant acoustic filter above the sound generator, while beak aperture (gape) dictates high-frequency radiation.

### 1. Songbird Chirp & Trill (Finch, Cardinal, Canary)
Songbirds produce agile, sinusoidal chirps (rapid downward sweeps or chevron arcs) around **2 kHz – 5 kHz**.

```
  [ Fast AD Envelope ] ──▶ Larynx PITCH (~-1 V inverted sweep)
            │
            └──▶ Larynx PRESSURE (40-90 ms burst)
                    │
                    ▼
  [ Larynx ] ──VOICED──▶ [ Oral Chamber ] ──▶ LIPS_OUTPUT
  (FOLDLEN: 2mm)          (VTL: 6cm)
  (OCT: 0)         (TONGUEY: 3, open beak)
```

* **Module Settings:**
  * **`CyberlineLarynx`:** `FOLDLEN`: `2.0 mm` (minimum fold mass), `OCT`: `0` (≈4.1 kHz at tension 0.7), `TENSION`: `0.70`, `PCOUPLE`: `0.3`.
  * **`CyberlineOralChamber`:** `VTL`: `6.0 cm` (minimum tract length), `TONGUEY`: `3.0` (open beak).
* **Patch:**
  1. Trigger an attack-decay envelope (**Attack: 5 ms, Decay: 50–80 ms**).
  2. Send it to `Larynx` **`PRESSURE`**, and an **inverted** copy scaled to about −1 V into **`PITCH`**, so each chirp falls from ≈4.1 kHz to ≈2 kHz.
  3. Every trigger produces a bright, clean chirp. `PCOUPLE` only adds up to 30 Hz, so a pitch envelope is required for the sweep.
  5. *For a trill:* Drive `PRESSURE` with a 12–20 Hz clock burst.

### 2. Dove or Owl "Coo" / "Hoo-Hoo"
Pigeons, mourning doves, and owls vocalise with closed beaks, creating warm, woody, pure tones between **200 Hz and 450 Hz** filtered through an elongated neck/trachea.

* **Module Settings:**
  * **`CyberlineLarynx`:** `FOLDLEN`: `9.0 mm – 10.0 mm`, `OCT`: `0` ($F_0 \approx 270\text{–}310\text{ Hz}$ at tension 0.3), `TENSION`: `0.25 – 0.35` (soft, round glottal wave), `VIBRATE`: `5.5 Hz – 6.5 Hz` (internal vibrato provides the tremolo flutter).
  * **`CyberlineOralChamber`:** `VTL`: `10.0 cm – 14.0 cm` (long neck/trachea; a rough approximation), `TONGUEY`: `8.0` (closed beak), `WALL`: `0.985` (extra damping of high harmonics for a mellow, wooden tone), `VELUM`: `0.3`.
* **Patch:**
  1. Trigger an envelope with a soft attack (~120 ms) and smooth release (~300 ms) into `PRESSURE`.
  2. Fire in rhythmic motifs: *two short pulses, then one long pulse* ("whoo-whoo... whooooo").

### 3. Crow / Gull "Caw" or Duck "Quack"
Corvids and waterfowl produce harsh, raucous calls with strong harmonic overtones, non-linear membrane vibration, and tracheal resonance flare.

* **Module Settings:**
  * **`CyberlineLarynx`:** `FOLDLEN`: `7.0 mm` (≈750 Hz at tension 0.85), `TENSION`: `0.85` (tense glottal wave), `DRIFT`: `0.5`, `FBGAIN`: `0.5 – 0.7`.
  * **`CyberlineOralChamber`:** `VTL`: `11.0 cm – 13.0 cm`, `TONGUEX`: `7.0` (forward flare), `TONGUEY`: `3.5`.
* **Patch:**
  1. Patch `Larynx` `VOICED` back into `Larynx` **`FEEDBACK_INPUT`**. This feeds acoustic energy back into the folds, inducing subharmonics, roughness, and vocal fold rasp.
  2. Send an envelope with a sharp attack and ~250 ms decay into `PRESSURE` (crow "caw") or a 5 Hz gate sequence (duck quacks).

---

## 5. Marine Mammal Patch Recipes

Marine mammals possess specialized respiratory and phonation adaptations: baleen whales sing underwater via internal air recycling across massive laryngeal folds, toothed whales generate ultrasonic clicks and agile signature whistles via nasal phonic lips, and pinnipeds bark with powerful chest resonances.

```
  [ Slow Slew / LFO ] ──▶ Larynx PITCH (Wandering 40-400 Hz FM)
                                 │
                                 ▼
  [ Larynx ] ──────────────▶ [ Oral Chamber ] ──▶ LIPS_OUTPUT
  (FOLDLEN: 40mm)             (VTL: 55cm, laryngeal sac)
  (DRIFT: 0.6)                (WALL: 0.985)
```

### 1. Humpback Whale (*Megaptera novaeangliae*)
Humpback songs consist of haunting, frequency-modulated sweeps, sub-bass groans, and mournful wails resonating inside a massive laryngeal sac.

* **Module Settings:**
  * **`CyberlineLarynx`:**
    * `FOLDLEN`: `40 mm` (≈50 Hz at tension 0.5).
    * `OCT`: `0` (a 0 to +3 V `PITCH` sweep covers ≈50–400 Hz).
    * `TENSION`: `0.50`.
    * `DRIFT`: `0.6` (continuous pitch wander).
    * `VIBRATE`: `3.0 Hz – 4.0 Hz` (slow underwater waver).
  * **`CyberlineOralChamber`:**
    * `VTL`: `50.0 cm – 58.0 cm` (cavernous laryngeal sac cavity).
    * `TONGUEY`: `4.0`.
    * `WALL`: `0.985` (extra damping to suggest dense tissue).
* **Patch:**
  1. Send a slow, smooth random voltage (slewed S&H or an LFO at 0.1–0.3 Hz) into `Larynx` **`PITCH`** (0 V to +3 V range) so the pitch continuously drifts through mournful glides.
  2. Feed a slow unipolar swell into `PRESSURE` (or keep `PRESSURE` constant at 0.7, as baleen whales recycle air internally without exhaling).
  3. Route `LIPS_OUTPUT` through a lush stereo reverb or delay to emulate ocean acoustic transmission.

### 2. Bottlenose Dolphin (*Tursiops truncatus*)
Dolphins produce two distinct classes of sounds: ultra-agile frequency-modulated **signature whistles** (used for social identification) and **burst-pulse clicks / creaks** (for echolocation).

* **Signature Whistle Settings:**
  * **`CyberlineLarynx`:** `FOLDLEN`: `2.0 mm` (minimum fold mass), `OCT`: `0` (≈4.6 kHz at tension 0.9; a 0 to +1 V sweep reaches ≈9 kHz), `TENSION`: `0.90`.
  * **`CyberlineOralChamber`:** `VTL`: `6.0 cm` (minimum tract length), `TONGUEY`: `3.0`.
* **Patch:**
  1. Patch a looping contour envelope or triangle LFO (1–3 Hz) into `Larynx` **`PITCH`** (0 V to +1 V sweep) to generate characteristic dolphin signature whistle contours (e.g. rising-falling loops).
  2. Modulate `PRESSURE` with snappy on/off gates.
  3. *For burst-pulse clicks / creaks:* Send a high-frequency trigger burst (100–300 Hz) into `Oral Chamber` **`INJECT`** with `CONSTRW` set to a low value (`0.2`) and listen from `NOSE_OUTPUT` (modelling radiation through the melon).

### 3. California Sea Lion (*Zalophus californianus*)
Sea lions produce loud, staccato, guttural barks ("honk-honk-honk" or "ark-ark-ark") at a 2–3 Hz cadence to establish territory.

* **Module Settings:**
  * **`CyberlineLarynx`:**
    * `FOLDLEN`: `15.0 mm – 18.0 mm` (≈180–240 Hz at tension 0.75).
    * `OCT`: `0`.
    * `TENSION`: `0.75` (husky, chesty bark).
    * `FBGAIN`: `0.30` (slight guttural throat rattle).
  * **`CyberlineOralChamber`:**
    * `VTL`: `15.0 cm – 18.0 cm`.
    * `TONGUEX`: `5.0`.
    * `TONGUEY`: `3.5` (open oral flare).
    * `WALL`: `0.999` (default).
* **Patch:**
  1. Clock an envelope generator at **2.0–2.8 Hz** (Attack: 10 ms, Decay: 120 ms).
  2. Send the envelope into `Larynx` **`PRESSURE`** for sharp, staccato barking bursts.
  3. Patch `Larynx` `VOICED` back into `FEEDBACK_INPUT` to introduce a husky growl into each bark.

---

## 6. Comprehensive Parameter Quick Reference

Pitch figures are approximate, computed from the Larynx pitch law (see the note at the top).

| Animal Archetype | `FOLDLEN` | Approx. pitch | `VTL` | Output | Key Feature |
|---|---|---|---|---|---|
| **Songbird (Chirp/Trill)** | 2.0 mm | 4.1 kHz falling to ~2 kHz | 6.0 cm | `LIPS` | AD envelope into Pressure, inverted into Pitch |
| **Dolphin (Whistle)** | 2.0 mm | 4.6 kHz up to ~9 kHz | 6.0 cm | `LIPS` / `NOSE` | Looping FM contour into Pitch |
| **Dove / Owl (Coo)** | 9–10 mm | 270–310 Hz | 10–14 cm | `LIPS` + `NOSE` | Low tension, `VIBRATE` ~6 Hz, extra damping |
| **Crow / Gull (Caw)** | 7 mm | ~750 Hz | 11–13 cm | `LIPS` | `FEEDBACK_INPUT` chaos |
| **Cat (Meow)** | 7.5 mm | ~830 Hz falling to ~550 Hz | 8.5 cm | `LIPS` | Pitch fall + vowel glide |
| **Cat (Purr)** | 7.5 mm | ~400 Hz carrier, 24–28 Hz pulsing | 8.0 cm | `LIPS` + `NOSE` | External LFO into Pressure/Tension |
| **Primate (Chimp)** | 9 mm | ~380 Hz, rising | 11 cm | `LIPS` | Rhythmic panting (35–50 bpm) |
| **Human F (Moan)** | 11.5 mm | ~215 Hz, rising as V empties | 15.5 cm | `LIPS` + `NOSE` | Inverted VOLUME → Pitch (strain), low TENSION (breathiness), VELUM ~0.1 |
| **Sea Lion (Bark)** | 15–18 mm | ~180–240 Hz | 15–18 cm | `LIPS` | 2–2.8 Hz bursts, feedback growl |
| **Large Feline (Roar)** | 28–35 mm | ~70–100 Hz | 30–40 cm | `LIPS` | Heavy folds, wide tract (untested) |
| **Elephant (Rumble)** | 45 mm | ~16–24 Hz | 55–60 cm | `NOSE` | `OCT -1`, no pitch bias, open velum |
| **Elephant (Trumpet)** | 30 mm | ~375 Hz up to ~570 Hz | 45–55 cm | `NOSE` | `OCT +2`, pitch envelope |
| **Humpback Whale** | 40 mm | ~50–400 Hz sweep | 50–58 cm | `LIPS` | Slow random/LFO into Pitch (0 to +3 V) |
