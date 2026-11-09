# Speech Synthesis, Prosody & Articulation Guide

When sequencing phonetic targets with [`CyberlineBroca`](../src/CyberlineBroca.hpp) or [`CyberlineBrocaSq`](../src/CyberlineBrocaSq.hpp) into [`CyberlineOralChamber`](../src/CyberlineOralChamber.hpp) or [`CyberlineVoice`](../src/CyberlineVoice.hpp), sequencing through random consonants and vowels often sounds like an electronic organ or an arpeggiator cycling formant filters rather than human speech or recognizable words.

This guide explains the psychoacoustic and linguistic mechanics behind why that happens, and provides practical patching strategies to turn raw phonetic gestures into expressive, speech-like utterances.

---

## 1. Why Random Phoneme Sequencing Doesn't Sound Like Words

The human auditory cortex does not recognize speech simply by identifying a sequence of spectral vowel targets and consonant bursts. Natural speech perception relies heavily on four fundamental cues:

### 1.1 Pitch Prosody & $F_0$ Contours (The "Organ Pipe" Effect)
- **Monotone pitch kills speech illusion:** If vocal pitch ($F_0$) is static or quantized to an unvarying musical note, the brain immediately classifies the sound as a mechanical synthesizer or organ pipe rather than an intentional voice.
- **Phrase Declination (Macro-Prosody):** Spoken phrases follow an intonation contour. In declarative statements, pitch rises to a peak on the primary tonic syllable and then gradually drifts downward across the phrase ("declination"). In questions, pitch rises sharply at the phrase boundary.
- **Stress Accents:** Stressed syllables receive a dynamic pitch bump ($+1.5\text{ to }+3\text{ semitones}$, or $+15\text{--}30\text{ Hz}$), accompanied by higher breath pressure and vocal effort.
- **Segmental Micro-Prosody:** Voiceless consonants (/p/, /t/, /k/, /s/, /f/) cause an involuntary aerodynamic spike in pitch upon vowel onset ($+10\text{--}15\text{ Hz}$), while voiced consonants (/b/, /d/, /g/, /z/) depress pitch slightly.

### 1.2 Temporal Non-Isochrony (Rhythm & Meter)
- **Metronomic clocks sound mechanical:** Sequencers typically fire at isochronous intervals (e.g., a rigid 120 BPM clock yielding 125 ms per 16th step). Human speech is never isochronous.
- **Stress-timed duration:**
  - **Stressed syllables** are elongated ($150\text{--}250\text{ ms}$).
  - **Unstressed / reduced syllables** (such as prefixes, schwas, or articles) compress down to $40\text{--}80\text{ ms}$.
- **Word and clause pauses:** Words are grouped into phonological chunks separated by brief transitional pauses ($60\text{--}200\text{ ms}$) and respiratory resets. Without pauses, syllables blur into an unnatural, continuous stream.

### 1.3 Syllable Anatomy: The Missing "Coda"
- **Open syllables (CV) vs. Closed syllables (CVC / VC):**
  - Broca and Broca² produce open syllables of the form **CV** (Consonant Onset $\to$ Vowel Nucleus).
  - In Germanic and Romance languages, most content words end in consonants—they have a **Coda** (e.g. *cat*, *this*, *stop*, *look*, *man*).
  - An endless sequence of open CV syllables inherently sounds like synthetic Japanese or infant babbling (*"ba-ge-tu-da-zi-po-me"*), because the ear misses the sharp terminal stops and post-vowel closures that define Western words.

### 1.4 Phonotactics & Diphone Transition Rules
- Real languages have strict structural constraints on which phonemes can follow each other.
- Uniformly picking consonants and vowels at random inevitably produces combinations that violate subconscious linguistic expectations, preventing the listener's language processor from assembling perceived "words."

---

## 2. Patching Recipes for Speech-Like Prosody

Here is how to patch the `cyberline` modules to introduce these missing acoustic cues.

### Recipe 1: Syllable Pitch Micro-Prosody & Stress Accent

This patch dynamically kicks the vocal pitch on every consonant release, instantly transforming a static vocal buzz into an animated, living articulation.

```
+------------------+                    +--------------------+
| CyberlineBrocaSq |                    |   Attenuator/VCA   |
|                  |                    |                    |
|      CVENV_OUTPUT|------------------->| IN                 |
|                  |                    | OUT (atten. +0.2V) |---+
+------------------+                    +--------------------+   |
                                                                 |
                                                                 v
+------------------+                    +--------------------+ (Pitch Sum)
|   Pitch CV /     |------------------->| PITCH_INPUT        |
|   1V/Oct Source  |                    | CyberlineLarynx    |
+------------------+                    | (or Voice)         |
                                        +--------------------+
```

- **Patching Steps:**
  1. Patch Broca²'s `CVENV_OUTPUT` (or `GATE_OUTPUT`) into an attenuator.
  2. Set the attenuator output to around **$+0.15\text{ V to }+0.25\text{ V}$** (approx. 1.8 to 3 semitones).
  3. Sum this attenuated voltage into `PITCH_INPUT` on [`CyberlineLarynx`](../src/CyberlineLarynx.hpp) or [`CyberlineVoice`](../src/CyberlineVoice.hpp).
