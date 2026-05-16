#pragma once
#ifndef DSP_SCHEDULER_H
#define DSP_SCHEDULER_H

#include "dripline/frng.h"
#include "param.h"

using dripline::FastRandom;

namespace dsp {

/** @brief Defines the rhythmic behavior of the grain scheduler. */
enum class SchedulerRhythm {
    FREE,   /**< Asynchronous, free-running triggering. */
    SYNCED, /**< Triggering is synced to the tap tempo. */
    BURST,  /**< Triggers bursts of grains synced to the tap tempo. */
    COUNT,  /**< The number of available rhythm modes. */
};

/** @brief Defines the tempo subdivisions for synced modes. */
enum class SchedulerSubdivision {
    QUARTER,         /**< Quarter notes. */
    DOTTED_EIGHTH,   /**< Dotted-eighth notes. */
    EIGHTH,          /**< Eighth notes. */
    EIGHTH_TRIPLETS, /**< Eighth-note triplets. */
    SIXTEENTH,       /**< Sixteenth notes. */
    COUNT,           /**< The number of available subdivisions. */
};

/** @brief A struct holding raw, unprocessed parameters for the Scheduler. */
struct RawSchedulerParams {
    SchedulerRhythm rhythm; /**< The raw rhythm mode setting. */
    float manual_freq;      /**< The raw manual frequency/subdivision control value. */
    float tapped_freq;      /**< The current frequency from the tap tempo module. */
};

/**
 * @brief A struct holding the smoothed and discrete parameters for the
 * Scheduler.
 */
struct SchedulerParams {
    DiscreteParam<SchedulerRhythm> rhythm;           /**< Discrete rhythm mode parameter. */
    DiscreteParam<SchedulerSubdivision> subdivision; /**< Discrete subdivision parameter. */
    SmoothedParam manual_freq;                       /**< Smoothed manual frequency parameter. */
    SmoothedParam tapped_freq;                       /**< Smoothed tapped frequency parameter. */
    float density;                                   /**< The calculated trigger density in Hz. */

    /**
     * @brief Initializes the scheduler parameters.
     * @param smoothing_coeff The coefficient for parameter smoothing.
     */
    void Init(float smoothing_coeff) {
        constexpr float kMaxFreq = 100.0f;
        constexpr float kMinFreq = 0.1f;
        constexpr float kDefaultFreq = 20.0f;
        constexpr SmoothedParam::Curve kFreqCurve = SmoothedParam::Curve::LOGARITHMIC;

        density = 0.0f;

        rhythm.Init(SchedulerRhythm::FREE, SchedulerRhythm::COUNT);
        subdivision.Init(SchedulerSubdivision::QUARTER, SchedulerSubdivision::COUNT);
        manual_freq.Init(kDefaultFreq, kMinFreq, kMaxFreq, smoothing_coeff, kFreqCurve);
        tapped_freq.Init(kDefaultFreq, kMinFreq, kMaxFreq, smoothing_coeff, kFreqCurve);
    }

    /**
     * @brief Updates the parameters from a raw parameter struct.
     * @param raw The struct containing the latest raw parameter values.
     */
    void Process(const RawSchedulerParams &raw) {
        rhythm.Set(raw.rhythm);

        manual_freq.Process(raw.manual_freq);
        tapped_freq.Process(raw.tapped_freq);

        switch (rhythm.Value()) {
        case SchedulerRhythm::SYNCED:
        case SchedulerRhythm::BURST:
            // In SYNCED or BURST mode, density is derived from tap tempo,
            // and the manual_freq knob is repurposed to select subdivisions.
            subdivision.Set(raw.manual_freq);
            density = CalculateDensityBasedOnTempo(tapped_freq.Value(), subdivision.Value());
            break;
        case SchedulerRhythm::FREE:
        case SchedulerRhythm::COUNT:
        default:
            // In FREE mode, the density knob directly controls the frequency.
            density = manual_freq.Value();
            break;
        }
    }

