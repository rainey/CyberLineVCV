# Plan: `cyberline-lungs` (`BREATH`) Held-Breath Physics & `CLOSED` Mode Air Rationing

## Context & Problem Statement
In [`src/CyberlineLungs.cpp`](../src/CyberlineLungs.cpp):
1. **`PRESSURE_OUTPUT` (`PRESS`) is currently active during inhalation and top breath-hold:**
   - Lines 163–168 compute `pressure` purely from `volume` (`(1 - effort) * recoil + regulated`) regardless of whether the lungs are inhaling (`!exhaling, flow > 0`), holding air at the top (`!exhaling, flow == 0`), or exhaling (`exhaling == true`).
   - When `BREATH PRESS` is patched to `LARYNX PRESS` (or `VOICE PRESS`), the vocal folds phonate during inhalation and drone continuously while holding a breath at the top with `GATE` high.
2. **Top Breath-Hold (`GATE` high at full inflation) leaves `flow` non-zero on the clamp tick:**
   - In [`src/CyberlineLungs.cpp`](../src/CyberlineLungs.cpp) lines 108–131, when `volume` reaches `depth` while `gate` is held high, `volume` is clamped to `depth` and `exhaling` remains `false`, but `flow` is only zeroed on the subsequent control tick rather than immediately on reaching the top hold.
3. **`CLOSED` mode (`Mode::CLOSED`) is currently identical to `TRIG` mode:**
   - Per [`docs/spec.md`](spec.md) §5b and [`src/CyberlineLungs.hpp`](../src/CyberlineLungs.hpp), `CLOSED` mode is intended to retain air across breaths ("whale-style" / valved air rationing across phrases), whereas currently a trigger in `CLOSED` mode inhales and immediately drains all the way to `volume = 0` in one continuous exhalation just like `TRIG` mode.

---

## Expected Physiological Behavior of Outputs Across Breath Phases

| Output | **Inhale** (`!exhaling`, `volume < depth`) | **Top / Mid Hold (Apnea)** (`!exhaling`, `flow == 0`, `volume > 0`) | **Exhale (Phonation)** (`exhaling`, `volume > 0`) | **Bottom Hold (FRC Rest)** (`!exhaling`, `volume == 0`) |
| :--- | :--- | :--- | :--- | :--- |
| **`VOL`** (`0–10 V`) | Ramps up `0 → DEPTH` | **Holds flat** at current `volume` (`DEPTH` at top hold, or remaining volume in `CLOSED` mid-hold) | Drains toward `0` along recoil/effort curve | **Holds flat at `0 V`** |
| **`FLOW +-`** (`±5 V`) | **`+` (positive)** inflow rate | **`0 V`** ($dV/dt = 0$, no air moving) | **`−` (negative)** outflow rate | **`0 V`** ($dV/dt = 0$) |
| **`PHASE`** (`0 / 5 V`) | `0 V` | **`0 V`** | **`5 V` gate** | `0 V` |
| **`PRESS`** (`0–10 V`) | **`0 V`** (no outward subglottal phonation drive during inspiration) | **`0 V`** (airway valved shut or inspiratory muscles holding recoil back) | **Active `0–10 V` subglottal drive** (`(1 - effort) * recoil + regulated`) | **`0 V`** |

---

## Implementation Steps for a Later Agent

### Step 1: Gate `PRESS` (`PRESSURE_OUTPUT`) by Active Exhalation (`exhaling`)
In [`src/CyberlineLungs.cpp`](../src/CyberlineLungs.cpp) around lines 163–175:
- Compute positive outward subglottal driving pressure `pressure` **only when `exhaling` is true** (or in `PHASOR` mode during the exhale half `p > 0.5f`).
- During inhalation (`!exhaling && flow > 0`), top/mid breath hold (`!exhaling && flow == 0 && volume > 0`), and bottom hold (`volume == 0`), set `pressure = 0.f` (smoothed via the existing per-sample `outPressure` interpolator so transitions at glottal release/closure do not click).
- Verify that when `GATE` is held high in `CYCLE` or `TRIG` mode:
  1. The lungs inhale to `DEPTH` (`VOL` rises, `FLOW > 0`, `PRESS = 0`).
  2. Upon reaching `volume == depth`, `flow` is immediately set to `0.f` (**Top Hold**: `VOL = DEPTH * 10V`, `FLOW = 0V`, `PRESS = 0V`, `PHASE = 0V`).
  3. When `GATE` falls, `exhaling` becomes `true`, `PHASE` goes to `5 V`, and `PRESS` rises to the subglottal recoil+effort pressure to drive `LARYNX`/`VOICE`.

### Step 2: Implement Valved Air Rationing / Mid-Breath Hold in `CLOSED` Mode
In [`src/CyberlineLungs.cpp`](../src/CyberlineLungs.cpp) when `mode == CLOSED`:
- **`TRIG` rising edge (`trigRising`):**
  - Initiates an **inhale** to `depth` (`exhaling = false`, `holding = false` until `volume >= depth`).
  - When `volume` reaches `depth`, if `gate` is not high, **hold the breath at the top** (`volume = depth`, `flow = 0.f`, `exhaling = false`) rather than automatically draining!
- **`GATE` input in `CLOSED` mode (glottal/airway release valve):**
  - While `gate` is **high** (and `volume > 0.001f`), open the airway to **exhale** (`exhaling = true`), draining `volume` according to the recoil + effort law and outputting subglottal `PRESS`, negative `FLOW +-`, and `PHASE = 5 V`.
  - When `gate` goes **low** (`gateFalling`) while `volume > 0.001f`, **close the airway and hold the remaining breath mid-lung** (`exhaling = false`, `flow = 0.f`, `phase = 0.f`, `pressure = 0.f`, preserving `volume` at its current level).
  - Subsequent `GATE` high pulses continue draining from the retained `volume` (each phrase starting at the remaining recoil pressure, so phrases naturally shorten/soften as air is used up) until either `volume` reaches `0` or a new `TRIG` refills the reservoir to `depth`.
- **Fallback when `GATE` is unpatched in `CLOSED` mode:**
  - If `!inputs[GATE_INPUT].isConnected()`, allow `TRIG` rising edge to either toggle between inhale-to-hold and step-exhale, or document `TRIG` (refill) + `GATE` (metering valve) as the standard `CLOSED` mode pair.

### Step 3: Visual Indication in `BreathGauge` ([`src/CyberlineLungsWidget.cpp`](../src/CyberlineLungsWidget.cpp))
- In [`BreathGauge::draw`](../src/CyberlineLungsWidget.cpp), detect held-breath state (`bool holding = !exhaling && vol > 0.01f && std::fabs(module->flow) < 0.005f;`).
- When `holding` is true, highlight a subtle `HOLD` indicator or brighten the meniscus horizon and tracheal valve marker so the user can visually distinguish **inhaling** (`flow > 0`), **holding breath** (`flow == 0, vol > 0`), and **exhaling** (`exhaling`).

### Step 4: Tests & Verification
- Add a unit test in `tests/` verifying:
  1. `PRESS == 0 V` during inhalation and during `GATE`-held top hold.
  2. `VOL` holds constant at `DEPTH * 10 V` and `FLOW == 0 V` during top hold.
  3. In `CLOSED` mode, a single `TRIG` followed by three short `GATE` pulses produces three distinct stepped exhalations (`VOL` stair-stepping down during each `GATE` pulse and holding flat between pulses) with `PRESS > 0` only during the `GATE` pulses.
