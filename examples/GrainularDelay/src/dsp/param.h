#pragma once
#ifndef GD_PARAM_H
#define GD_PARAM_H

#include "math.h"
#include <cmath>

namespace dsp {

/**
 * @brief A class for handling smoothed, continuous parameters.
 *
 * This class takes a normalized (0.0-1.0) input and smooths it towards a
 * target value within a specified range, using either a linear or logarithmic
 * curve.
 */
class SmoothedParam {
  public:
    /** @brief Defines the mapping curve for the parameter. */
    enum class Curve {
        LINEAR,      /**< Linear mapping. */
        LOGARITHMIC, /**< Logarithmic mapping. */
    };

    SmoothedParam() = default;
    ~SmoothedParam() = default;

    /**
     * @brief Initializes the smoothed parameter.
     */
    void Init(float initial_value, float min, float max, float smoothing_coeff, Curve curve = Curve::LINEAR) {
        min_ = min;
        max_ = max;
        val_ = initial_value;
        target_ = initial_value;
        last_normalized_target_ = -2.0f;
        changed_ = false;
        coeff_ = smoothing_coeff;
        curve_ = curve;

        if (curve == Curve::LOGARITHMIC) {
            log_min_ = logf(min_ < 0.0000001f ? 0.0000001f : min_);
            log_max_ = logf(max_);
        }
    }

    /**
     * @brief Processes a new normalized target value.
     *
     * This should be called at the control rate to update the internal smoothed
     * value.
     */
    void Process(float normalized_target) {
        if (fabsf(normalized_target - last_normalized_target_) > 0.0001f) {
            changed_ = true;
            last_normalized_target_ = normalized_target;
        }

        switch (curve_) {
        case Curve::LINEAR:
            target_ = (normalized_target * (max_ - min_)) + min_;
            break;
        case Curve::LOGARITHMIC:
            target_ = expf((normalized_target * (log_max_ - log_min_)) + log_min_);
            break;
        }
        fonepole_deadband(val_, target_, coeff_);
    }

    /** @brief Gets the current smoothed value. */
    float Value() const {
        return val_;
    }

    /** @brief Checks if the parameter's target has changed since the last call. */
    bool HasChanged() {
        bool was_changed = changed_;
        changed_ = false;
        return was_changed;
    }

    operator float() const {
        return val_;
    }

  private:
    float min_;                    /**< The minimum value of the parameter range. */
    float max_;                    /**< The maximum value of the parameter range. */
    float log_min_;                /**< The cached log of the minimum value. */
    float log_max_;                /**< The cached log of the maximum value. */
    float val_;                    /**< The current smoothed value. */
    float target_;                 /**< The target value. */
    float last_normalized_target_; /**< The last received normalized target. */
    bool changed_;                 /**< Flag indicating if the target has changed. */
    float coeff_;                  /**< The smoothing coefficient. */
    Curve curve_;                  /**< The mapping curve. */
};

/**
 * @brief A class for handling discrete parameters (enums).
 *
 * This class manages a parameter that can only take on a fixed set of discrete
 * values, typically represented by an enum.
 *
 * @tparam T The enum type of the parameter.
 */
template <typename T>
class DiscreteParam {
  public:
    DiscreteParam() = default;
    ~DiscreteParam() = default;

    /** @brief Initializes the discrete parameter. */
    void Init(T initial_value, T num_values_enum) {
        val_ = initial_value;
        size_t num_vals = static_cast<size_t>(num_values_enum);
        num_values_ = num_vals > 0 ? num_vals : 1;
        changed_ = false;
    }

    /** @brief Sets the parameter value from an integer index. */
    void Set(int index) {
        if (index < 0) {
            index = 0;
        }
        if (static_cast<size_t>(index) >= num_values_) {
            index = num_values_ - 1;
        }
        T new_val = static_cast<T>(index);
        if (new_val != val_) {
            val_ = new_val;
            changed_ = true;
        }
    }

    /** @brief Sets the parameter value from a normalized float (0.0-1.0). */
    void Set(float value) {
        if (value < 0.0f) {
            value = 0.0f;
        }
        if (value > 1.0f) {
            value = 1.0f;
        }
        int index = static_cast<int>(value * num_values_);
        if (static_cast<size_t>(index) >= num_values_) {
            index = num_values_ - 1;
        }
        T new_val = static_cast<T>(index);
        if (new_val != val_) {
            val_ = new_val;
            changed_ = true;
        }
    }

    /** @brief Sets the parameter value directly from a value of type T. */
    void Set(T value) {
        if (value != val_) {
            val_ = value;
            changed_ = true;
        }
    }

    /** @brief Gets the current value. */
    T Value() const {
        return val_;
    }

    /** @brief Checks if the parameter's value has changed since the last call. */
    bool HasChanged() {
        bool was_changed = changed_;
        changed_ = false;
        return was_changed;
    }

    operator T() const {
        return val_;
    }

  private:
    T val_;                 /**< The current value. */
    size_t num_values_ = 1; /**< The total number of possible values. */
    bool changed_ = false;  /**< Flag indicating if the value has changed. */
};

} // namespace dsp

#endif