- **Result:** Every time a consonant releases into a vowel, the pitch surges slightly and decays down into the vowel body, mimicking the vocal cord tension spike of human speech.

---

### Recipe 2: Phrase Intonation & Sentence Declination

Simulate the natural downward pitch drift of spoken sentences.

- **Patching Steps:**
  1. Patch a slow decay envelope or downward ramp LFO (period roughly 1.5 to 3.0 seconds, matching a 4- or 8-step word sequence) into an attenuverter.
  2. Invert or set the depth so it drops pitch by **$-0.2\text{ V to }-0.4\text{ V}$** (approx. 2.5 to 5 semitones) across the phrase.
  3. Reset the LFO/envelope on step 1 of your sequencer.
  4. Sum this with your base pitch into the Larynx or Voice `PITCH_INPUT`.
- **Result:** The sequence now sounds like a coherent spoken sentence with a declarative cadence, rather than a repeating arpeggio loop.

---

### Recipe 3: Non-Isochronous Clocking (Rhythm & Variable Syllable Duration)

Break the rigid metronome to introduce natural syllabic timing.

- **Option A: External Swung / Irregular Clock:**
  1. Set Broca² to `MODE_CLOCK` (top switch).
  2. Drive `CLOCK_INPUT` from an irregular trigger source, a clock with high swing/shuffle, or a probabilistic gate generator (e.g. Euclidean sequencer with non-uniform step spacing).
- **Option B: Dynamic Onset Modulation:**
  1. Patch a stepped CV or sample-and-hold source into Broca²'s `ONSET` CV input.
  2. Short onset voltages ($40\text{--}60\text{ ms}$) produce fast, crisp unstressed consonants; higher voltages ($120\text{--}200\text{ ms}$) produce broad, deliberate, stressed syllables.

---

### Recipe 4: Constructing Word Boundaries with Rests

Grouping syllables into distinct perceptual "words" requires deliberate pauses and vowel-only syllables.

- **Syllable Grouping Example (8-step phrase):**
  - **Step 1:** Consonant + Vowel (e.g. `/t/ + /i/` — "ti")
  - **Step 2:** Consonant + Vowel (e.g. `/k/ + /o/` — "ko") $\rightarrow$ *Word 1: "tiko"*
  - **Step 3:** Set Consonant to **`REST`** (or manner `NONE`). Lower Breath Pressure. $\rightarrow$ **Pause**
  - **Step 4:** Consonant + Vowel (e.g. `/m/ + /a/` — "ma")
  - **Step 5:** Consonant + Vowel (e.g. `/l/ + /u/` — "lu")
  - **Step 6:** Consonant + Vowel (e.g. `/s/ + /i/` — "si") $\rightarrow$ *Word 2: "malusi"*
  - **Steps 7–8:** Set to **`REST`** for breath intake.
- **Respiratory Reset with [`CyberlineLungs`](../src/CyberlineLungs.hpp):**
  - Sync Lungs to cycle once per phrase (e.g. 15–20 BPM).
  - When Lungs is in the inhale phase (`PRESSURE_OUTPUT` is low or gated off), Broca rests. When Lungs exhales, Broca speaks.

---

### Recipe 5: Creating Closed Syllables (C-V-C Word Endings)

Because Broca and Broca² step forward per trigger, you can simulate a closed syllable (like *"cat"* or *"pan"*) across two consecutive rapid steps:

1. **Step 1 (The Onset + Vowel):**
   - Consonant: `/k/`, Vowel: `/a/`
2. **Step 2 (The Terminal Coda):**
   - Consonant: `/t/`, Vowel: `REST` (or close with `VELUM` for nasals like `/n/`, `/m/`)
   - Syllable gate releases quickly into closure.
- **Result:** The tract articulates *"ka"*, then immediately snaps shut into the alveolar stop occlusion of *"t"*, producing a true closed CVC syllable: *"kat"*.

---

## 3. Quick Reference: Speech Articulation Checklist

| Desired Perceptual Effect | Primary Controls / Modulations |
| :--- | :--- |
| **Living, human voice tone** | Attenuated `CVENV_OUTPUT` $\rightarrow$ `PITCH_INPUT` ($+1.5\text{--}3\text{ st}$). |
| **Sentence-ending cadence** | Slow negative ramp $\rightarrow$ `PITCH_INPUT` ($-3\text{--}5\text{ st}$ over the phrase). |
| **Separating words from babble** | Insert 1–2 `REST` steps between syllable clusters; gate off breath pressure. |
| **Stressed vs. unstressed syllables** | Modulate `ONSET` CV ($40\text{ ms}$ for short; $150\text{ ms}$ for stressed). |
| **Breath-driven realism** | Gate Larynx/Voice with [`CyberlineLungs`](../src/CyberlineLungs.hpp) `PRESSURE_OUTPUT`. |
| **Snappy plosives without mud** | Keep `SPEED` knob on Oral Chamber / Voice $\ge$ 2:00 ($30\text{--}38\text{ cm/s}$). |
