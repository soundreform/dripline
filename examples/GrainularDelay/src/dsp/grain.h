#pragma once
#ifndef GD_GRAIN_H
#define GD_GRAIN_H

#include "daisysp.h"
#include "dripline/frng.h"
#include "math.h"
#include "param.h"
#include "quantizer.h"
#include "window.h"
#include <cmath>

using daisysp::DelayLine;
using daisysp::fclamp;
using dripline::FastRandom;

namespace dsp {

/** @brief Defines the playback direction of a grain. */
enum class GrainDirection {
    FORWARD, /**< Play the grain forward. */
    RANDOM,  /**< Randomly choose forward or reverse playback. */
    REVERSE, /**< Play the grain in reverse. */
    COUNT,   /**< The number of available directions. */
};

/**
 * @brief A struct holding the final, calculated parameters for a single grain.
 *
 * This struct is created by the GrainManager and passed to a Grain instance
 * when it is triggered.
 */
struct GrainParams {
    float duration;       /**< The duration of the grain in samples. */
    float offset;         /**< The read position offset in the delay line, in samples. */
    float pan;            /**< The stereo pan position (0.0=L, 0.5=C, 1.0=R). */
    float pitch;          /**< The playback pitch ratio. */
    const Window *window; /**< A pointer to the windowing function to use. */
};

/** @brief A struct representing a single stereo audio output from a grain. */
struct GrainOutput {
    float L; /**< Left channel output sample. */
    float R; /**< Right channel output sample. */
};

/**
 * @brief A single grain voice that reads from a delay line.
 *
 * A Grain is a short audio segment read from a delay line buffer. It has its
 * own lifecycle, starting when Trigger() is called and ending when its phase
 * reaches the end of its windowed duration.
 *
 * @tparam max_delay The maximum size of the delay lines in samples.
 */
template <size_t max_delay>
class Grain {
  public:
    Grain() = default;
    ~Grain() = default;

    /** @brief Resets the grain to an inactive state. */
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
        pan_L_ = cosf(angle);
        pan_R_ = sinf(angle);
    }

    /**
     * @brief Processes one audio sample for the grain.
     *
     * @param buf_L A pointer to the left channel delay line.
     * @param buf_R A pointer to the right channel delay line.
     * @return The processed stereo output sample.
     */
    GrainOutput Process(DelayLine<float, max_delay> *buf_L, DelayLine<float, max_delay> *buf_R) {
        if (!active_) {
            return {0.0f, 0.0f};
        }

        float read_pos = params_.offset + (phase_ * params_.duration * params_.pitch);

        float L = buf_L->ReadHermite(read_pos);
        float R = buf_R->ReadHermite(read_pos);

        float window = params_.window->Read(phase_);

        phase_ += phase_inc_;
        if (phase_ >= 1.0f) {
            active_ = false;
        }

        return {L * window * pan_L_, R * window * pan_R_};
    }

    /**
     * @brief Checks if the grain is currently active.
     *
     * @return true if the grain is active, false otherwise.
     */
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

/**
 * @brief A struct holding raw, unprocessed parameters for the GrainManager.
 */
struct RawGrainManagerParams {
    GrainDirection direction;   /**< The grain playback direction. */
    float duration;             /**< The base duration of grains. */
    float pan_spread;           /**< The amount of random stereo spread. */
    float pitch;                /**< The base pitch of grains. */
    float pitch_spray;          /**< The amount of random pitch variation. */
    QuantizeMode quantize_mode; /**< The pitch quantization mode. */
    float time_spray;           /**< The amount of random read position jitter. */
    WindowShape window;         /**< The window shape for grains. */
};

/**
 * @brief A struct holding the smoothed and discrete parameters for the
 * GrainManager.
 */
struct GrainManagerParams {
    SmoothedParam duration;    /**< Smoothed grain duration parameter. */
    SmoothedParam pan_spread;  /**< Smoothed pan spread parameter. */
    SmoothedParam pitch;       /**< Smoothed pitch parameter. */
    SmoothedParam pitch_spray; /**< Smoothed pitch spray parameter. */
    SmoothedParam time_spray;  /**< Smoothed time spray parameter. */

    DiscreteParam<QuantizeMode> quantize_mode; /**< Discrete pitch quantization mode. */
    DiscreteParam<GrainDirection> direction;   /**< Discrete grain direction. */
    DiscreteParam<WindowShape> window_shape;   /**< Discrete grain window shape. */

