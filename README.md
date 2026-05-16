# Dripline

Dripline is a hardware abstraction layer (HAL) for SoundReform's audio hardware modules, built on top of the [libDaisy](https://github.com/electro-smith/libDaisy) library for the Daisy Seed.

It provides a high-level C++ API for interacting with various hardware components including analog knobs, toggle switches, footswitches, jack detection, and RGB LEDs.

## Features

- **Analog Controls**: Support for 12 smoothed analog knobs and an expression pedal input.
- **Toggle Switches**: Driver for 6 three-position (UP/CENTER/DOWN) toggle switches interfaced via 74HC165 shift registers.
- **Footswitches**: Support for 2 hardware footswitches with debouncing.
- **Jack Detection**: Automatic detection for Left/Right Input and Output jacks.
- **LED Control**: Driver for multi-color RGB LEDs via the PCA9685 PWM controller.
- **Audio Utilities**: Easy configuration of audio sample rates, block sizes, and hardware bypass/mute relays.

## Directory Structure

- `src/`: Core library source files.
- `template/`: Boilerplate project files used by the creation script.
- `vendor/`: External dependencies, including `libDaisy`.
- `createproject.py`: Utility script to bootstrap new firmware projects.

## Getting Started

### Prerequisites

- Python 3.x
- Daisy Toolchain (arm-none-eabi-gcc, make, openocd).

### Creating a New Project

Use the `createproject.py` script to generate a new project folder pre-configured with the Dripline library and necessary build settings (including VS Code configuration).

#### Basic Usage

Run the script from the root of the repository:

```bash
python3 createproject.py --proj_name MyNewEffect
```

This will create a new project in `./projects/MyNewEffect`.

#### Command Line Arguments

| Argument      | Description                                      | Default       |
| :------------ | :----------------------------------------------- | :------------ |
| `--proj_name` | **Required**. The name of your new project.      | N/A           |
| `--location`  | The directory where the project will be created. | `./projects/` |

#### Example: Custom Location

```bash
python3 createproject.py --proj_name DelayUnit --location ./my_work/
```

## Programming Example

Once a project is created, you can interact with the hardware using the `Dripline` class:

```cpp
#include "dripline.h"

dripline::Dripline hw;

void AudioCallback(AudioHandle::InputBuffer in, AudioHandle::OutputBuffer out, size_t size) {
    hw.ProcessAllControls();
    float gain = hw.knobs[dripline::Dripline::KNOB_1].Value();

    for (size_t i = 0; i < size; i++) {
        out[0][i] = in[0][i] * gain;
    }
}

int main(void) {
    hw.Init();
    hw.StartAudio(AudioCallback);
    while(1) {}
}
```

## License

Dripline uses the MIT license.

It can be used in both closed source and commercial projects, and does not provide a warranty of any kind.

For the full license, read the LICENSE file in the root directory.
