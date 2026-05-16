#pragma once
#ifndef GD_SCHEDULER_H
#define GD_SCHEDULER_H

#include "frng.h"

using dripline::FastRandom;

enum class SchedulerMode {
    FREE,
    SYNCED,
    BURST,
};

struct SchedulerParams {
    SchedulerMode mode;
    float density;
};

class Scheduler {
  public:
    Scheduler() = default;
    ~Scheduler() = default;

    void Init(float sample_rate, FastRandom *rng) {
        inverted_sample_rate_ = 1.0f / sample_rate;
        rng_ = rng;
        accumulator_ = 0.0f;
        threshold_ = 1.0f;
    }

    void Reset() {
        accumulator_ = 0.0f;
        threshold_ = 1.0f;
    }

    int Process(const SchedulerParams &params, float mod_scaler = 1.0f) {
        int triggers_to_fire = 0;

        accumulator_ += params.density * inverted_sample_rate_;

        while (accumulator_ >= threshold_) {
            accumulator_ -= threshold_;

            switch (params.mode) {
            case SchedulerMode::BURST:
                // Burst mode triggers once per accumulation cycle
                threshold_ = 1.0f;
                // Scale cluster size based on modulation depth
                // None: 1, Some: 3, More: 6 grains
                triggers_to_fire += (int)(1.0f + 5.0f * mod_scaler);
                break;
            case SchedulerMode::SYNCED:
                // Synced mode triggers once per accumulation cycle
                threshold_ = 1.0f;
                // Scale doubling: None: 1, Some: 1, More: 2 grains
                triggers_to_fire += (int)(1.0f + 1.0f * mod_scaler);
                break;
            case SchedulerMode::FREE:
                // Free mode has random threshold
                float jitter_amount = 0.15f * mod_scaler;
                threshold_ = (1.0f - jitter_amount) + (rng_->NextFloat() * 2.0f * jitter_amount);
                triggers_to_fire += 1;
                break;
            }

            if (params.mode != SchedulerMode::FREE) {
                break;
            }
        }

        return triggers_to_fire;
    }

  private:
    FastRandom *rng_;
    float inverted_sample_rate_;
    float accumulator_;
    float threshold_ = 1.0f;
};

#endif
