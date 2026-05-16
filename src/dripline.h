#pragma once
#ifndef DL_H
#define DL_H

#include "daisy.h"
#include "daisy_seed.h"

using daisy::AnalogControl;
using daisy::AudioHandle;
using daisy::DaisySeed;
using daisy::GPIO;
using daisy::LedDriverPca9685;
using daisy::Pin;
using daisy::SaiHandle;
using daisy::Switch;
using daisy::System;

namespace dripline
{
    /**
     * @brief Driver for 74HC165 8-bit parallel-in/serial-out shift registers.
     *
     * This class manages a chain of shift registers, allowing multiple digital
     * inputs to be read using only three GPIO pins.
     *
     * @tparam num_devices The number of 8-bit shift registers in the chain.
     */
    template <size_t num_devices>
    class ShiftRegister165
    {
    public:
        ShiftRegister165() = default;
        ~ShiftRegister165() = default;

        /**
         * @brief Initializes the shift register GPIO pins.
         *
         * @param clock_pin Pin connected to the Clock (CP) line.
         * @param data_pin Pin connected to the Serial Data Output (Q7) line.
         * @param latch_pin Pin connected to the Parallel Load (PL) line.
         */
        void Init(Pin clock_pin, Pin data_pin, Pin latch_pin)
        {
            clock_.Init(clock_pin, GPIO::Mode::OUTPUT);
            data_.Init(data_pin, GPIO::Mode::INPUT);
            latch_.Init(latch_pin, GPIO::Mode::OUTPUT);

            memset(state_, false, sizeof(state_));
        }

        /**
         * @brief Captures the current state of all parallel inputs and shifts them into internal memory.
         *
         * This method should be called regularly to refresh the cached input states.
         */
        void Update()
        {
            latch_.Write(false);
            latch_.Write(true);

            for (size_t i = 0; i < num_devices * 8; i++)
            {
                state_[i] = data_.Read();
                clock_.Write(true);
                clock_.Write(false);
            }
        }

        /**
         * @brief Returns the last read state of a specific input bit.
         *
         * @param idx The index of the bit to retrieve (0 to num_devices * 8 - 1).
         * @return true if the input was high, false if low or if idx is out of bounds.
         */
        bool Get(size_t idx)
        {
            if (idx < num_devices * 8)
            {
                return state_[idx];
            }
            else
            {
                return false;
            }
        }

    private:
        GPIO clock_;
        GPIO data_;
        GPIO latch_;
        bool state_[num_devices * 8];
    };

    /**
     * @brief Represents a digital control, typically a three-position toggle switch.
     *
     * This class reads two bits from a shift register to determine if a switch is
     * in the UP, DOWN, or CENTER (off) position.
     */
    class DigitalControl
    {
    public:
        /** @brief Possible positions of the digital control. */
        enum class Position
        {
            CENTER, /**< Switch is in the middle (off) position. */
            UP,     /**< Switch is in the UP position. */
            DOWN,   /**< Switch is in the DOWN position. */
        };

        DigitalControl() = default;
        ~DigitalControl() = default;

        /**
         * @brief Initializes the digital control with its associated shift register and pin indices.
         *
         * @param sr Pointer to the shift register instance.
         * @param up_idx The bit index in the shift register corresponding to the UP position.
         * @param down_idx The bit index in the shift register corresponding to the DOWN position.
         */
        void Init(ShiftRegister165<2> *sr, uint8_t up_idx, uint8_t down_idx);

        /**
         * @brief Reads the current state from the shift register and returns the position.
         *
         * If only the UP bit is set, returns Position::UP.
         * If only the DOWN bit is set, returns Position::DOWN.
         * Otherwise (both bits set or neither bit set), returns Position::CENTER.
         *
         * @return The current physical position of the switch.
         */
        Position GetPosition();

    private:
        ShiftRegister165<2> *sr_;
        uint8_t up_idx_;
        uint8_t down_idx_;
    };

    /**
     * @brief Main hardware interface for the Dripline audio module.
     *
     * Provides a high-level API to interact with the Daisy Seed, knobs, toggle switches,
     * footswitches, jack detection, and multi-color LEDs.
     */
    class Dripline
    {
    public:
        /** @brief Indices for the 12 analog knobs. */
        enum Knob
        {
            KNOB_1,
            KNOB_2,
            KNOB_3,
            KNOB_4,
            KNOB_5,
            KNOB_6,
            KNOB_7,
            KNOB_8,
            KNOB_9,
            KNOB_10,
            KNOB_11,
            KNOB_12,
            KNOB_LAST,
        };

        /** @brief Indices for the 6 three-position toggle switches. */
        enum ToggleSwitch
        {
            TOGGLE_SWITCH_1,
            TOGGLE_SWITCH_2,
            TOGGLE_SWITCH_3,
            TOGGLE_SWITCH_4,
            TOGGLE_SWITCH_5,
            TOGGLE_SWITCH_6,
            TOGGLE_SWITCH_LAST,
        };

