## **Comprehensive Architecture Handoff: Multi-Topology Stereo Granular Delay**

**Target Platform:** Daisy Seed (ARM Cortex-M7)
**System Type:** Real-Time Live-Input Granular Processor

---

### **1. Global Memory & Audio Callback Architecture**

The system uses a dual-buffer architecture to maintain true stereo spatial imaging of the live input and accommodate multi-routing topologies.

- **Dual Delay Lines:** Instantiate two parallel `daisysp::DelayLine<float, MAX_DELAY>` objects (Left and Right).
- **Size:** 2 seconds per channel at 48kHz (96,000 samples each).
- **Memory Allocation:** This requires roughly 768KB of RAM. The engineer **must** use the `DSY_SDRAM_BSS` macro to allocate these buffers in the 64MB external SDRAM to prevent SRAM overflow and hardware crashes.
- **Block vs. Sample Execution:** Audio must be processed from the hardware in small blocks (e.g., 48 samples per callback) to minimize CPU cache misses.
- **Feedback Phase Integrity:** While audio is passed in blocks, the core `GranularEngine` routing loop must operate sample-by-sample within the audio callback block using a standard `for(size_t i = 0; i < size; i++)` loop. This ensures the feedback matrices resolve with exactly 1 sample of delay.

---

### **2. The Grain Worker Class (Voice Logic)**

A lightweight, heavily optimized class representing a single grain of audio (target polyphony: 16 voices). Each active grain outputs a true stereo slice.

- **Phase Accumulation:** Calculate a phase increment (1.0f / duration_samples) once upon triggering, accumulating it from 0.0 to 1.0 to track the grain's lifespan.
- **Interpolation:** The read heads must use **Hermite (Cubic) Interpolation** when fetching from floating-point delay times. Linear interpolation is strictly prohibited to prevent high-frequency muffling when grains are pitched down.
- **True Stereo Reading:** The grain reads simultaneously from both the Left and Right delay lines.
- **Constant-Power Panning:** Each grain is assigned a random pan position ($p$) from 0.0 to 1.0 on trigger. Calculate the gains using ARM CMSIS-DSP functions once per trigger:
  $L_{gain} = \cos(p \cdot \frac{\pi}{2})$
  $R_{gain} = \sin(p \cdot \frac{\pi}{2})$
- **Windowing:** Pre-calculate static `const float` wavetables (e.g., 4096 samples) of the envelope curves in Flash memory (using `D5_DTCMRAM`). Grains read from this lookup table using their current phase. The standard Hann generation formula is:

$$w(n) = 0.5 \cdot \left(1 - \cos\left(\frac{2\pi n}{N}\right)\right)$$

---

### **3. The GranularEngine Class (Routing & Processing)**

The master orchestrator handles voice allocation and the dynamic feedback matrix. It follows a strict **Read First, Write Last** processing order inside the per-sample loop.

**Per-Sample Processing Order:**

1. **Read:** Sum all active true-stereo grains into a master `grain_mix_L` and `grain_mix_R` (applying a 0.5f gain compensation).
2. **Route:** Apply live inputs and feedback to the grain mixes based on the selected topology.
3. **Saturate & Filter:** Pass the internal feedback paths through an integrated bipolar one-pole **Tone** filter (Lowpass behavior below 0.5, Highpass above 0.5) to prevent blowout, then apply a fast algebraic saturation approximation— `x / (1.0f + fabsf(x))` —to save CPU while taming peaks.
4. **Write:** Push the saturated signals into the Left and Right SDRAM delay lines.
5. **Output:** Apply `tanhf()` to the final wet/dry master outputs for smooth, hard limiting. (Swap for a faster Pade approximant if CPU exceeds 75%).

**Multi-Topology Routing Matrix:**

