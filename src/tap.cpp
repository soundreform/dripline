#include "tap.h"

using dripline::TapTempo;

void TapTempo::Init(float sample_rate, float default_bpm)
{
    sample_rate_ = sample_rate;
    current_bpm_ = default_bpm;

    sample_counter_.store(0, std::memory_order_relaxed);
    last_tap_time_samples_ = 0;
    tap_count_ = 0;
    interval_sum_ = 0;
    interval_index_ = 0;

    // If 2 seconds pass without a tap, we assume the user is starting a new tempo
    timeout_samples_ = (uint32_t)(sample_rate_ * 2.0f);

    for (int i = 0; i < kNumTapsToAverage; i++)
    {
        intervals_[i] = 0;
    }
}

void TapTempo::Process(size_t num_samples)
{
    sample_counter_.fetch_add(num_samples, std::memory_order_relaxed);
}

void TapTempo::Tap()
{
    uint32_t current_time = sample_counter_.load(std::memory_order_relaxed);

    // Reset averaging if the gap between taps is too long (timeout)
    if (current_time - last_tap_time_samples_ > timeout_samples_)
    {
        tap_count_ = 0;
        interval_sum_ = 0;
    }

    if (tap_count_ > 0)
    {
        uint32_t interval = current_time - last_tap_time_samples_;

        // Moving average: Subtract the oldest interval before adding the new one
        if (tap_count_ >= kNumTapsToAverage)
        {
            interval_sum_ -= intervals_[interval_index_];
        }

        intervals_[interval_index_] = interval;
        interval_sum_ += interval;
        interval_index_ = (interval_index_ + 1) % kNumTapsToAverage;

        int num_to_average = (tap_count_ < kNumTapsToAverage) ? tap_count_ : kNumTapsToAverage;
        float average_interval = (float)interval_sum_ / num_to_average;

        if (average_interval > 0.0f)
        {
            current_bpm_ = (sample_rate_ * 60.0f) / average_interval;

            if (current_bpm_ > kMaxBpm)
                current_bpm_ = kMaxBpm;
            if (current_bpm_ < kMinBpm)
                current_bpm_ = kMinBpm;
        }
    }

    last_tap_time_samples_ = current_time;
    tap_count_++;
}
