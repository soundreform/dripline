# TestJackSwitches

## Description

A diagnostic utility designed to verify the functionality of the hardware's jack detection switches. This example monitors the insertion state of the audio jacks and provides visual feedback via the onboard LEDs to confirm that the hardware is correctly detecting plug connections.

### Controls

| CONTROL               | DESCRIPTION  | COMMENTS                                          |
| --------------------- | ------------ | ------------------------------------------------- |
| KNOB_1 - 12           | Unused       | No effect in this test                            |
| EXP                   | Unused       | No effect in this test                            |
| TOGGLE_SWITCH_1 - 6   | Unused       | No effect in this test                            |
| LED_1                 | Left Status  | Lights up when a plug is detected in a Left jack  |
| LED_2                 | Right Status | Lights up when a plug is detected in a Right jack |
| FOOTSWITCH_1          | Unused       |                                                   |
| FOOTSWITCH_2          | Unused       |                                                   |
| JACK_SWITCH_LEFT_IN   | Detection    | Triggers LED_1 when a cable is inserted           |
| JACK_SWITCH_LEFT_OUT  | Detection    | Triggers LED_1 when a cable is inserted           |
| JACK_SWITCH_RIGHT_IN  | Detection    | Triggers LED_2 when a cable is inserted           |
| JACK_SWITCH_RIGHT_OUT | Detection    | Triggers LED_2 when a cable is inserted           |
