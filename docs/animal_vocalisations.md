# Animal Vocalisation & Bioacoustic Patch Guide

The `cyberline` voice synthesis pipeline—[`CyberlineLungs`](../src/CyberlineLungs.hpp), [`CyberlineLarynx`](../src/CyberlineLarynx.hpp), and [`CyberlineOralChamber`](../src/CyberlineOralChamber.hpp)—is built on mammalian acoustic physics and allometric scaling. Rather than relying on static formant filters or pre-recorded samples, sounds are synthesized from physical models of the respiratory tract, vibrating vocal folds, and acoustic scattering waveguides.

This guide details how to configure and modulate the modules to simulate non-human animal vocalisations, including cats, elephants, and primates.

---

## 1. Bioacoustic Principles in Cyberline

### Allometric Scaling of the Glottal Source
In mammalian biology, fundamental frequency ($F_0$) scales inversely with vocal fold length ($L_{\text{fold}}$) according to the interspecific mammalian allometric power law:

$$\log_{10}(F_0) = -1.43 \cdot \log_{10}(L_{\text{fold}}) + 4.01$$

This equation is implemented directly in [`CyberlineLarynx`](../src/CyberlineLarynx.hpp) via the **`FOLDLEN`** parameter:
- **2 mm – 4 mm:** Small mammals, rodents, kittens ($F_0 \approx 400\text{ Hz} - 3800\text{ Hz}$).
- **12 mm – 16 mm:** Adult humans ($F_0 \approx 100\text{ Hz} - 250\text{ Hz}$).
- **30 mm – 45 mm:** Large carnivores, ungulates, elephants ($F_0 \approx 15\text{ Hz} - 60\text{ Hz}$).

### Formant Scaling via Vocal Tract Length (VTL)
Formant frequencies $F_n$ are determined by the quarter-wave resonance frequencies of the vocal tract tube:

$$F_n = \frac{(2n - 1) \cdot c}{4 \cdot \text{VTL}}$$

where $c \approx 35000\text{ cm/s}$ is the speed of sound in warm moist air. [`CyberlineOralChamber`](../src/CyberlineOralChamber.hpp) supports a **`VTL`** range from **6 cm** up to **60 cm** (further extensible with $\pm 10\text{ V}$ CV at $5.4\text{ cm/V}$):
- **6 cm – 9 cm:** High, bright infant/small animal formants ($F_1 \sim 1000\text{–}1400\text{ Hz}$, $F_2 \sim 3000\text{–}4500\text{ Hz}$).
- **16 cm – 18 cm:** Human vocal tract ($F_1 \sim 250\text{–}850\text{ Hz}$, $F_2 \sim 800\text{–}2400\text{ Hz}$).
- **45 cm – 60 cm:** Giant mammal cavities and trunk extensions ($F_1 \sim 150\text{ Hz}$, dense formant clustering).

### Nasal Shunt (Velum)
Opening the velum (**`VELUM`** knob or CV) routes acoustic energy into the nasal waveguide branch. This is essential for:
- Nasal snorts and hums.
- Elephant trunk acoustics (`NOSE_OUTPUT`).
- Cat purrs and mouth-closed murmurs.

---

## 2. Modulation Needs: Internal vs. External

- **Internal to Cyberline:**
  - `CyberlineLungs` provides the natural respiratory pressure, airflow direction, and chest volume.
  - `CyberlineLarynx` provides pitch-pressure coupling (`PCOUPLE`), organic pitch drift (`DRIFT`), and vibrato (`VIBRATE`).
