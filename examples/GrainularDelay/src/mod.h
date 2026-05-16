#pragma once
#ifndef GD_MOD_H
#define GD_MOD_H

#include "daisysp.h"

using daisysp::Oscillator;

enum class ModDepth {
    NONE,
    SOME,
    MORE
};

struct ModParams {
    ModDepth depth;
};

class Mod {
  public:
    Mod() = default;
    ~Mod() = default;

    void Init(float sample_rate) {
        lfo_.Init(sample_rate);
        lfo_.SetWaveform(daisysp::Oscillator::WAVE_TRI);
        lfo_.SetFreq(0.1f);
        lfo_.SetAmp(1.0f);
    }

    float Process(const ModParams &params) {
        float lfo_freq = 0.1f;
        if (params.depth == ModDepth::MORE) {
            lfo_freq = 0.35f;
        }
        lfo_.SetFreq(lfo_freq);

        float lfo_val = (lfo_.Process() + 1.0f) * 0.5f;

        float mod_scaler = 0.0f;
        if (params.depth == ModDepth::SOME) {
            mod_scaler = lfo_val * 0.5f;
        } else if (params.depth == ModDepth::MORE) {
            mod_scaler = lfo_val;
        }

        return mod_scaler;
    }

  private:
    Oscillator lfo_;
};

#endif
