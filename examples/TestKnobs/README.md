# TestKnobs

## Description

A test utility to verify the functionality of all 12 analog knobs. It uses Knobs 1-6 to control the blink frequency (0Hz to 4Hz) of the individual R, G, and B components for LED_1 and LED_2. Knobs 7-12 control the base intensity/brightness of those same components. Audio is passed through without modification.

### Controls

| CONTROL               | DESCRIPTION            | COMMENTS                                             |
| --------------------- | ---------------------- | ---------------------------------------------------- |
| KNOB_1                | LED_1 Red Blink Rate   | Range: 0 - 4.0 Hz                                    |
| KNOB_2                | LED_1 Green Blink Rate | Range: 0 - 4.0 Hz                                    |
| KNOB_3                | LED_1 Blue Blink Rate  | Range: 0 - 4.0 Hz                                    |
| KNOB_4                | LED_2 Red Blink Rate   | Range: 0 - 4.0 Hz                                    |
| KNOB_5                | LED_2 Green Blink Rate | Range: 0 - 4.0 Hz                                    |
| KNOB_6                | LED_2 Blue Blink Rate  | Range: 0 - 4.0 Hz                                    |
| KNOB_7                | LED_1 Red Intensity    | 0.0 - 1.0                                            |
| KNOB_8                | LED_1 Green Intensity  | 0.0 - 1.0                                            |
| KNOB_9                | LED_1 Blue Intensity   | 0.0 - 1.0                                            |
| KNOB_10               | LED_2 Red Intensity    | 0.0 - 1.0                                            |
| KNOB_11               | LED_2 Green Intensity  | 0.0 - 1.0                                            |
| KNOB_12               | LED_2 Blue Intensity   | 0.0 - 1.0                                            |
| EXP                   |                        |                                                      |
| TOGGLE_SWITCH_1       |                        |                                                      |
| TOGGLE_SWITCH_2       |                        |                                                      |
| TOGGLE_SWITCH_3       |                        |                                                      |
| TOGGLE_SWITCH_4       |                        |                                                      |
| TOGGLE_SWITCH_5       |                        |                                                      |
| TOGGLE_SWITCH_6       |                        |                                                      |
| LED_1                 | RGB Visualizer         | Controlled by Knobs 1-3 (Rate) and 7-9 (Intensity)   |
| LED_2                 | RGB Visualizer         | Controlled by Knobs 4-6 (Rate) and 10-12 (Intensity) |
| FOOTSWITCH_1          |                        |                                                      |
| FOOTSWITCH_2          |                        |                                                      |
| JACK_SWITCH_LEFT_IN   |                        |                                                      |
| JACK_SWITCH_LEFT_OUT  |                        |                                                      |
| JACK_SWITCH_RIGHT_IN  |                        |                                                      |
| JACK_SWITCH_RIGHT_OUT |                        |                                                      |
