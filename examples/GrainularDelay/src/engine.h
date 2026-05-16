#pragma once
#ifndef GD_ENGINE_H
#define GD_ENGINE_H

#include "daisysp.h"
#include "frng.h"
#include "grain.h"
#include "math.h"
#include "mod.h"
#include "router.h"
#include "scheduler.h"

struct EngineParams {
    GrainManagerParams grain;
    ModParams mod;
    RouterParams router;
    SchedulerParams scheduler;
};

struct EngineSignals {
    float L;
    float R;
};

struct EngineOutput {
    float L;
    float R;
};

template <size_t max_delay, size_t grain_size>
class Engine {
  public:
    Engine() = default;
    ~Engine() = default;

    void Init(float sample_rate, DelayLine<float, max_delay> *L, DelayLine<float, max_delay> *R) {
        rng_.Init(123456);
        grain_manager_.Init(L, R, &rng_);
        mod_.Init(sample_rate);
        router_.Init(sample_rate, L, R);
        scheduler_.Init(sample_rate, &rng_);

        // Grain Manager Defaults
        smoothed_params_.grain.pitch = 1.0f;                  // unity pitch
        smoothed_params_.grain.duration = sample_rate * 0.1f; // 100ms
        smoothed_params_.grain.pan_spread = 0.5f;             // mid spread
        smoothed_params_.grain.time_spray = 0.0f;
        smoothed_params_.grain.pitch_spray = 0.0f;
        smoothed_params_.grain.delay_min = sample_rate * 0.25f; // 250ms base delay
        smoothed_params_.grain.quantize_mode = QuantizeMode::FREE;
        smoothed_params_.grain.window.size = 0;
        smoothed_params_.grain.window.table = nullptr;
        smoothed_params_.grain.random_direction = false;

        // Router Defaults
        smoothed_params_.router.mode = RouterMode::PARALLEL;
        smoothed_params_.router.feedback = 0.0f;
        smoothed_params_.router.tone = 0.5f; // Neutral filter position
        smoothed_params_.router.mix = 0.5f;  // 50/50 wet/dry
        smoothed_params_.router.freeze = false;
        smoothed_params_.router.drive = 1.0f;
        smoothed_params_.router.output = 1.0f; // Master makeup gain
        smoothed_params_.router.trails_bypass = false;

        // Scheduler Defaults
        smoothed_params_.scheduler.mode = SchedulerMode::FREE;
        smoothed_params_.scheduler.density = 20.0f; // 20Hz base density

        smoothed_params_.mod.depth = ModDepth::SOME;
    }

    void UpdateParameters(const EngineParams &raw_targets) {
        const float kAlpha = 0.05f;

        fonepole_deadband(smoothed_params_.grain.pitch, raw_targets.grain.pitch, kAlpha);
        fonepole_deadband(smoothed_params_.grain.duration, raw_targets.grain.duration, kAlpha);
        fonepole_deadband(smoothed_params_.grain.time_spray, raw_targets.grain.time_spray, kAlpha);
        fonepole_deadband(smoothed_params_.grain.pitch_spray, raw_targets.grain.pitch_spray, kAlpha);
        fonepole_deadband(smoothed_params_.grain.pan_spread, raw_targets.grain.pan_spread, kAlpha);

        smoothed_params_.grain.delay_min = raw_targets.grain.delay_min;
        smoothed_params_.grain.window = raw_targets.grain.window;
        smoothed_params_.grain.quantize_mode = raw_targets.grain.quantize_mode;
        smoothed_params_.grain.random_direction = raw_targets.grain.random_direction;

        fonepole_deadband(smoothed_params_.router.tone, raw_targets.router.tone, kAlpha);
        fonepole_deadband(smoothed_params_.router.feedback, raw_targets.router.feedback, kAlpha);
        fonepole_deadband(smoothed_params_.router.mix, raw_targets.router.mix, kAlpha);
        fonepole_deadband(smoothed_params_.router.drive, raw_targets.router.drive, kAlpha);
        fonepole_deadband(smoothed_params_.router.output, raw_targets.router.output, kAlpha);

        smoothed_params_.router.mode = raw_targets.router.mode;
        smoothed_params_.router.freeze = raw_targets.router.freeze;
        smoothed_params_.router.trails_bypass = raw_targets.router.trails_bypass;

        smoothed_params_.scheduler = raw_targets.scheduler;
        smoothed_params_.mod = raw_targets.mod;
    }

    EngineOutput Process(const EngineSignals in) {
        float mod_scaler = mod_.Process(smoothed_params_.mod);
        int fire_count = scheduler_.Process(smoothed_params_.scheduler, mod_scaler);
        GrainOutput go = grain_manager_.Process(smoothed_params_.grain, fire_count, mod_scaler);
        RouterOutput ro = router_.Process(smoothed_params_.router, {in.L, in.R, go.L, go.R});
        return {ro.L, ro.R};
    }

    const EngineParams &GetParams() const {
        return smoothed_params_;
    }

    int GetActiveGrainCount() const {
        return grain_manager_.GetActiveGrainCount();
    }

  private:
    EngineParams smoothed_params_;
    FastRandom rng_;
    GrainManager<max_delay, grain_size> grain_manager_;
    Mod mod_;
    Router<max_delay> router_;
    Scheduler scheduler_;
};

#endif
