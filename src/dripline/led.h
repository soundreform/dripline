#pragma once
#ifndef DL_LED_H
#define DL_LED_H

#include <algorithm>
#include <array>

#include "daisy.h"
#include "daisysp.h"
#include "dripline.h"

using daisy::Color;
using daisy::System;
using daisysp::fclamp;
using dripline::Dripline;

namespace dripline {

class LedManager {
  public:
    class Animation {
      public:
        Animation() = default;
        virtual ~Animation() = default;
        virtual Color Process(uint32_t now) = 0;
    };

    /**
     * @brief An animation that keeps the LED off.
     */
    class Off : public Animation {
      public:
        Off() = default;
        ~Off() override = default;

        Color Process(uint32_t now) override {
            return color_;
        }

      private:
        Color color_ = Color(0.0f, 0.0f, 0.0f);
    };

    /**
     * @brief An animation that holds an LED at a constant, solid color.
     */
    class Solid : public Animation {
      public:
        /**
         * @brief Constructs a Solid animation.
         * @param color The solid color to display.
         */
        Solid(Color color) : color_(color) {
        }
        ~Solid() override = default;

        /**
         * @brief Sets the color of the solid animation.
         * @param color The new solid color.
         */
        void SetColor(Color color) {
            color_ = color;
        }

        Color Process(uint32_t now) override {
            return color_;
        }

      private:
        Color color_;
    };

    /**
     * @brief An animation that blinks an LED on and off.
     * The total period of the blink is 2 * half_period_ms.
     */
    class Blink : public Animation {
      public:
        /**
         * @brief Constructs a Blink animation.
         * @param color The color to display when the LED is on.
         * @param half_period_ms The duration of the 'on' state and the 'off'
         *                       state, in milliseconds. The total period is
         *                       twice this value.
         */
        Blink(Color color, uint32_t half_period_ms) : on_color_(color), half_period_ms_(half_period_ms) {
        }
        ~Blink() override = default;

        /**
         * @brief Sets the 'on' color of the blink.
         * @param c The new color.
         */
        void SetColor(Color c) {
            on_color_ = c;
        }

        /**
         * @brief Sets the rate of the blink.
         * @param half_period_ms The new half-period in milliseconds.
         */
        void SetPeriod(uint32_t half_period_ms) {
            half_period_ms_ = half_period_ms > 0 ? half_period_ms : 1;
        }

        Color Process(uint32_t now) override {
            if (half_period_ms_ == 0) {
                return off_color_;
            }
            uint32_t elapsed = now - last_updated_;
            if (elapsed >= half_period_ms_) {
                // Catch up if multiple periods have passed since the last update.
                uint32_t num_toggles = elapsed / half_period_ms_;
                if (num_toggles % 2 != 0) {
                    active_ = !active_;
                }
                // Advance last_updated_ by the number of full periods that have passed.
                last_updated_ += num_toggles * half_period_ms_;
            }

            return active_ ? on_color_ : off_color_;
        }

      private:
        Color on_color_;
        Color off_color_ = Color(0.0f, 0.0f, 0.0f);
        uint32_t half_period_ms_;
        uint32_t last_updated_ = 0;
        bool active_ = false;
    };

    /**
     * @brief An animation that pulses the brightness of an LED using a sine wave.
     */
    class Pulse : public Animation {
      public:
        /**
         * @brief Constructs a Pulse animation.
         * @param color The base color of the pulse.
         * @param rate The period of the pulse in milliseconds.
         */
        Pulse(Color color, uint32_t rate) : color_(color), rate_ms_(rate) {
        }
        ~Pulse() override = default;

        /**
         * @brief Sets the color of the pulse.
         * @param c The new color.
         */
        void SetColor(Color c) {
            color_ = c;
        }

        /**
         * @brief Sets the rate of the pulse.
         * @param rate_ms The new period of the pulse in milliseconds.
         */
        void SetRate(uint32_t rate_ms) {
            rate_ms_ = rate_ms > 0 ? rate_ms : 1;
        }

        /**
         * @brief Sets the rate of the pulse from a frequency in Hz.
         * @param rate_hz The new frequency of the pulse in Hz.
         */
        void SetRate(float rate_hz) {
            if (rate_hz > 0.0f) {
                rate_ms_ = static_cast<uint32_t>(1000.0f / rate_hz);
            } else {
                rate_ms_ = 0;
            }
        }

        /**
         * @brief Sets an overall brightness multiplier for the pulse.
         * @param brightness A float from 0.0 to 1.0.
         */
        void SetBrightness(float brightness) {
            brightness_override_ = fclamp(brightness, 0.0f, 1.0f);
        }

        Color Process(uint32_t now) override {
            if (rate_ms_ == 0) {
                return Color(0, 0, 0);
            }

            float phase = fmodf((float)now, (float)rate_ms_) / (float)rate_ms_;
            float pulse_brightness = 0.5f + (sinf(phase * TWOPI_F) * 0.5f);
            return color_ * (pulse_brightness * brightness_override_);
        }

      private:
        Color color_;
        uint32_t rate_ms_;
        float brightness_override_ = 1.0f; // Default to full brightness
    };

    struct AnimationStep {
        Animation *animation;
        uint32_t duration_ms;
    };

    LedManager() = default;

    void Init(Dripline *hw) {
        hw_ = hw;
        for (size_t i = 0; i < Dripline::LED_LAST; i++) {
            animations_[i] = &default_off_animation_;
            sequences_[i].is_active = false;
        }
    }