    void Init(float sample_rate, float smoothing_coeff) {
        pitch.Init(1.0f, 0.25f, 4.0f, smoothing_coeff, SmoothedParam::Curve::LOGARITHMIC);
        pitch_spray.Init(0.0f, 0.0f, 1.0f, smoothing_coeff);
        duration.Init(sample_rate * 0.1f, sample_rate * 0.01f, sample_rate * 0.5f, smoothing_coeff, SmoothedParam::Curve::LOGARITHMIC);
        pan_spread.Init(0.5f, 0.0f, 1.0f, smoothing_coeff);
        time_spray.Init(0.0f, 0.0f, sample_rate * 0.5f, smoothing_coeff);
        quantize_mode.Init(QuantizeMode::FREE, QuantizeMode::COUNT);
        direction.Init(GrainDirection::FORWARD, GrainDirection::COUNT);
        window_shape.Init(WindowShape::SMOOTH, WindowShape::COUNT);
    }

    void Process(const RawGrainManagerParams &raw) {
        pitch.Process(raw.pitch);
        pitch_spray.Process(raw.pitch_spray);
        duration.Process(raw.duration);
        pan_spread.Process(raw.pan_spread);
        time_spray.Process(raw.time_spray);
        quantize_mode.Set(static_cast<int>(raw.quantize_mode));
        direction.Set(static_cast<int>(raw.direction));
        window_shape.Set(static_cast<int>(raw.window));
    }
};

/**
 * @brief Calculates a randomized stereo pan position.
 *
 * @param rand A random float between 0.0 and 1.0.
 * @param spread The amount of spread (0.0 for center, 1.0 for full stereo).
 * @return The calculated pan position.
 */
float CalculatePan(float rand, float spread) {
    return 0.5f + ((rand * 2.0f) - 1.0f) * 0.5f * spread;
}

/**
 * @brief Calculates a randomized read offset.
 *
 * @param rand A random float between 0.0 and 1.0.
 * @param min The minimum offset in samples.
 * @param spray The maximum random spray amount in samples.
 * @return The calculated offset in samples.
 */
float CalculateOffset(float rand, float min, float spray) {
    return min + (rand * spray);
}

/**
 * @brief Clamps a target offset to ensure grain playback stays within buffer
 * boundaries.
 *
 * @param target The desired offset in samples.
 * @param duration The duration of the grain in samples.
 * @param pitch The pitch of the grain.
 * @param padding A safety padding in samples from the buffer edges.
 * @param max The maximum size of the buffer in samples.
 * @return The clamped, safe offset in samples.
 */
float ClampOffset(float target, float duration, float pitch, float padding, float max) {
    const float read_length = duration * fmaxf(1.0f, fabsf(pitch));

    float safe_offset_min;
    float safe_offset_max;

    if (pitch >= 0.f) { // Forward playback
        safe_offset_min = padding;
        safe_offset_max = max - read_length - padding;
    } else { // Reverse playback
        safe_offset_min = read_length + padding;
        safe_offset_max = max - padding;
    }

    // Clamp the target offset to the safe range.
    // Also ensure the max is always greater than the min to handle edge cases.
    safe_offset_max = fmaxf(safe_offset_min, safe_offset_max);
    return fclamp(target, safe_offset_min, safe_offset_max);
}

/**
 * @brief Calculates a randomized pitch value.
 *
 * @param rand A random float between 0.0 and 1.0.
 * @param pitch The base pitch ratio.
 * @param spray The maximum random spray amount.
 * @return The calculated pitch ratio.
 */
float CalculatePitch(float rand, float pitch, float spray) {
    return pitch + ((rand * 2.0f - 1.0f) * spray);
}

/**
 * @brief Applies a playback direction to a pitch value.
 *
 * @param rand A random float between 0.0 and 1.0 (used for RANDOM direction).
 * @param pitch The input pitch ratio.
 * @param dir The desired playback direction.
 * @return The direction-adjusted pitch ratio.
 */
float RedirectPitch(float rand, float pitch, GrainDirection dir) {
    if (dir == GrainDirection::REVERSE) {
        pitch = -pitch;
    } else if (dir == GrainDirection::RANDOM && rand > 0.5f) {
        pitch = -pitch;
    }
    return pitch;
}

/**
 * @brief Configuration settings for initializing the GrainManager.
 *
 * @tparam max_delay The maximum size of the delay lines in samples.
 */
template <size_t max_delay>
struct GrainManagerConfig {
    DelayLine<float, max_delay> *buffer_L; /**< Pointer to the left channel delay line. */
    DelayLine<float, max_delay> *buffer_R; /**< Pointer to the right channel delay line. */
    FastRandom *rng;                       /**< Pointer to a random number generator. */
    const WindowBank *window_bank;         /**< Pointer to the window bank. */
    float delay_min;                       /**< The minimum grain delay time in samples. */
    float sample_rate;                     /**< The audio sample rate in Hz. */
    float smoothing_coeff;                 /**< The coefficient for parameter smoothing. */
};

/**
 * @brief Manages a pool of polyphonic grains.
 *
 * This class is responsible for triggering new grains, managing the polyphonic
 * grain pool, calculating randomized grain parameters, and summing the output
 * of all active grains.
 *
 * @tparam max_delay The maximum size of the delay lines in samples.
 * @tparam grain_size The number of grains in the polyphonic pool.
 */
template <size_t max_delay, size_t grain_size>
class GrainManager {
  public:
    GrainManager() = default;
    ~GrainManager() = default;

