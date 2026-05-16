#pragma once
#ifndef GD_ENGINE_H
#define GD_ENGINE_H

#include "daisysp.h"
#include "dripline/frng.h"
#include "grain.h"
#include "math.h"
#include "modulator.h"
#include "router.h"
#include "scheduler.h"
#include "window.h"

namespace dsp {

/**
 * @brief A struct holding all raw, unprocessed parameters for the audio engine.
 *
 * This struct is passed to the Engine to update its internal state. The
 * parameters are then processed and smoothed by the individual DSP modules.
 */
struct RawEngineParams {
    RawGrainManagerParams grain_manager; /**< Raw parameters for the GrainManager. */
    RawModulatorParams modulator;        /**< Raw parameters for the Modulator. */
    RawRouterParams router;              /**< Raw parameters for the Router. */
    RawSchedulerParams scheduler;        /**< Raw parameters for the Scheduler. */
};

/**
 * @brief A struct representing a single stereo audio input sample for the engine.
 */
struct EngineInput {
    float L; /**< Left channel input sample. */
    float R; /**< Right channel input sample. */
};

/**
 * @brief A struct representing a single stereo audio output sample from the engine.
 */
struct EngineOutput {
    float L;      /**< Left channel output sample. */
    float R;      /**< Right channel output sample. */
    bool clipped; /**< Flag indicating if clipping occurred during processing. */
};

/**
 * @brief Configuration settings for initializing the Engine.
 *
 * @tparam max_delay The maximum size of the delay lines in samples.
 */
template <size_t max_delay>
struct EngineConfig {
    float sample_rate;              /**< The audio sample rate in Hz. */
    float control_rate;             /**< The rate at which control parameters are updated, in Hz. */
    DelayLine<float, max_delay> *L; /**< Pointer to the left channel delay line buffer. */
    DelayLine<float, max_delay> *R; /**< Pointer to the right channel delay line buffer. */
    WindowBank *window_bank;        /**< Pointer to the bank of windowing functions. */
};

/**
 * @brief The main audio processing engine for the granular delay effect.
 *
 * This class orchestrates the various DSP modules (Scheduler, GrainManager,
 * Modulator, Router) to create the final granular effect. It takes raw
 * parameters, updates the internal modules, and processes audio samples.
 *
 * @tparam max_delay The maximum size of the delay lines in samples.
 * @tparam grain_size The number of grains in the polyphonic grain pool.
 */
template <size_t max_delay, size_t grain_size>
class Engine {
  public:
    Engine() = default;
    ~Engine() = default;

    /**
     * @brief Initializes the engine and all its sub-modules.
     *
     * @param cfg The configuration settings for the engine.
     */
    void Init(EngineConfig<max_delay> &cfg) {
        float smoothing_time_ms = 50.0f;
        float smoothing_coeff = 1.0f - expf(-1000.0f / (smoothing_time_ms * cfg.control_rate));

        rng_.Init(123456);

        GrainManagerConfig<max_delay> grain_mgr_cfg;
        grain_mgr_cfg.buffer_L = cfg.L;
        grain_mgr_cfg.buffer_R = cfg.R;
        grain_mgr_cfg.rng = &rng_;
        grain_mgr_cfg.window_bank = cfg.window_bank;
        grain_mgr_cfg.delay_min = cfg.sample_rate * kDefaultDelayMinSec;
        grain_mgr_cfg.sample_rate = cfg.sample_rate;
        grain_mgr_cfg.smoothing_coeff = smoothing_coeff;
        grain_manager_.Init(grain_mgr_cfg);

        ModulatorConfig mod_cfg;
        mod_cfg.sample_rate = cfg.sample_rate;
        mod_cfg.smoothing_coeff = smoothing_coeff;
        modulator_.Init(mod_cfg);

        RouterConfig<max_delay> router_cfg;
        router_cfg.sample_rate = cfg.sample_rate;
        router_cfg.buffer_L = cfg.L;
        router_cfg.buffer_R = cfg.R;
        router_cfg.smoothing_coeff = smoothing_coeff;
        router_.Init(router_cfg);

        SchedulerConfig sched_cfg;
        sched_cfg.sample_rate = cfg.sample_rate;
        sched_cfg.rng = &rng_;
        sched_cfg.smoothing_coeff = smoothing_coeff;
        scheduler_.Init(sched_cfg);
    }

    /**
     * @brief Updates the engine's parameters from a raw parameter struct.
     *
     * This function is typically called at the control rate to update the state
     * of all internal DSP modules based on user input.
     *
     * @param raw The struct containing the latest raw parameter values.
     */
    void UpdateParams(const RawEngineParams &raw) {
        grain_manager_.UpdateParams(raw.grain_manager);
        modulator_.UpdateParams(raw.modulator);
        router_.UpdateParams(raw.router);
        scheduler_.UpdateParams(raw.scheduler);
    }

    /**
     * @brief Processes a single stereo audio sample.
     *
     * This is the main audio processing function. It takes a stereo input
     * sample, runs it through the granular processing chain, and returns a stereo
     * output sample.
     *
     * @param in The stereo input sample.
     * @return The processed stereo output sample.
     */
    EngineOutput Process(const EngineInput in) {
        float mod_scaler = modulator_.Process();
        int fire_count = scheduler_.Process(mod_scaler);
        GrainOutput go = grain_manager_.Process(fire_count, mod_scaler);
        RouterOutput ro = router_.Process({in.L, in.R, go.L, go.R});
        return {ro.L, ro.R, ro.clipped};
    }

    /**
     * @brief Resets the state of all internal DSP modules.
     *
     * This function is used for clearing all active grains and resetting delay
     * lines and filters, often used as a "panic" function.
     */
    void Reset() {
        grain_manager_.Reset();
        router_.Reset();
        scheduler_.Reset();
    }

    /**
     * @brief Gets the current number of active (playing) grains.
     *
     * @return The number of active grains.
     */
    int GetActiveGrainCount() const {
        return grain_manager_.GetActiveGrainCount();
    }

    /**
     * @brief Gets the current tempo subdivision used by the scheduler.
     *
     * @return The current scheduler subdivision.
     */
    SchedulerSubdivision GetSubdivision() const {
        return scheduler_.GetSubdivision();
    }

    /**
     * @brief Gets the current trigger frequency of the scheduler in Hz.
     *
     * @return The trigger frequency in Hz.
     */
    float GetFrequencyHz() const {
        return scheduler_.GetFrequencyHz();
    }

  private:
    /** @brief The minimum delay time for a grain in seconds. */
    static constexpr float kDefaultDelayMinSec = 0.25f;

    FastRandom rng_;                                    /**< Fast random number generator. */
    GrainManager<max_delay, grain_size> grain_manager_; /**< Manages the pool of grains. */
    Modulator modulator_;                               /**< LFO for modulation effects. */
    Router<max_delay> router_;                          /**< Handles audio routing, feedback, and mixing. */
    Scheduler scheduler_;                               /**< Determines when to trigger new grains. */
};

} // namespace dsp
#endif
