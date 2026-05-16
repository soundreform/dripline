#pragma once
#ifndef GD_TONE_H
#define GD_TONE_H

#include "daisysp.h"

class Tone {
  public:
    Tone() = default;
    ~Tone() = default;

    struct Output {
        float lp;
        float hp;
    };

    void Init(float sample_rate, float lp_min = 200.0f, float lp_max = 18000.0f, float hp_min = 10.0f, float hp_max = 3000.0f) {
        sample_rate_ = sample_rate;
        lp_min_ = lp_min;
        lp_max_ = lp_max;
        hp_min_ = hp_min;
        hp_max_ = hp_max;
        Reset();
    }

    void Reset() {
        lp_state_ = 0.0f;
        hp_state_ = 0.0f;
    }

    float Filter(float in, const Output &coeffs) {
        hp_state_ += coeffs.hp * (in - hp_state_);
        float hp_out = in - hp_state_;
        lp_state_ += coeffs.lp * (hp_out - lp_state_);
        return lp_state_;
    }

    Output Process(float pos) {
        float lp_param = (pos <= 0.5f) ? (pos * 2.0f) : 1.0f;
        float hp_param = (pos >= 0.5f) ? ((pos - 0.5f) * 2.0f) : 0.0f;
        float lp_freq = lp_min_ + (lp_param * lp_param * lp_max_);
        float lp = 1.0f - expf(-TWOPI_F * lp_freq / sample_rate_);
        float hp_freq = hp_min_ + (hp_param * hp_param * hp_max_);
        float hp = 1.0f - expf(-TWOPI_F * hp_freq / sample_rate_);
        return {lp, hp};
    }

  private:
    float sample_rate_;
    float lp_min_;
    float lp_max_;
    float hp_min_;
    float hp_max_;
    float lp_state_ = 0.0f;
    float hp_state_ = 0.0f;
};

#endif
