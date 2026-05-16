#pragma once
#ifndef GD_GRAIN_H
#define GD_GRAIN_H

#include "daisysp.h"
#include "frng.h"
#include "math.h"
#include "quantizer.h"

using daisysp::DelayLine;
using dripline::FastRandom;

struct GrainWindow {
    size_t size;
    float *table;

    float Read(float pos) {
        float idx_f = pos * (float)size;
        size_t idx_i = (size_t)idx_f;
        if (idx_i >= size) {
            return table[size];
        }
        float frac = idx_f - (float)idx_i;
        float window = table[idx_i] + frac * (table[idx_i + 1] - table[idx_i]);
        return window;
    }
};

struct GrainParams {
    float duration;
    float offset;
    float pan;
    float pitch;
    GrainWindow window;
};

struct GrainOutput {
    float L;
    float R;
};

template <size_t max_delay>
class Grain {
  public:
    Grain() = default;
    ~Grain() = default;

    void Reset() {
        active_ = false;
        phase_ = 0.0f;
    }

    void Trigger(const GrainParams &params) {
        params_ = params;
        // Safety: Prevent division by zero
        if (params_.duration < 1.0f) {
            params_.duration = 1.0f;
        }
        phase_ = 0.0f;
        phase_inc_ = 1.0f / params_.duration;
        active_ = true;

        float angle = params_.pan * HALFPI_F;
        pan_L_ = arm_cos_f32(angle);
        pan_R_ = arm_sin_f32(angle);
    }

    GrainOutput Process(DelayLine<float, max_delay> *buf_L, DelayLine<float, max_delay> *buf_R) {
        if (!active_) {
            return {0.0f, 0.0f};
        }

        float read_pos = params_.offset + (phase_ * params_.duration * params_.pitch);

        float L = buf_L->ReadHermite(read_pos);
        float R = buf_R->ReadHermite(read_pos);

        float window = params_.window.Read(phase_);

        phase_ += phase_inc_;
        if (phase_ >= 1.0f) {
            active_ = false;
        }

        return {L * window * pan_L_, R * window * pan_R_};
    }

    bool IsActive() const {
        return active_;
    }

  private:
    GrainParams params_;
    bool active_ = false;
    float pan_L_;
    float pan_R_;
    float phase_;
    float phase_inc_;
};

struct GrainManagerParams {
    float delay_min;
    float duration;
    float pan_spread;
    float time_spray;
    float pitch;
    float pitch_spray;
    GrainWindow window;
    QuantizeMode quantize_mode;
    bool random_direction;
};

constexpr float kSafetyPadding = 100.0f;

template <size_t max_delay, size_t grain_size>
class GrainManager {
  public:
    GrainManager() = default;
    ~GrainManager() = default;

    void Init(DelayLine<float, max_delay> *L, DelayLine<float, max_delay> *R, FastRandom *rng) {
        buffer_L_ = L;
        buffer_R_ = R;
        rng_ = rng;
    }

    void Reset() {
        for (size_t i = 0; i < grain_size; i++) {
            grains_[i].Reset();
        }
    }

    GrainOutput Process(const GrainManagerParams &params, size_t count, float mod_scaler = 1.0f) {
        for (size_t i = 0; i < count; i++) {
            TriggerNextGrain(params, mod_scaler);
        }
        return ProcessGrains();
    }

    int GetActiveGrainCount() const {
        int active = 0;
        for (size_t i = 0; i < grain_size; i++) {
            if (grains_[i].IsActive()) {
                active++;
            }
        }
        return active;
    }

  private:
    DelayLine<float, max_delay> *buffer_L_;
    DelayLine<float, max_delay> *buffer_R_;
    Grain<max_delay> grains_[grain_size];
    FastRandom *rng_;
    Quantizer q_;

    void TriggerNextGrain(const GrainManagerParams &params, float mod_scaler) {
        for (size_t i = 0; i < grain_size; i++) {
            if (!grains_[i].IsActive()) {
                float pan_spread = params.pan_spread * mod_scaler;
                float pitch_spray = params.pitch_spray * mod_scaler;
                float time_spray = params.time_spray * mod_scaler;

                GrainParams grain_params;
                grain_params.duration = params.duration;
                grain_params.pan = 0.5f + ((rng_->NextFloat() * 2.0f) - 1.0f) * 0.5f * pan_spread;
                grain_params.window = params.window;

                float spray = (rng_->NextFloat() * 2.0f - 1.0f) * pitch_spray;
                float pitch = params.pitch + spray;
                if (params.random_direction && rng_->NextFloat() > 0.5f) {
                    pitch = -pitch;
                }
                grain_params.pitch = q_.Process(params.quantize_mode, pitch);

                float pitch_factor = fmaxf(1.0f, fabsf(grain_params.pitch));
                float min_safe = (grain_params.duration * pitch_factor) + time_spray + kSafetyPadding;
                float max_safe = (float)max_delay - grain_params.duration - time_spray - kSafetyPadding;
                float clamped_offset = fmaxf(min_safe, fminf(params.delay_min, max_safe));
                grain_params.offset = clamped_offset + (rng_->NextFloat() * time_spray);

                grains_[i].Trigger(grain_params);
                return;
            }
        }
    }

    GrainOutput ProcessGrains() {
        float L = 0.0f;
        float R = 0.0f;

        for (size_t g = 0; g < grain_size; g++) {
            GrainOutput go = grains_[g].Process(buffer_L_, buffer_R_);
            L += go.L;
            R += go.R;
        }

        return {L * 0.25f, R * 0.25f};
    }
};

#endif
