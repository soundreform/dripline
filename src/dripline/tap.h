#pragma once
#ifndef DL_TAP_H
#define DL_TAP_H

#include <atomic>
#include <cstddef>
#include <stdint.h>

namespace dripline {
/**
 * @brief A utility class for calculating tempo from manual "tap" inputs.
 *
 * TapTempo tracks the time between calls to the Tap() method to determine
 * a BPM. It uses a moving average to smooth out timing variations and
 * includes a timeout mechanism to reset the averaging if tapping stops.
 */
class TapTempo {
  public:
    TapTempo() = default;
    ~TapTempo() = default;

    /**
     * @brief Initializes the TapTempo tracker.
     * @param sample_rate The audio sample rate in Hz.
     * @param default_bpm The initial BPM value.
     */
    void Init(float sample_rate, float default_bpm = 120.0f) {
        sample_rate_ = sample_rate;
        current_bpm_ = default_bpm;

        Reset();

        timeout_samples_ = (uint32_t)(sample_rate_ * 2.5f);
        debounce_samples_ = (uint32_t)(sample_rate_ * 0.020f);

        for (int i = 0; i < kNumTapsToAverage; i++) {
            intervals_[i] = 0;
        }
    }

    /**
     * @brief Advances the internal timer.
     *
     * This should be called in the audio processing loop with the number of samples
     * processed in the current block.
     *
     * @param num_samples Number of samples to add to the counter.
     */
    void Process(size_t num_samples) {
        sample_counter_.fetch_add(num_samples, std::memory_order_relaxed);
    }

    /**
     * @brief Records a tap event and updates the calculated tempo.
     *
     * If the time since the last tap exceeds the timeout (2 seconds), the
     * averaging history is cleared.
     */
    void Tap() {
        uint32_t current_time = sample_counter_.load(std::memory_order_relaxed);

        // Reset averaging if the gap between taps is too long (timeout)
        if (current_time - last_tap_time_samples_ > timeout_samples_) {
            tap_count_ = 0;
            interval_sum_ = 0;
        }

        if (tap_count_ > 0) {
            uint32_t interval = current_time - last_tap_time_samples_;

            // Hardware debounce: ignore triggers within 20ms of the last valid tap
            if (interval < debounce_samples_) {
                return;
            }

            // Moving average: Subtract the oldest interval before adding the new one
            if (tap_count_ >= kNumTapsToAverage) {
                interval_sum_ -= intervals_[interval_index_];
            }

            intervals_[interval_index_] = interval;
            interval_sum_ += interval;
            interval_index_ = (interval_index_ + 1) % kNumTapsToAverage;

            int num_to_average = (tap_count_ < kNumTapsToAverage) ? tap_count_ : kNumTapsToAverage;
            float average_interval = (float)interval_sum_ / num_to_average;

            if (average_interval > 0.0f) {
                current_bpm_ = (sample_rate_ * 60.0f) / average_interval;

                if (current_bpm_ > kMaxBpm) {
                    current_bpm_ = kMaxBpm;
                }
                if (current_bpm_ < kMinBpm) {
                    current_bpm_ = kMinBpm;
                }
            }
        }

        last_tap_time_samples_ = current_time;
        tap_count_++;
    }

    /** @brief Resets the tap history and timer. */
    void Reset() {
        sample_counter_.store(0, std::memory_order_relaxed);
        last_tap_time_samples_ = 0;
        tap_count_ = 0;
        interval_sum_ = 0;
        interval_index_ = 0;
    }

    /**
     * @brief Returns true if there is a valid tapped tempo currently active.
     * @return true if at least two taps have occurred within the timeout period.
     */
    bool IsActive() const {
        return tap_count_ > 1;
    }

    /** @return The current calculated tempo in Beats Per Minute. */
    float GetBPM() const {
        return current_bpm_;
    }

    /** @return The current beat interval in samples. */
    float GetIntervalSamples() const {
        return (sample_rate_ * 60.0f) / current_bpm_;
    }

    /** @return The current beat interval in milliseconds. */
    float GetIntervalMs() const {
        return 60000.0f / current_bpm_;
    }

    /** @return The current tempo expressed as a frequency in Hz. */
    float GetFrequencyHz() const {
        return current_bpm_ / 60.0f;
    }

  private:
    /** @brief Number of taps to average for the tempo calculation. */
    static const int kNumTapsToAverage = 3;
    /** @brief Minimum allowable BPM. */
    static constexpr float kMinBpm = 30.0f;
    /** @brief Maximum allowable BPM. */
    static constexpr float kMaxBpm = 300.0f;

    float sample_rate_;
    float current_bpm_;

    std::atomic<uint32_t> sample_counter_;
    uint32_t last_tap_time_samples_;

    uint32_t intervals_[kNumTapsToAverage];
    uint32_t interval_sum_;
    int interval_index_;
    int tap_count_;

    uint32_t timeout_samples_;
    uint32_t debounce_samples_;
};

}; // namespace dripline
#endif