    /**
     * @brief Sets a persistent animation for an LED.
     *
     * This will cancel any active animation sequence on the given LED.
     * The provided animation will become the active one until a new
     * animation or sequence is set.
     *
     * @param led The LED to set the animation on.
     * @param animation A pointer to the LedAnimation to use.
     */
    void Set(Dripline::Led led, Animation *animation) {
        // If a temporary sequence is active, do nothing. The persistent
        // animation will be set by a subsequent call to Set() in the main
        // loop after the sequence has completed.
        if (sequences_[led].is_active) {
            return;
        }
        animations_[led] = animation;
    }

    /**
     * @brief Sets a single animation to run for a specific duration.
     *
     * This is a convenience method that creates a one-step sequence. After the
     * duration, the animation will become the persistent animation for the LED.
     * To run this animation in a blocking manner, call `Update(true)`
     * immediately after this.
     *
     * @param led The LED to set the animation on.
     * @param animation A pointer to the LedAnimation to use.
     * @param duration_ms The duration in milliseconds to run the animation.
     */
    void Set(Dripline::Led led, Animation *animation, uint32_t duration_ms) {
        AnimationStep step = {animation, duration_ms};
        SetSequence(led, &step, 1);
    }

    /**
     * @brief Sets a sequence of animations to play on an LED.
     *
     * Each step in the sequence has an animation and a duration. The sequence
     * will play through each animation for its specified duration. After the
     * final animation in the sequence completes, it will remain as the active
     * animation for the LED.
     *
     * @note The sequence is copied into an internal buffer. Sequences longer
     * than `AnimationSequence::kMaxSteps` will be truncated.
     *
     * @param led The LED to run the sequence on.
     * @param steps A pointer to an array of AnimationStep.
     * @param num_steps The number of steps in the array.
     */
    void SetSequence(Dripline::Led led, const AnimationStep *steps, size_t num_steps) {
        if (steps && num_steps > 0) {
            auto &seq = sequences_[led];
            seq.num_steps = std::min(num_steps, AnimationSequence::kMaxSteps);
            std::copy(steps, steps + seq.num_steps, seq.steps.begin());
            seq.current_step_index = 0;
            seq.step_start_time = daisy::System::GetNow();
            seq.is_active = true;
        } else {
            sequences_[led].is_active = false;
        }
    }

    /**
     * @brief Sets a sequence of animations to play on an LED using std::array.
     *
     * This is a convenience overload that accepts a std::array, providing
     * compile-time safety for the sequence size.
     *
     * @tparam N The size of the animation sequence array.
     * @param led The LED to run the sequence on.
     * @param sequence A const reference to a std::array of AnimationStep.
     */
    template <size_t N>
    void SetSequence(Dripline::Led led, const std::array<AnimationStep, N> &sequence) {
        SetSequence(led, sequence.data(), sequence.size());
    }

    /**
     * @brief Updates the state of all LEDs. Can optionally block until sequences complete.
     *
     * By default, this method performs a single non-blocking update pass.
     * If `block_until_sequences_complete` is true, it will enter a loop,
     * continuously updating LEDs until all active animation sequences on all
     * LEDs are finished. This is useful for simple, linear flows like a
     * boot-up confirmation flash.
     *
     * @param block_until_sequences_complete If true, the function will not
     * return until all active sequences are done. Defaults to false.
     */
    void Update(bool block_until_sequences_complete = false) {
        UpdatePass();

        if (block_until_sequences_complete) {
            bool sequences_are_active;
            do {
                sequences_are_active = false;
                for (size_t i = 0; i < Dripline::LED_LAST; i++) {
                    if (sequences_[i].is_active) {
                        sequences_are_active = true;
                        break;
                    }
                }

                if (sequences_are_active) {
                    UpdatePass();
                    hw_->DelayMs(1);
                }
            } while (sequences_are_active);
        }
    }

    bool IsSequenceActive(Dripline::Led led) const {
        return sequences_[led].is_active;
    }

  private:
    void UpdatePass() {
        uint32_t now = System::GetNow();
        bool updated = false;

        for (size_t i = 0; i < Dripline::LED_LAST; i++) {
            auto &seq = sequences_[i];
            Animation *animation_to_process;

            if (seq.is_active) {
                uint32_t current_step_duration = seq.steps[seq.current_step_index].duration_ms;
                if (now - seq.step_start_time > current_step_duration) {
                    if (seq.current_step_index < seq.num_steps - 1) {
                        seq.current_step_index++;
                        seq.step_start_time = now;
                    } else {
                        animations_[i] = seq.steps[seq.current_step_index].animation;
                        seq.is_active = false;
                    }
                }
            }

            if (seq.is_active) {
                animation_to_process = seq.steps[seq.current_step_index].animation;
            } else {
                animation_to_process = animations_[i];
            }

            Color c = animation_to_process->Process(now);
            Color cc = current_colors_[i];

            if (c.Red() != cc.Red() || c.Green() != cc.Green() || c.Blue() != cc.Blue()) {
                hw_->SetLed(static_cast<Dripline::Led>(i), c.Red(), c.Green(), c.Blue());
                current_colors_[i] = c;
                updated = true;
            }
        }

        if (updated) {
            hw_->UpdateLeds();
        }
    }

    struct AnimationSequence {
        static constexpr size_t kMaxSteps = 8;
        std::array<AnimationStep, kMaxSteps> steps;
        size_t num_steps;
        size_t current_step_index;
        uint32_t step_start_time;
        bool is_active;
    };

    Dripline *hw_;
    Animation *animations_[Dripline::LED_LAST];
    AnimationSequence sequences_[Dripline::LED_LAST];
    Color current_colors_[Dripline::LED_LAST];
    Off default_off_animation_;
};
} // namespace dripline

#endif