        /** @brief Indices for the 2 footswitches. */
        enum Footswitch
        {
            FOOTSWITCH_1,
            FOOTSWITCH_2,
            FOOTSWITCH_LAST,
        };

        /** @brief Indices for the jack detection switches. */
        enum JackSwitch
        {
            JACK_SWITCH_LEFT_IN,
            JACK_SWITCH_RIGHT_IN,
            JACK_SWITCH_LEFT_OUT,
            JACK_SWITCH_RIGHT_OUT,
            JACK_SWITCH_LAST,
        };

        /** @brief Indices for the RGB LEDs. */
        enum Led
        {
            LED_1,
            LED_2,
            LED_LAST,
        };

        /** @brief Audio channels. */
        enum Channel
        {
            CHANNEL_LEFT,
            CHANNEL_RIGHT,
            CHANNEL_LAST,
        };

        Dripline() = default;
        ~Dripline() = default;

        /**
         * @brief Initializes the hardware peripherals.
         * @param boost If true, sets the Daisy Seed to its maximum clock speed.
         */
        void Init(bool boost = false);

        /**
         * @brief Blocking delay in milliseconds.
         * @param del Time to wait in ms.
         */
        void DelayMs(size_t del);

        /**
         * @brief Starts the audio engine with the provided callback.
         * @param cb Function to be called for every audio block.
         */
        void StartAudio(AudioHandle::AudioCallback cb);

        /**
         * @brief Switches the active audio callback.
         * @param cb The new callback function.
         */
        void ChangeAudioCallback(AudioHandle::AudioCallback cb);

        /** @brief Stops the audio engine. */
        void StopAudio();

        /**
         * @brief Configures the audio sample rate.
         * @param samplerate The sample rate (e.g., SAI_48K).
         */
        void SetAudioSampleRate(SaiHandle::Config::SampleRate samplerate);

        /** @return Current audio sample rate in Hz. */
        float AudioSampleRate();

        /**
         * @brief Configures the audio block size.
         * @param size Number of samples per block.
         */
        void SetAudioBlockSize(size_t size);

        /** @return Current audio block size. */
        size_t AudioBlockSize();

        /** @return Frequency at which the audio callback is triggered. */
        float AudioCallbackRate();

        /** @brief Starts background ADC conversions for analog controls. */
        void StartAdc();

        /** @brief Stops ADC conversions. */
        void StopAdc();

        /** @brief Samples the current state of knobs and the expression pedal. */
        void ProcessAnalogControls();

        /** @brief Updates the state of toggle switches and debounces footswitches/jacks. */
        void ProcessDigitalControls();

        /** @brief Helper to process both analog and digital inputs in one call. */
        inline void ProcessAllControls()
        {
            ProcessAnalogControls();
            ProcessDigitalControls();
        }

        /** @brief Sends updated LED colors to the PCA9685 driver via DMA. */
        void UpdateLeds();

        /**
         * @brief Sets the color of a specific LED.
         * @param led The LED index.
         * @param r Red component (0.0 to 1.0).
         * @param g Green component (0.0 to 1.0).
         * @param b Blue component (0.0 to 1.0).
         */
        void SetLed(Led led, float r, float g, float b);

        /** @brief Handle to the underlying Daisy Seed hardware. */
        DaisySeed seed;
        /** @brief Array of 12 smoothed analog knob inputs. */
        AnalogControl knobs[KNOB_LAST];
        /** @brief Smoothed input for the expression pedal jack. */
        AnalogControl expression;
        /** @brief State managers for the 6 toggle switches. */
        DigitalControl toggles[TOGGLE_SWITCH_LAST];
        /** @brief Debounced hardware footswitches. */
        Switch footswitches[FOOTSWITCH_LAST];
        /** @brief Debounced jack detection switches. */
        Switch jacks[JACK_SWITCH_LAST];
        /** @brief Control pins for the bypass relays. */
        GPIO relays[CHANNEL_LAST];
        /** @brief Control pins for the hardware audio mutes. */
        GPIO mutes[CHANNEL_LAST];

    private:
        /** @brief Shift register reading the toggle switches. */
        ShiftRegister165<2> toggle_sr_;
        /** @brief Driver for the RGB LEDs. */
        LedDriverPca9685<1, true> led_driver_;
        /** @brief Pin for LED power. */
        GPIO led_power_;

        /** @brief Updates sample rates for control smoothing. */
        void SetHidUpdateRates();
        /** @brief Configures the ADC and initializes knob inputs. */
        void InitAnalogControls();
        /** @brief Configures shift registers and GPIO for digital inputs. */
        void InitDigitalControls();
        /** @brief Initializes the PCA9685 LED driver. */
        void InitLeds();
        /** @brief Initializes bypass relay and mute pins. */
        void InitBypasses();
    };
};

#endif