    void Init(const GrainManagerConfig<max_delay> &config) {
        buffer_L_ = config.buffer_L;
        buffer_R_ = config.buffer_R;
        delay_min_ = config.delay_min;
        rng_ = config.rng;
        window_bank_ = config.window_bank;
        params_.Init(config.sample_rate, config.smoothing_coeff);
    }

    /** @brief Resets all grains and delay line buffers. */
    void Reset() {
        buffer_L_->Reset();
        buffer_R_->Reset();

        for (size_t i = 0; i < grain_size; i++) {
            grains_[i].Reset();
        }
    }

    /**
     * @brief Updates the manager's parameters from a raw parameter struct.
     *
     * @param raw The struct containing the latest raw parameter values.
     */
    void UpdateParams(const RawGrainManagerParams &raw) {
        params_.Process(raw);
    }

    /**
     * @brief Processes audio, triggering new grains and summing active ones.
     *
     * @param count The number of new grains to trigger in this block.
     * @param mod_scaler A modulation scaler (0.0-1.0) from the LFO.
     * @return The summed stereo output of all active grains.
     */
    GrainOutput Process(size_t count, float mod_scaler = 1.0f) {
        for (size_t i = 0; i < count; i++) {
            TriggerNextGrain(mod_scaler);
        }
        return ProcessGrains();
    }

    /**
     * @brief Gets the current number of active (playing) grains.
     *
     * @return The number of active grains.
     */
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
    /** @brief A safety padding from the start/end of the delay buffer. */
    static constexpr float kSafetyPadding = 100.0f;

    /**
     * @brief Finds an inactive grain in the pool and triggers it.
     *
     * @param mod_scaler The modulation scaler to pass to the new grain.
     */
    void TriggerNextGrain(float mod_scaler) {
        for (size_t i = 0; i < grain_size; i++) {
            if (!grains_[i].IsActive()) {
                grains_[i].Trigger(BuildGrainParams(mod_scaler));
                return;
            }
        }
    }

    /**
     * @brief Builds a set of randomized parameters for a new grain.
     *
     * @param mod_scaler A modulation scaler (0.0-1.0) from the LFO.
     * @return A populated GrainParams struct.
     */
    GrainParams BuildGrainParams(float mod_scaler) {
        float pan_spread = params_.pan_spread.Value();
        float pitch_spray = params_.pitch_spray.Value();
        float time_spray = params_.time_spray.Value();
        float duration = params_.duration.Value();
        float base_pitch = params_.pitch.Value();
        GrainDirection direction = params_.direction.Value();
        QuantizeMode quantize_mode = params_.quantize_mode.Value();
        WindowShape window_shape = params_.window_shape.Value();

        float random_offset = rng_->NextFloat();
        float random_pan = rng_->NextFloat();
        float random_spray = rng_->NextFloat();
        float random_direction = rng_->NextFloat();

        float offset = CalculateOffset(random_offset, delay_min_, time_spray);

        float pitch = CalculatePitch(random_spray, base_pitch, pitch_spray);
        pitch = RedirectPitch(random_direction, pitch, direction);
        pitch = q_.Process(quantize_mode, pitch);

        GrainParams grain_params;
        grain_params.duration = duration;
        grain_params.offset = ClampOffset(offset, duration, pitch, kSafetyPadding, max_delay);
        grain_params.pan = CalculatePan(random_pan, pan_spread);
        grain_params.pitch = pitch;
        grain_params.window = window_bank_->Get(window_shape);

        return grain_params;
    }

    /**
     * @brief Processes all active grains and sums their output.
     *
     * @return The summed and gain-compensated stereo output.
     */
    GrainOutput ProcessGrains() {
        float L = 0.0f;
        float R = 0.0f;

        for (size_t g = 0; g < grain_size; g++) {
            GrainOutput go = grains_[g].Process(buffer_L_, buffer_R_);
            L += go.L;
            R += go.R;
        }

        float comp = 1.0f / (float)grain_size;
        return {L * comp, R * comp};
    }

    GrainManagerParams params_;             /**< Internal parameter state. */
    DelayLine<float, max_delay> *buffer_L_; /**< Pointer to the left delay line. */
    DelayLine<float, max_delay> *buffer_R_; /**< Pointer to the right delay line. */
    Grain<max_delay> grains_[grain_size];   /**< The pool of grain voices. */
    FastRandom *rng_;                       /**< Pointer to a random number generator. */
    const WindowBank *window_bank_;         /**< Pointer to the window bank. */
    Quantizer q_;                           /**< Pitch quantizer instance. */
    float delay_min_;                       /**< Minimum grain delay time in samples. */
};

} // namespace dsp

#endif