- **When External Modulation is Recommended:**
  - **Audio-rate / High-rate LFOs (20–30 Hz):** Required for active neuromuscular laryngeal tremor (such as a cat's purr).
  - **Function Generators / Envelopes:** Needed for fast ballistic sweeps (screeches, chirps, sudden trumpets).
  - **Stepped Random / S&H:** Adds natural biological variation across consecutive calls.

---

## 3. Example Patch Recipes

### 1. Cat (*Felis catus*)

Cats possess two primary phonation mechanisms: vocal fold vibration with dynamic vocal tract filtering (meow/hiss) and intrinsic laryngeal muscle oscillation (purr).

```
                          [ 24-28 Hz LFO ]
                                 │
                                 ▼ (VCA gated by Lungs FLOW)
  [ Lungs ] ──PRESSURE──▶ [ Larynx ] ──VOICED──▶ [ Oral Chamber ] ──▶ LIPS / NOSE
             ──VOLUME───▶  (FOLDLEN: 3.5mm)       (VTL: 8cm)
```

#### A. Purr (20–28 Hz Neuromuscular Tremor)
A cat's purr is not a standard phonation; it is produced by rapid twitching of the laryngeal muscles alternating between opening and closing the glottis during both inhalation and exhalation.

* **Module Settings:**
  * **`CyberlineLarynx`:**
    * `FOLDLEN`: `3.5 mm`
    * `OCT`: `0`
    * `TENSION`: `0.30`
    * `DRIFT`: `0.35`
    * `PCOUPLE`: `0.2`
  * **`CyberlineOralChamber`:**
    * `VTL`: `8.0 cm`
    * `TONGUEX`: `4.0`
    * `TONGUEY`: `7.5` (mouth mostly closed)
    * `VELUM`: `0.4`
* **Patch:**
  1. Patch an external LFO set to a sine or rectified wave at **24–28 Hz** into a VCA.
  2. Modulate the VCA level using `CyberlineLungs` `VOLUME` or `std::abs(FLOW)` so the purr continues during both inhale and exhale.
  3. Route the pulsed 25 Hz signal into `Larynx` **`PRESSURE`** and **`TENSION`** inputs.
  4. Connect `Larynx` `VOICED` to `Oral Chamber` `SOURCE`.
  5. Mix both `LIPS_OUTPUT` and `NOSE_OUTPUT`.

#### B. Meow (Acoustic Vowel Glide /m-i-a-u/)
* **Module Settings:**
  * `CyberlineLarynx`: `FOLDLEN` = `3.5 mm`, `OCT` = `0`, `TENSION` = `0.55`.
  * `CyberlineOralChamber`: `VTL` = `8.5 cm`.
* **Patch:**
  1. Trigger an attack-decay envelope (Attack ~80 ms, Decay ~450 ms).
  2. Patch the envelope into `Larynx` **`PITCH`** (+0.6 V peak, gliding downward by ~8 semitones).
  3. Patch the envelope into `Oral Chamber` **`TONGUEX`** configured to sweep from front vowel (high voltage $\approx 8\text{ V}$, /i/) to back vowel (low voltage $\approx 2\text{ V}$, /u/).
  4. Patch an inverted short decay envelope into **`VELUM`** so the call begins with a closed mouth nasal hum (/m-/) before releasing into the vowel.

#### C. Hiss
* **Module Settings:**
  * Turn down `Larynx` voicing (`PRESSURE` = 0).
  * In `Oral Chamber`: patch `CyberlineLungs` `BREATH` or white noise into **`INJECT`**.
  * Set `CONSTRPOS` to `8.0` (palatal/dental), `CONSTRW` to `0.8` (tight constriction), and raise `FRIC` to `0.8`.

---

### 2. Elephant (*Loxodonta*)

Elephants produce both low-frequency infrasonic rumbles and piercing trumpet blasts.

```
  [ Lungs ] ──PRESSURE──▶ [ Larynx ] ──VOICED──▶ [ Oral Chamber ] ──▶ NOSE (Trunk)
  (Slow rate)              (FOLDLEN: 45mm)        (VTL: 55-60cm)
                           (OCT: -2)              (VELUM: 1.0)
```

#### A. Infrasonic / Sub-bass Rumble (15–30 Hz)
Elephants communicate over long distances using heavy, large vocal folds vibrating at infrasonic rates through their elongated vocal and nasal tract.

* **Module Settings:**
  * **`CyberlineLarynx`:**
    * `FOLDLEN`: `45 mm` (maximum fold length)
    * `OCT`: `-2`
    * `TENSION`: `0.22` (slack, heavy folds)
    * `DRIFT`: `0.5`
  * **`CyberlineOralChamber`:**
    * `VTL`: `58.0 cm` (maximum tract elongation)
    * `VELUM`: `1.0` (fully open to the nasal branch / trunk)
    * `WALL`: `0.8` (high wall compliance absorption)
* **Patch:**
  1. Patch a negative CV bias ($\approx -1.0\text{ V}$ to $-1.8\text{ V}$) into `Larynx` **`PITCH`** to reach the 16–24 Hz fundamental.
  2. Drive `Larynx` **`PRESSURE`** with a slow swell from `CyberlineLungs` `PRESSURE` (BPM set to 4–6 bpm).
  3. Listen exclusively from **`NOSE_OUTPUT`** (the acoustic termination of the trunk).
  4. The result is a deep, sub-bass seismic vibration.

#### B. Trumpet Call (High-Pressure Blast)
* **Module Settings:**
  * `CyberlineLarynx`: `FOLDLEN` = `30 mm`, `OCT` = `+1`, `TENSION` = `0.85` (brassy glottal wave), `PCOUPLE` = `0.85`.
  * `CyberlineOralChamber`: `VTL` = `50 cm`, `VELUM` = `0.9`.
* **Patch:**
  1. Fire a high-amplitude envelope (0 to 10 V, fast attack, 600 ms decay) into `Larynx` **`PRESSURE`**.
  2. Due to high `PCOUPLE`, the intense burst kicks the fundamental pitch up into a shrill register (300–600 Hz).
  3. The elongated nasal tract acts as a resonant brass-like column, terminating at `NOSE_OUTPUT`.

---

### 3. Primate / Chimpanzee (*Pan troglodytes*)

Primate calls combine fast rhythmic respiratory bursts (pant-hoots) with flexible tongue/jaw shaping.

```
  [ Lungs: 45 bpm ] ──PRESSURE/FLOW──▶ [ Larynx: 7mm ] ──▶ [ Oral Chamber: 11cm ] ──▶ LIPS
                                                                    ▲
  [ 8 Hz LFO ] ─────────────────────────────────────────────────────┘ (TONGUEY: chatter)
```

#### A. Pant-Hoot Sequence
Chimpanzee pant-hoots consist of a build-up of alternating voiced exhales ("hoo") and gasping inhales, culminating in an unconstrained climax scream.

* **Module Settings:**
  * **`CyberlineLarynx`:**
    * `FOLDLEN`: `7.0 mm` ($F_0 \sim 300\text{–}450\text{ Hz}$)
    * `TENSION`: `0.45`
    * `DRIFT`: `0.4`
  * **`CyberlineOralChamber`:**
    * `VTL`: `11.0 cm`
    * `TONGUEX`: `3.0`
    * `TONGUEY`: `4.0` (open round "oo" shape)
* **Patch:**
  1. Clock `CyberlineLungs` to a rapid tempo (35–50 bpm).
  2. Route `Lungs` `PRESSURE` into `Larynx` `PRESSURE`.
  3. Over a sequence of 6–10 respiratory cycles, use a sequencer or slow ramp generator to gradually increase:
     - `Larynx` `PITCH` (+1 to +2 octaves).
     - `Larynx` `TENSION` (0.45 $\to$ 0.90).
     - `Oral Chamber` `TONGUEY` and mouth opening.
  4. The sound transitions from warm, cyclic guttural panting into a climax screech.

#### B. Alarm Screech / Lip-Smacking Chatter
* **Patch:**
  1. Set `Larynx` to a high base pitch ($F_0 \sim 600\text{–}900\text{ Hz}$).
  2. Patch a 7–10 Hz triangle or square LFO into `Oral Chamber` **`TONGUEY`** and **`CONSTRW`** to simulate rapid rhythmic jaw/lip chattering.
  3. Introduce slight noise into `INJECT` with `FRIC` = `0.4` to add dental/labial friction.

---

## 4. Parameter Quick Reference for Animal Types

| Animal Archetype | Fold Length (`FOLDLEN`) | Base Pitch Range | Tract Length (`VTL`) | Primary Output | Key Feature |
|---|---|---|---|---|---|
| **Small Rodent / Bat** | 2.0 mm – 2.5 mm | 1 kHz – 4 kHz+ | 6.0 cm | `LIPS` | Extremely high F0, narrow formants |
| **Cat (Meow)** | 3.0 mm – 4.0 mm | 300 Hz – 700 Hz | 7.5 cm – 9.0 cm | `LIPS` | Downward pitch + vowel glide |
| **Cat (Purr)** | 3.5 mm | 20 Hz – 30 Hz tremolo | 8.0 cm | `LIPS` + `NOSE` | Audio-rate LFO into Pressure/Tension |
| **Canine / Wolf** | 8.0 mm – 12.0 mm | 150 Hz – 450 Hz | 12.0 cm – 15.0 cm | `LIPS` | Pitch bend howl; nasal whines |
| **Primate (Macaque/Chimp)** | 6.0 mm – 8.0 mm | 250 Hz – 800 Hz | 10.0 cm – 12.0 cm | `LIPS` | Rapid panting cadence; jaw chatter |
| **Large Feline (Lion Roar)** | 28.0 mm – 35.0 mm | 40 Hz – 100 Hz | 30.0 cm – 40.0 cm | `LIPS` | Heavy fold distortion, wide tract |
| **Elephant (Rumble)** | 45.0 mm | 14 Hz – 25 Hz | 55.0 cm – 60.0 cm | `NOSE` | Infrasound, deep nasal/trunk shunt |
| **Elephant (Trumpet)** | 30.0 mm | 300 Hz – 600 Hz blast | 45.0 cm – 55.0 cm | `NOSE` | High pressure-pitch coupling (`PCOUPLE`) |
