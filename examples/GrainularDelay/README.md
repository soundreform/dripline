# GrainularDelay

## Description

GrainularDelay is a granular effects processor for the Daisy Seed platform. It operates by capturing incoming audio into a buffer and replaying small segments, called "grains," from that buffer. Users have control over grain playback parameters like pitch, duration, density, and direction. This allows for a wide range of effects, including rhythmic delays, pitch-shifting, ambient textures, and time-stretching. The pedal also features multiple stereo routing options, tempo synchronization, and on-board modulation to further shape the sound.

### Controls

| CONTROL         | DESCRIPTION   | COMMENTS                                                        |
| --------------- | ------------- | --------------------------------------------------------------- |
| KNOB_1          | Pitch         | Master transposition (-2 to +2 octaves)                         |
| KNOB_2          | Duration      | Duration of each grain (10ms to 500ms)                          |
| KNOB_3          | Rate          | Trigger rate (Async Freq or Tap Subdivision)                    |
| KNOB_4          | Time Spray    | Randomizes read position jitter (0 to 500ms)                    |
| KNOB_5          | Pitch Spray   | Randomizes per-grain micro-tuning/octaves                       |
| KNOB_6          | Pan Spread    | Controls stereo width of grain cloud                            |
| KNOB_7          | Feedback      | Amount of grain mix fed back to delay lines                     |
| KNOB_8          | Drive         | Input gain/saturation into the granular engine                  |
| KNOB_9          | Tone          | Bipolar Filter (LP < 12 o'clock > HP)                           |
| KNOB_10         | Expr Depth    | Bipolar: Sets expression pedal sweep range & polarity           |
| KNOB_11         | Output Lvl    | Master makeup gain                                              |
| KNOB_12         | Mix           | Dry / Wet blend                                                 |
| TOGGLE_SWITCH_1 | Routing       | UP: Parallel, CENTER: Ping-Pong, DOWN: Series                   |
| TOGGLE_SWITCH_2 | Quantize      | UP: Free, CENTER: Semitones, DOWN: Oct/5ths                     |
| TOGGLE_SWITCH_3 | Direction     | UP: Forward, CENTER: Random, DOWN: Reverse                      |
| TOGGLE_SWITCH_4 | Mod Depth     | UP: Some, CENTER: None, DOWN: More                              |
| TOGGLE_SWITCH_5 | Window        | UP: Pluck, CENTER: Smooth, DOWN: Swell                          |
| TOGGLE_SWITCH_6 | Rhythm        | UP: Free (Async), CENTER: Synced, DOWN: Burst                   |
| LED_1           | Status        | Green: Active, Blue: Freeze, Red: Clipping/Bypass               |
| LED_2           | Tempo         | Pulses at clock rate; Color shows subdivision                   |
| FOOTSWITCH_1    | Bypass/Freeze | Tap: Toggle Bypass, Hold: Momentary Freeze                      |
| FOOTSWITCH_2    | Tap/Function  | Tap: Tap Tempo, Hold (0.5s): Clear/Panic, Hold (1s): EXP Assign |

### Boot Gestures

- **Toggle Bypass Mode:** Hold **FOOTSWITCH_1** while powering on to toggle between **True Bypass** (relay) and **Trails Bypass** (DSP remains active to allow ring-out).

### Expression Pedal Assignment

To assign a parameter to the expression pedal:

1.  Press and hold **FOOTSWITCH_2** for more than one second to enter EXP Assign mode.
2.  **LED 2** will pulse a color corresponding to the currently selected parameter.
3.  Turn **KNOB_10** to select the parameter you wish to control. The color of **LED 2** will change to indicate the selected target:
    - **White:** Unassigned
    - **Blue:** Pitch
    - **Green:** Duration
    - **Yellow:** Density
    - **Cyan:** Time Spray
    - **Magenta:** Pitch Spray
    - **Orange:** Feedback
4.  Release **FOOTSWITCH_2** to save the assignment. **LED 2** will flash to confirm the new setting.

During normal operation, **KNOB_10** controls the depth and polarity of the expression pedal's sweep for the assigned parameter.

### Example Settings

#### 1. Lush Ambient Clouds

Create a soft, evolving texture that washes behind your playing.

- **Routing:** Parallel (UP)
- **Window:** Smooth (CENTER)
- **Pitch:** 1.0 (Neutral)
- **Grain Size:** ~2 o'clock (300ms)
- **Density:** ~3 o'clock (Dense)
- **Feedback:** 2 o'clock
- **Tone:** 11 o'clock (Gentle LP)
- **Pan Spread:** Max

#### 2. Rhythmic Glitch Shifter

Perfect for percussive inputs or adding rhythmic movement.

- **Routing:** Ping-Pong (CENTER)
- **Rhythm Mode:** Synced (CENTER)
- **Window:** Pluck (UP)
- **Quantize:** Semitones (CENTER)
- **Pitch:** +1 Octave
- **Density:** Set via Tap Tempo
- **Pitch Spray:** 9 o'clock (Occasional octave jumps)

#### 3. Reverse Swell Shimmer

Ethereal reverse textures with pitch shifting.

- **Routing:** Series (DOWN)
- **Direction:** Reverse (DOWN)
- **Window:** Swell (DOWN)
- **Pitch:** +1 Octave
- **Grain Size:** 12 o'clock
- **Feedback:** 3 o'clock
- **Mix:** 50%

#### 4. Frozen Time Stretcher

Turn a single note into an infinite, evolving drone.

- **Routing:** Series (DOWN)
- **Freeze:** Active (Hold FSW1)
- **Time Spray:** 12 o'clock (Adds movement to the frozen loop)
- **Pitch Spray:** 10 o'clock
- **Tone:** Sweep for filter effects

### LED Indicators

**LED 1 (Status):**

- **Solid Green:** Effect is active.
- **Solid Blue:** Freeze is active (buffer is locked).
- **Dim Red:** Bypassed with Trails active (DSP is ringing out).
- **Off:** True Bypass active.
- **Flicker Red:** Input or Feedback clipping detected.

**LED 2 (Tempo):**

- **Color:** Indicates subdivision (White: 1/4, Yellow: Dotted 1/8, Cyan: 1/8, Magenta: Triplet, Blue: 1/16).
- **Brightness:** Modulated by the number of active grains.