| Routing Mode  | Left Feedback Input                      | Right Feedback Input              | Best For                                                    |
| ------------- | ---------------------------------------- | --------------------------------- | ----------------------------------------------------------- |
| **Parallel**  | `in_L + (grain_mix_L * feedback)`        | `in_R + (grain_mix_R * feedback)` | True stereo independence; preserves external stereo pedals. |
| **Ping-Pong** | `in_L + (grain_mix_R * feedback)`        | `in_R + (grain_mix_L * feedback)` | Wide, dancing rhythms; pushes center-panned audio outward.  |
| **Series**    | `grain_mix_R + (grain_mix_L * feedback)` | `in_R + (grain_mix_R * feedback)` | Cascaded granulation; creates ultra-dense ambient clouds.   |

> **Note on Series Output:** The final Master Output wet signal should ideally be pulled purely from `grain_mix_L` (or a blend of L/R), as the Left channel contains the fully cascaded audio.

---

### **4. Control-Rate Parameter Smoothing**

Hardware ADC inputs must be decoupled from the audio-rate loop using a Block-Rate One-Pole Lowpass Filter to prevent "zippering" noise and pitch artifacts.

**The Mathematical Foundation:**
Implement an inline one-pole filter for continuous hardware controls:

$$y[n] = y[n-1] + \alpha \cdot (x[n] - y[n-1])$$

_Where $y[n]$ is the current smoothed output, $y[n-1]$ is the previous, $x[n]$ is the new target value, and $\alpha$ is the smoothing coefficient._

**Execution Architecture:**

- **Block-Rate Updates:** Execute ADC reads and smoothing math exactly once per audio callback block (~1kHz control rate).
- **ADC Deadbanding:** If the absolute difference between the new raw ADC value and the previously stored value is `< 0.001f`, ignore the read and bypass smoothing to save CPU.
- **Parameter Latching:** Grains must latch onto their smoothed Pitch, Size, and Pan parameters **only upon triggering**.

---

### **5. Dynamic Safety Clamping (Guardrails)**

Dynamically recalculate boundaries whenever Size, Pitch, or Spray change, utilizing an absolute minimum 100.0f sample safety padding.

- **Minimum Safe Delay:** `(Grain Size * max(1.0, Pitch)) + Spray + Safety Padding`
- **Maximum Safe Delay:** `Buffer Size - Grain Size - Spray - Safety Padding`

---

### **6. Hardware UI Mapping (12 Pots, 6 Toggles)**

**Analog Controls (Potentiometers):**

1. **Pitch:** Master transposition (-2.0x to +2.0x).
2. **Grain Size:** Duration of the grain (10ms to 500ms).
3. **Density / Rate:** Controls trigger scheduler frequency (sparse clicks to dense clouds).
4. **Time Spray:** Randomizes read head base position (0ms to 500ms jitter). Must be calculated **once** per grain trigger.
5. **Pitch Spray:** Adds random micro-tuning or octave jumps per grain.
6. **Pan Spread:** Scales random panning. (0 = Mono Center, Max = Hard L/R limits).
7. **Feedback:** Amount of grain mix fed back into the delay lines (0.0 to 0.85).
8. **Drive / Saturation:** Input gain into the algebraic saturator before granulation.
9. **Tone:** Bipolar control. 0.0 to 0.5 sweeps a Lowpass filter (cutting highs); 0.5 to 1.0 sweeps a Highpass filter (cutting lows). Neutral at 0.5.
10. **Reserved / Unassigned.**
11. **Output Level:** Master makeup gain before the final limiter.
12. **Wet / Dry Mix:** Blends the live input signal with the granulated output.

**Toggle Switches (On-Off-On):**

1. **Routing:** Parallel / Ping-Pong / Series
2. **Pitch Quantize:** Free / Semitones / Octaves & 5ths
3. **Direction:** Forward / 50-50 Random / Reverse
4. **Write State:** Live Input / Overdub / Freeze (Disables the `Write` step for infinite drone)
5. **Window Shape:** Percussive (Pluck) / Hann (Smooth) / Reverse Percussive (Swell)
6. **Density Rhythm:** Free (Async) / Synced (Tremolo) / Burst Mode (Clusters)

---

### **7. Footswitch Integration & Trigger Scheduler**

**Switch Assignments:**

