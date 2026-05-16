#pragma once
#ifndef GD_WINDOW_H
#define GD_WINDOW_H

#include "daisysp.h"
#include <cmath>

class WindowTable {
  public:
    static constexpr size_t kTableSize = 4096;

    WindowTable() = default;
    ~WindowTable() = default;

    void Init() {
        for (size_t i = 0; i <= kTableSize; i++) {
            float phase = (float)i / (float)kTableSize;

            // Smooth (Hann)
            smooth_[i] = 0.5f * (1.0f - cosf(TWOPI_F * phase));

            // Pluck (Percussive: fast attack, slow exponential decay)
            // 5% attack, 95% decay
            if (phase < 0.05f) {
                pluck_[i] = phase / 0.05f;
            } else {
                pluck_[i] = expf(-5.0f * (phase - 0.05f) / 0.95f);
            }

            // Swell (Reverse Percussive: slow exponential attack, fast decay)
            // 95% attack, 5% decay
            if (phase < 0.95f) {
                swell_[i] = expf(-5.0f * (0.95f - phase) / 0.95f);
            } else {
                swell_[i] = (1.0f - phase) / 0.05f;
            }
        }
    }

    float *GetSmooth() {
        return smooth_;
    }
    float *GetPluck() {
        return pluck_;
    }
    float *GetSwell() {
        return swell_;
    }

  private:
    float smooth_[kTableSize + 1];
    float pluck_[kTableSize + 1];
    float swell_[kTableSize + 1];
};

#endif
