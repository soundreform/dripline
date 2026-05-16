# Granular Delay State Machine Model

This document breaks down the behavior of the Granular Delay effect, as defined in `SPEC.md`, into a formal state machine model. This model is intended to guide the software implementation by defining distinct operational states, the events that cause transitions between them, and the actions performed within each state.

## 1. High-Level State Diagram

```
[POWER_OFF] --> POWER_ON --> [BOOTING]

[BOOTING] -- BOOT_COMPLETE --> [BYPASSED]

[BYPASSED] <--> FSW1_TAPPED <--> [ACTIVE]
     |
     +-- TRUE_BYPASS
     +-- TRAILS_BYPASS

[ACTIVE] -- FSW1_SHORT_PRESSED --> [FROZEN]

[FROZEN] -- FSW1_RELEASED --> [ACTIVE]

[ACTIVE] -- FSW2_LONG_PRESSED --> [EXPR_ASSIGN]

[EXPR_ASSIGN] -- FSW2_RELEASED --> [ACTIVE]
```

## 2. States and Transitions

### Global Events

These events trigger state transitions.

| Event                | Trigger                                        | Description                                                                    |
| -------------------- | ---------------------------------------------- | ------------------------------------------------------------------------------ |
| `POWERED_ON`         | Power is applied to the unit.                  | Starts the system.                                                             |
| `BOOT_COMPLETED`     | Initial hardware and software checks are done. | The system is ready for audio processing.                                      |
| `FSW1_TAPPED`        | Footswitch 1 is pressed and released < 500ms.  | Toggles between `ACTIVE` and `BYPASSED` states.                                |
| `FSW1_SHORT_PRESSED` | Footswitch 1 is held for > 500ms.              | Enters `FROZEN` mode from `ACTIVE`. This is a momentary action.                |
| `FSW1_RELEASED`      | Footswitch 1 is released after a long press.   | Exits `FROZEN` mode and returns to `ACTIVE`.                                   |
| `FSW2_LONG_PRESSED`  | Footswitch 2 is held for > 1 second.           | From the `ACTIVE` state, enters the expression pedal assignment mode.          |
| `FSW2_RELEASED`      | Footswitch 2 is released.                      | If in `EXPR_ASSIGN` mode, confirms assignment and returns to normal operation. |

### Global Actions (Non-State-Changing)

These actions can be triggered from multiple states but do not cause a state transition themselves.

| Action               | Trigger                       | Description                                      |
| -------------------- | ----------------------------- | ------------------------------------------------ |
| Tap Tempo            | `FSW2_TAPPED`                 | Sets the tempo for synced rhythm modes.          |
| Panic / Clear Buffer | `FSW2_SHORT_PRESSED` (>500ms) | Clears the SDRAM delay buffers.                  |
| Input Clipping       | `INPUT_CLIPPING`              | Triggers a visual warning (`LED 1` Red flicker). |

### State Definitions

#### **1. `BOOTING`**

- **Description:** The initial state on power-up. Handles loading of persistent settings.
- **Entry Actions:**
  - Check if `Footswitch 1` is held down.
  - If `FS1` is held:
    - Toggle the `BypassMode` setting (True/Trails) in persistent QSPI flash storage.
    - Flash `LED 1` (White) rapidly for 200ms to confirm the change.
  - Load the current `BypassMode` from storage.
- **Transitions:**
  - On `BOOT_COMPLETE` -> Transition to `BYPASSED`.

#### **2. `BYPASSED`**

- **Description:** The effect is not applied to the signal. Behavior depends on the persistent `BypassMode`.
- **Entry Actions:**
  - If `BypassMode` is **True Bypass**:
    - Engage hardware relays using `dripline::AudioBypass`.
    - Turn `LED 1` Off.
  - If `BypassMode` is **Trails Bypass**:
    - Set the engines bypassed param to true.
    - Set `LED 1` to Dim Red.
- **Transitions:**
  - On `FSW1_TAPPED` -> Transition to `ACTIVE`.
- **Exit Actions:**
  - If exiting from a True Bypass state, disengage hardware relays.

#### **3. `ACTIVE`**

- **Description:** The granular engine is actively processing the live input.
- **Entry Actions:**
  - Set `LED 1` to Solid Green.
  - If `BypassMode` is **True Bypass**:
    - Disengage hardware relays using `dripline::AudioBypass`.
  - If `BypassMode` is **Trails Bypass**:
    - Set the engines bypassed param to false.
- **Internal Activities:**
  - Update the engine params using knob and toggle values.
  - Update `LED 2` color and brightness based on tempo subdivision and grain count.
  - On `INPUT_CLIPPING`, briefly flicker `LED 1` Red.
- **Transitions:**
  - On `FSW1_TAPPED` -> Transition to `BYPASSED`.
  - On `FSW1_SHORT_PRESSED` -> Transition to `FROZEN`.
  - On `FSW2_LONG_PRESSED` -> Transition to `EXPR_ASSIGN`.

### **4 `FROZEN`**

- **Description:** The delay line buffer is locked, and no new audio is recorded. The engine granulates the captured loop. This is a momentary state, active only while the footswitch is held.
- **Entry Actions:**
  - Set `LED 1` to Solid Blue.
  - Set the engine's freeze parameter to true.
- **Internal Activities:**
  - Continue to trigger and process grains from the existing (frozen) buffer content.
  - Parameters like Pitch, Sprays, and Tone continue to affect the output.
- **Exit Actions:**
  - Set `LED 1` to off (it will be immediately set to Green upon re-entry to `ACTIVE`).
  - Set the engine's freeze parameter to false.
- **Transitions:**
  - On `FSW1_RELEASED` -> Transition to `ACTIVE`.

### **5 `EXPR_ASSIGN`**

- Entered on `FSW2_LONG_PRESSED` from the `ACTIVE` state.
- Continuously monitor `KNOB_10`.
- Quantize the knob's value to select a parameter target (e.g., Pitch, Duration, Feedback).
- Update `LED 2` color to match the currently selected parameter target, providing real-time feedback.
- **Transitions:**
  - On `FSW2_RELEASED`:
    - **Action:** Finalize the assignment of the currently selected parameter to the expression pedal.
    - **Action:** Save the new assignment to persistent QSPI flash storage.
    - **Action:** Blink `LED 2` with the assigned parameter's color to confirm.
    - **Transition:** Return to the `ACTIVE` state.
