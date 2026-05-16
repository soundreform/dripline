#pragma once
#ifndef DL_RAMP_H
#define DL_RAMP_H

namespace dripline {
/**
 * @brief A linear ramp generator typically used for audio faders or internal timing.
 *
 * The Ramp class produces a normalized (0.0 to 1.0) linear ramp over a
 * specified duration. It supports bi-directional ramping and is intended
 * to be updated sample-by-sample.
 */
class Ramp {
  public:
    /** @brief Possible directions for the ramp. */
    enum class Dir {
        DOWN, /**< Ramp towards 0.0. */
        UP,   /**< Ramp towards 1.0. */
    };

    /**
     * @brief Configures the ramp with the current system state.
     * @param sample_rate The current audio sample rate in Hz.
     * @param duration    The duration of the full 0.0 to 1.0 ramp in milliseconds.
     * @param direction   The initial direction of the ramp (UP or DOWN).
     */
    void Init(float sample_rate, float duration, Dir direction) {
        val_ = 0.0f;
        step_ = 1.0f / ((duration / 1000.0f) * sample_rate);
        dir_ = direction;
        Reset();
    }

    /**
     * @brief Updates the ramp state for a single sample and returns the result.
     *
     * This method increments or decrements the internal value based on the current
     * direction and the pre-calculated step size.
     *
     * @return The current ramp value, clamped between 0.0 and 1.0.
     */
    float Process() {
        switch (dir_) {
        case Dir::UP:
            val_ += step_;
            if (val_ > 1.0f) {
                val_ = 1.0f;
            }
            break;
        case Dir::DOWN:
            val_ -= step_;
            if (val_ < 0.0f) {
                val_ = 0.0f;
            }
            break;
        }

        return val_;
    }

    /**
     * @brief Retrieves the current ramp value without updating it.
     * @return The current value in the range [0.0, 1.0].
     */
    float Value() const {
        return val_;
    }

    /**
     * @brief Resets the internal value to the starting point.
     * Resets to 0.0 if direction is UP, or 1.0 if direction is DOWN.
     */
    void Reset() {
        val_ = dir_ == Dir::UP ? 0.0f : 1.0f;
    }

  private:
    float val_;  /**< Current normalized ramp value. */
    float step_; /**< Value change per sample. */
    Dir dir_;    /**< Ramping direction. */
};

}; // namespace dripline

#endif
