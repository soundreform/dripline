# Granular Delay - Manual Hardware Test Plan

This document outlines the manual testing procedure for the Granular Delay pedal hardware.

## 1. Initial Setup

1.  **Power:** Connect a standard 9V center-negative power supply.
2.  **Audio:** Connect an audio source (guitar, synthesizer) to the input and connect the output to an amplifier or audio interface.
3.  **Expression (Optional):** Connect a standard TRS expression pedal.

---

## 2. UI & Interaction Testing

This section verifies all user interface elements, including LEDs, footswitches, and boot-time gestures.

### 2.1. Boot Gesture: Bypass Mode

**Objective:** Verify the ability to toggle the global bypass mode.

| Step                                                 | Expected Result                                                                  | Pass/Fail |
| :--------------------------------------------------- | :------------------------------------------------------------------------------- | :-------- |
| 1. With power disconnected, press and hold **FSW1**. | -                                                                                |           |
| 2. While holding **FSW1**, apply power.              | **LED 1** should flash white rapidly for ~200ms.                                 |           |
| 3. Release **FSW1** and power cycle the unit.        | The bypass mode (True/Trails) is now toggled from its previous state.            |           |
| 4. Repeat steps 1-3 to toggle the mode back.         | **LED 1** flashes white again, confirming the change back to the original state. |           |

### 2.2. LED Indicators

**Objective:** Verify all visual feedback from the LEDs is correct.

| State / Action                               | Expected LED Behavior                                                                                                                | Pass/Fail |
| :------------------------------------------- | :----------------------------------------------------------------------------------------------------------------------------------- | :-------- |
| **Effect Active**                            | **LED 1** is solid Green.                                                                                                            |           |
| **Bypassed (Trails Mode)**                   | **LED 1** is dim Red.                                                                                                                |           |
| **Bypassed (True Bypass Mode)**              | **LED 1** is Off.                                                                                                                    |           |
| **Freeze Active** (Hold **FSW1**)            | **LED 1** is solid Blue, overriding other status colors.                                                                             |           |
| **Input Clipping** (Increase `Drive` knob)   | **LED 1** briefly flickers Red.                                                                                                      |           |
| **Tap Tempo** (Tap **FSW2**)                 | **LED 2** flashes at the tapped quarter-note rate.                                                                                   |           |
| **Subdivision Color** (Turn `Density` knob)  | In `Synced` mode, **LED 2** color changes to reflect the subdivision: White (1x), Yellow (1.5x), Cyan (2x), Magenta (3x), Blue (4x). |           |
| **Grain Activity** (Increase `Density` knob) | The brightness of **LED 2**'s pulse increases as the number of active grains increases.                                              |           |

### 2.3. Footswitch Controls

**Objective:** Verify all footswitch actions perform correctly.

| Action                                   | Initial State | Expected Result                                           | Pass/Fail |
| :--------------------------------------- | :------------ | :-------------------------------------------------------- | :-------- |
| **FSW1** - Short Press (<500ms)          | `ACTIVE`      | Enters `BYPASSED` state.                                  |           |
| **FSW1** - Short Press (<500ms)          | `BYPASSED`    | Enters `ACTIVE` state.                                    |           |
| **FSW1** - Long Press (>500ms) & Hold    | `ACTIVE`      | Enters momentary `FROZEN` state.                          |           |
| **FSW1** - Release after Long Press      | `FROZEN`      | Returns to `ACTIVE` state.                                |           |
| **FSW2** - Short Press                   | Any           | Sets the tap tempo.                                       |           |
| **FSW2** - Long Press (>500ms) & Release | Any           | Clears the audio buffer. Both LEDs should flash Red once. |           |

### 2.4. Expression Pedal Assignment

**Objective:** Verify the expression pedal assignment workflow.

| Step                                                               | Expected Result                                                                                                                                                                                      | Pass/Fail |
| :----------------------------------------------------------------- | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :-------- |
| 1. From the `ACTIVE` state, press and hold **FSW2** for >1 second. | The pedal enters `EXPR_ASSIGN` mode.                                                                                                                                                                 |           |
| 2. While holding **FSW2**, turn **Knob 10**.                       | **LED 2** should change color to indicate the selected parameter: White (Unassigned), Blue (Pitch), Green (Duration), Yellow (Density), Cyan (Time Spray), Magenta (Pitch Spray), Orange (Feedback). |           |
| 3. Release **FSW2**.                                               | The assignment is confirmed. **LED 2** blinks with the color of the assigned parameter. The setting is saved and will persist after a power cycle.                                                   |           |

---

## 3. Audio Feature Testing

This section verifies the audio processing and DSP features.

### 3.1. Core Granular Engine

| Feature | Test Procedure | Expected Audio Result - | :--------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | :-------- |
| **Pitch** | Sweep **Knob 1** from min to max. | Pitch shifts smoothly from -2x to +2x. - |
| **Duration** | Sweep **Knob 2**. | Grain length changes from short, glitchy stutters to long, smooth tones. - |
| **Density** | Set Rhythm to `Free`. Sweep **Knob 3**. | Trigger rate goes from sparse clicks to a dense, continuous cloud of grains. - |
| **Time Spray** | Increase **Knob 4**. | Grains become audibly more scattered and randomized in their start position. - |
| **Pitch Spray** | Increase **Knob 5**. | Individual grains have more noticeable random pitch variations. - |
| **Pan Spread** | Increase **Knob 6**. | The stereo field becomes wider and more spacious. - |
| **Feedback** | Increase **Knob 7**. | Grains begin to repeat and build on each other, leading to self-oscillation at high settings. - |
| **Drive** | Increase **Knob 8**. | The signal becomes audibly saturated and harmonically richer. - |
| **Tone** | Sweep **Knob 9** below 12 o'clock, then above. | Below 12:00, high frequencies are cut (LPF). Above 12:00, low frequencies are cut (HPF). - |
| **Mix** | Sweep **Knob 12**. | Blends from 100% dry signal to 100% wet (granular) signal. - |
| **Freeze** | Play a chord, then press and hold **FSW1**. While holding, adjust `Pitch`, `Tone`, and `Spray` knobs. | The chord is captured and granulated indefinitely. The knob adjustments manipulate the frozen sound in real-time. - |
| **Quantize** | Set **Toggle 2** to `Semitones`. Sweep `Pitch`. Set to `Octaves/Fifths` and repeat. | In `Semitones` mode, pitch snaps to the chromatic scale. In `Octaves/Fifths` mode, it snaps only to root notes, perfect fifths, and octaves. - |
| **Direction** | Set **Toggle 3** to `Reverse`. | Grains play backward, creating a reverse-delay effect. - |
| **Window** | Set **Toggle 5** to `Pluck`, then `Swell`. | `Pluck` creates a sharp, percussive attack. `Swell` creates a slow, soft attack. - |
| **Routing** | Set **Toggle 1** to `Ping-Pong`, then `Series`. | `Ping-Pong` creates a wide, rhythmic stereo effect. `Series` creates a very dense, cascading ambient texture. - |
| **Rhythm** | Set **Toggle 6** to `Synced`. Tap a tempo with **FSW2**. Turn **Knob 3**. | The grain trigger rate locks to subdivisions of the tapped tempo. - |
| **Expression** | Assign a parameter (e.g., `Pitch`). Set **Knob 10** to max, then min. | Sweeping the expression pedal controls the assigned parameter. The direction of the sweep is inverted when Knob 10 is turned below 12 o'clock. - |
