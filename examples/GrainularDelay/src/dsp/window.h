#pragma once
#ifndef DSP_WINDOW_H
#define DSP_WINDOW_H

#include "daisysp.h"
#include <cmath>

namespace dsp {

/** @brief Defines the shape of the grain amplitude window. */
enum class WindowShape {
    SMOOTH, /**< A smooth, symmetrical Hann window. */
    PLUCK,  /**< A percussive window with a fast attack and slow decay. */
    SWELL,  /**< A reversed window with a slow attack and fast decay. */
    COUNT,  /**< The number of available window shapes. */
};

/**
 * @brief A struct representing a single window function lookup table.
 */
struct Window {
    const size_t size;  /**< The size of the lookup table. */
    const float *table; /**< A pointer to the lookup table data. */

    /**
     * @brief Reads a value from the window table using linear interpolation.
     *
     * @param pos The phase position to read from, normalized from 0.0 to 1.0.
     * @return The interpolated value from the window table.
     */
    float Read(float pos) const {
        float idx_f = pos * (float)size;
        size_t idx_i = (size_t)idx_f;
        // Safety check to ensure we don't read past the end of the table.
        // This can happen when pos is exactly 1.0.
        if (idx_i >= size) {
            return table[size];
        }
        float frac = idx_f - (float)idx_i;
        float window = table[idx_i] + frac * (table[idx_i + 1] - table[idx_i]);
        return window;
    }
};

/**
 * @brief A class that holds pre-calculated lookup tables for various window
 * shapes.
 */
class WindowBank {
  public:
    /** @brief The size of the internal lookup tables. */
    static constexpr size_t kTableSize = 4096;

    WindowBank()
        : smooth_({kTableSize, smooth_data_}),
          pluck_({kTableSize, pluck_data_}),
          swell_({kTableSize, swell_data_}) {
    }
    ~WindowBank() = default;

    /**
     * @brief Initializes the window table by calculating the values for each
     * shape.
     */
    void Init() {
        for (size_t i = 0; i <= kTableSize; i++) {
            float phase = (float)i / (float)kTableSize;

            // Smooth (Hann)
            smooth_data_[i] = 0.5f * (1.0f - cosf(TWOPI_F * phase));

            // Pluck (Percussive: fast attack, slow exponential decay)
            // 5% attack, 95% decay
            if (phase < 0.05f) {
                pluck_data_[i] = phase / 0.05f;
            } else {
                pluck_data_[i] = expf(-5.0f * (phase - 0.05f) / 0.95f);
            }

            // Swell (Reverse Percussive: slow exponential attack, fast decay)
            // 95% attack, 5% decay
            if (phase < 0.95f) {
                swell_data_[i] = expf(-5.0f * (0.95f - phase) / 0.95f);
            } else {
                swell_data_[i] = (1.0f - phase) / 0.05f;
            }
        }
    }

    /**
     * @brief Gets a pointer to a specific window.
     *
     * @param shape The desired window shape.
     * @return A const pointer to the Window struct.
     */
    const Window *Get(WindowShape shape) const {
        switch (shape) {
        case WindowShape::SMOOTH:
            return &smooth_;
        case WindowShape::PLUCK:
            return &pluck_;
        case WindowShape::SWELL:
            return &swell_;
        default:
            return &smooth_;
        }
    }

  private:
    // Data must be defined before Window objects that use it.
    float smooth_data_[kTableSize + 1]; /**< Data for the SMOOTH window. */
    float pluck_data_[kTableSize + 1];  /**< Data for the PLUCK window. */
    float swell_data_[kTableSize + 1];  /**< Data for the SWELL window. */
    Window smooth_;                     /**< The SMOOTH window instance. */
    Window pluck_;                      /**< The PLUCK window instance. */
    Window swell_;                      /**< The SWELL window instance. */
};

} // namespace dsp

#endif
