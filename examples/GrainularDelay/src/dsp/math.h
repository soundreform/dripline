#pragma once
#ifndef GD_MATH_H
#define GD_MATH_H

namespace dsp {

/**
 * @brief A one-pole low-pass filter with a deadband.
 *
 * This function applies a one-pole smoothing filter, but only when the
 * difference between the current value and the target value exceeds a small
 * threshold. This prevents "zombie" processing on static parameters.
 *
 * @param current The current value (will be updated in place).
 * @param target The target value to move towards.
 * @param alpha The smoothing coefficient.
 */
inline void fonepole_deadband(float &current, float target, float alpha) {
    if (fabsf(target - current) > 0.001f) {
        current += alpha * (target - current);
    }
}

} // namespace dsp

#endif
