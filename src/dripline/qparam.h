#pragma once
#ifndef DL_QPARAMETER_H
#define DL_QPARAMETER_H

#include "daisy.h"

using daisy::AnalogControl;

namespace dripline {
/**
 * @brief Quantized parameter mapping tool.
 *
 * Maps a 0.0-1.0 input from an AnalogControl to discrete integer bins.
 * This is useful for mapping continuous controls like knobs to discrete
 * states or enum values.
 */
class QParameter {
  public:
    QParameter() : in_(nullptr), num_bins_(1), val_(-1) {
    }
    ~QParameter() = default;

    /**
     * @brief Initializes the parameter.
     * @param input A pointer to the AnalogControl hardware source.
     * @param num_bins The number of discrete bins to map to.
     */
    void Init(AnalogControl *input, int num_bins) {
        in_ = input;
        num_bins_ = num_bins > 0 ? num_bins : 1;
        val_ = -1; // Initialize to -1 to allow an initial snap on first Process()
    }

    /**
     * @brief Processes the input value and updates the bin value with hysteresis.
     * This should be called after the AnalogControl has been processed for the current block.
     * @return The current bin index (0 to num_bins - 1).
     */
    int Process() {
        if (!in_) {
            return 0;
        }
        const float raw = in_->Value();
        int potential_new_val = static_cast<int>(raw * (float)num_bins_);

        if (potential_new_val >= num_bins_) {
            potential_new_val = num_bins_ - 1;
        }
        if (potential_new_val < 0) {
            potential_new_val = 0;
        }

        if (val_ < 0) {
            val_ = potential_new_val;
        } else if (potential_new_val != val_) {
            const float bin_size = 1.0f / (float)num_bins_;
            const float margin = bin_size * 0.1f; // 10% bin width hysteresis margin

            if (potential_new_val > val_) {
                if (raw > (float)potential_new_val * bin_size + margin) {
                    val_ = potential_new_val;
                }
            } else {
                if (raw < (float)(potential_new_val + 1) * bin_size - margin) {
                    val_ = potential_new_val;
                }
            }
        }

        return val_;
    }

    /**
     * @return The current bin value without processing another sample.
     */
    inline int Value() const {
        return val_;
    }

  private:
    AnalogControl *in_;
    int num_bins_;
    int val_;
};
} // namespace dripline

#endif