  private:
    /**
     * @brief Calculates the trigger density based on a tempo and subdivision.
     *
     * @param freq The base tempo frequency in Hz.
     * @param subdivision The tempo subdivision.
     * @return The calculated trigger density in Hz.
     */
    float CalculateDensityBasedOnTempo(float freq, SchedulerSubdivision subdivision) {
        float mult = 1.0f;
        switch (subdivision) {
        case SchedulerSubdivision::QUARTER:
            mult = 1.0f;
            break;
        case SchedulerSubdivision::DOTTED_EIGHTH:
            mult = 1.5f;
            break;
        case SchedulerSubdivision::EIGHTH:
            mult = 2.0f;
            break;
        case SchedulerSubdivision::EIGHTH_TRIPLETS:
            mult = 3.0f;
            break;
        case SchedulerSubdivision::SIXTEENTH:
            mult = 4.0f;
            break;
        case SchedulerSubdivision::COUNT:
            break;
        }
        return freq * mult;
    }
};

/** @brief Configuration settings for initializing the Scheduler. */
struct SchedulerConfig {
    float sample_rate;     /**< The audio sample rate in Hz. */
    FastRandom *rng;       /**< Pointer to a random number generator. */
    float smoothing_coeff; /**< The coefficient for parameter smoothing. */
};

/**
 * @brief Determines when to trigger new grains based on density and rhythm mode.
 *
 * This class uses an accumulator-based approach. At each sample, it increments
 * an accumulator by the current density. When the accumulator crosses a
 * threshold, it returns a number of triggers to fire.
 */
class Scheduler {
  public:
    Scheduler() = default;
    ~Scheduler() = default;

    /**
     * @brief Initializes the scheduler.
     * @param config The configuration settings for the scheduler.
     */
    void Init(const SchedulerConfig &config) {
        inverted_sample_rate_ = 1.0f / config.sample_rate;
        rng_ = config.rng;
        accumulator_ = 0.0f;
        threshold_ = 1.0f;
        params_.Init(config.smoothing_coeff);
    }

    /** @brief Resets the scheduler's accumulator and threshold. */
    void Reset() {
        accumulator_ = 0.0f;
        threshold_ = 1.0f;
    }

    /**
     * @brief Updates the scheduler's parameters from a raw parameter struct.
     * @param raw The struct containing the latest raw parameter values.
     */
    void UpdateParams(const RawSchedulerParams &raw) {
        params_.Process(raw);
    }

    /**
     * @brief Processes one sample tick of the scheduler.
     *
     * @param mod_scaler A modulation scaler (0.0-1.0) that can affect the number of triggers fired, depending on the rhythm mode.
     * @return The number of new grains that should be triggered in this sample.
     */
    int Process(float mod_scaler = 1.0f) {
        int triggers_to_fire = 0;

        // In non-free modes, the threshold is always 1.0 for predictable rhythm.
        // Reset it here to ensure correct timing when switching from FREE mode.
        if (params_.rhythm.Value() != SchedulerRhythm::FREE) {
            threshold_ = 1.0f;
        }

        accumulator_ += params_.density * inverted_sample_rate_;

        while (accumulator_ >= threshold_) {
            accumulator_ -= threshold_;

            switch (params_.rhythm.Value()) {
            case SchedulerRhythm::BURST: // Scale cluster size based on modulation
                triggers_to_fire += (int)(1.0f + 5.0f * mod_scaler);
                break;
            case SchedulerRhythm::SYNCED: // Scale doubling based on modulation
                triggers_to_fire += (int)(1.0f + 1.0f * mod_scaler);
                break;
            case SchedulerRhythm::FREE: {
                // Free mode has a random threshold for the *next* trigger.
                float jitter_amount = 0.15f * mod_scaler;
                threshold_ = (1.0f - jitter_amount) + (rng_->NextFloat() * 2.0f * jitter_amount);
                triggers_to_fire += 1;
                break;
            }
            case SchedulerRhythm::COUNT:
                break;
            }

            // For synced modes, we only want one set of triggers per accumulation cycle.
            if (params_.rhythm.Value() != SchedulerRhythm::FREE) {
                break;
            }
        }

        return triggers_to_fire;
    }

    /**
     * @brief Gets the current tempo subdivision.
     * @return The current scheduler subdivision.
     */
    SchedulerSubdivision GetSubdivision() const {
        return params_.subdivision.Value();
    }

    /**
     * @brief Gets the current trigger frequency (density) in Hz.
     * @return The trigger frequency in Hz.
     */
    float GetFrequencyHz() const {
        return params_.density;
    }

  private:
    SchedulerParams params_;     /**< Internal parameter state. */
    FastRandom *rng_;            /**< Pointer to a random number generator. */
    float inverted_sample_rate_; /**< Cached inverse of the sample rate. */
    float accumulator_;          /**< The phase accumulator. */
    float threshold_ = 1.0f;     /**< The trigger threshold. */
};

} // namespace dsp

#endif
