#pragma once
#ifndef DSP_TONE_FILTER_H
#define DSP_TONE_FILTER_H

#include "daisysp.h"

namespace dsp {

/**
 * @brief A bipolar tone filter combining a low-pass and a high-pass filter.
 *
 * This class implements a simple tone control where the 0.0-0.5 range of the
 * control position sweeps a low-pass filter, and the 0.5-1.0 range sweeps a
 * high-pass filter. The center position (0.5) is neutral. It provides
 * independent processing for stereo channels.
 */
class ToneFilter {
  public:
    ToneFilter() = default;
    ~ToneFilter() = default;

    /**
     * @brief Initializes the tone filter.
     * @param sample_rate The audio sample rate in Hz.
     * @param lp_min The minimum cutoff frequency for the low-pass filter.
     * @param lp_max The maximum cutoff frequency for the low-pass filter.
     * @param hp_min The minimum cutoff frequency for the high-pass filter.
     * @param hp_max The maximum cutoff frequency for the high-pass filter.
     */
    void Init(float sample_rate, float lp_min = 200.0f, float lp_max = 18000.0f, float hp_min = 10.0f, float hp_max = 3000.0f) {
        sample_rate_ = sample_rate;
        lp_min_ = lp_min;
        lp_max_ = lp_max;
        hp_min_ = hp_min;
        hp_max_ = hp_max;
        current_pos_ = -1.0f; // Force coefficient calculation on first SetPos
        Reset();
    }

    /** @brief Resets the internal state of the filters. */
    void Reset() {
        lp_state_l_ = 0.0f;
        hp_state_l_ = 0.0f;
        lp_state_r_ = 0.0f;
        hp_state_r_ = 0.0f;
    }

    /**
     * @brief Sets the position of the tone control.
     *
     * @param pos The control position, from 0.0 (max LPF) to 1.0 (max HPF).
     */
    void SetPos(float pos) {
        // A direct float comparison is used here. This is intentional.
        // The coefficients will only be recalculated if the new `pos` value
        // is exactly different from the current one. For smoothed parameters,
        // this might happen on every sample, which is the desired behavior.
        if (pos != current_pos_) {
            current_pos_ = pos;
            RecalculateCoeffs(pos);
        }
    }

    /**
     * @brief Processes one sample through the left channel filter.
     *
     * @param in The input sample.
     * @return The filtered output sample.
     */
    float ProcessLeft(float in) {
        hp_state_l_ += coeffs_.hp * (in - hp_state_l_);
        float hp_out_l = in - hp_state_l_;
        lp_state_l_ += coeffs_.lp * (hp_out_l - lp_state_l_);
        return lp_state_l_;
    }

    /**
     * @brief Processes one sample through the right channel filter.
     *
     * @param in The input sample.
     * @return The filtered output sample.
     */
    float ProcessRight(float in) {
        hp_state_r_ += coeffs_.hp * (in - hp_state_r_);
        float hp_out_r = in - hp_state_r_;
        lp_state_r_ += coeffs_.lp * (hp_out_r - lp_state_r_);
        return lp_state_r_;
    }

  private:
    struct Coeffs {
        float lp; /**< Low-pass filter coefficient. */
        float hp; /**< High-pass filter coefficient. */
    };

    /**
     * @brief Recalculates the filter coefficients based on the control position.
     * @param pos The control position (0.0-1.0).
     */
    void RecalculateCoeffs(float pos) {
        float lp_param = (pos <= 0.5f) ? (pos * 2.0f) : 1.0f;
        float hp_param = (pos >= 0.5f) ? ((pos - 0.5f) * 2.0f) : 0.0f;

        float lp, hp;

        if (lp_param >= 1.0f) {
            lp = 1.0f;
        } else {
            float lp_freq = lp_min_ + (lp_param * lp_param * (lp_max_ - lp_min_));
            lp = 1.0f - expf(-TWOPI_F * lp_freq / sample_rate_);
        }

        if (hp_param <= 0.0f) {
            hp = 0.0f;
        } else {
            float hp_freq = hp_min_ + (hp_param * hp_param * (hp_max_ - hp_min_));
            hp = 1.0f - expf(-TWOPI_F * hp_freq / sample_rate_);
        }
        coeffs_ = {lp, hp};
    }

    float sample_rate_;                           /**< The audio sample rate in Hz. */
    float lp_min_, lp_max_, hp_min_, hp_max_;     /**< Filter frequency range settings. */
    float lp_state_l_ = 0.0f, hp_state_l_ = 0.0f; /**< Left channel filter states. */
    float lp_state_r_ = 0.0f, hp_state_r_ = 0.0f; /**< Right channel filter states. */
    float current_pos_ = -1.0f;                   /**< The current control position. */
    Coeffs coeffs_;                               /**< The current filter coefficients. */
};

} // namespace dsp

#endif