- **FS 1 (Left) - Short Press:** Toggles Bypass. Behavior (True vs Trails) is determined by the global persistent setting.
- **FS 1 (Left) - Long Press (>500ms):** Momentary Freeze (traps buffer while held).
- **FS 2 (Right) - Short Press:** Tap Tempo (Overrides internal clock).
- **FS 2 (Right) - Long Press (>500ms):** Clear Buffer (Zeroes SDRAM; panic button).

---

### **8. Persistent Settings & Boot Gestures**

The system utilizes the Daisy Seed's QSPI Flash to store global configuration states that persist across power cycles.

- **Bypass Mode Toggle:** To toggle between **True Bypass** and **Trails Bypass**, hold **Footswitch 1 (Left)** while applying power to the unit.
  - **True Bypass (Default):** Hardware relays physically decouple the DSP. This must be implemented using the `dripline::AudioBypass` class to manage the timed relay and mute sequencing for click-free performance.
  - **Trails Bypass:** DSP remains in the signal path; live input is muted on bypass while the granular delay rings out naturally.
- **Visual Confirmation:** Upon a successful mode toggle at boot, **LED 1** should flash rapidly for 2 seconds before entering the main application loop.
- **Storage Mechanism:** Use `daisy::PersistentStorage` to manage the settings block in Flash memory.

**Tap Tempo Logic (FS 2):**

This functionality is handled by the `dripline::TapTempo` class, providing sample-accurate timing updates:

- **Debounce:** 20ms debounce to prevent double-triggering.
- **Averaging:** Store the last 3 valid tap delta times in a FIFO array. Tempo is the average of these three.
- **Timeout:** If time since the last tap exceeds 2.5 seconds, clear the averaging array.

- **Density Knob Override:** When a valid tapped tempo is active, **Knob 3 (Density)** temporarily stops controlling raw frequency and acts as a discrete subdivision selector:
- `0.0 - 0.2`: Quarter Notes (1x)
- `0.2 - 0.4`: Dotted Eighth (1.5x)
- `0.4 - 0.6`: Eighth Notes (2x)
- `0.6 - 0.8`: Eighth Note Triplets (3x)
- `0.8 - 1.0`: Sixteenth Notes (4x)

---

### **9. Visual Feedback & LED UI**

The two RGB LEDs provide real-time feedback on the system's state. Colors should be mixed using the RGB channels to represent complex states.

#### **LED 1 (Left): Primary Status**

- **Effect Active:** Solid Green.
- **Bypassed (True Bypass):** Off.
- **Bypassed (Trails Bypass):** Dim Red (indicates the DSP is still alive and ringing out).
- **Freeze Active:** Solid Blue (overrides Bypass/Active colors).
- **Input Clipping:** Brief Red flicker if the input drive or grain sum exceeds 0dB before the algebraic saturator.

#### **LED 2 (Right): Tempo & Rhythm**

- **Flash Rate:** Flashes in sync with the Quarter Note of the Tap Tempo. If Tap is inactive, it pulses at the base Density frequency.
- **Color (Subdivision Indicator):**
  - **White:** Quarter Notes (1x)
  - **Yellow:** Dotted Eighth (1.5x)
  - **Cyan:** Eighth Notes (2x)
  - **Magenta:** Eighth Note Triplets (3x)
  - **Blue:** Sixteenth Notes (4x)
- **Grain Activity:** The brightness of the flash is modulated by the number of currently active grains (brighter for dense clouds, sharper blinks for sparse grains).

#### **Global UI States**

- **Boot Gesture:** As defined in Section 8, LED 1 flashes rapidly (White) for 2 seconds when the Bypass Mode is changed.
- **Panic/Clear:** Both LEDs flash Red once when the buffer is cleared (Long press on FS 2).

#### **Scheduler Jitter Density Rhythm (Async Mode):**

To prevent metallic resonance and comb-filtering artifacts at high densities, the scheduler applies a random timing jitter when in **Free (Async)** rhythm mode. After each trigger, the accumulator threshold is randomized between `0.85` and `1.15` (+/- 15% jitter). This jitter is disabled in **Synced** and **Burst** modes to ensure rhythmic pulses remain tight and predictable for tapped tempos.
