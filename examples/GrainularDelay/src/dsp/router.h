#pragma once
#ifndef GD_ROUTER_H
#define GD_ROUTER_H

#include "daisysp.h"
#include "param.h"
#include "tone_filter.h"
#include <cmath>

using daisysp::DelayLine;

namespace dsp {

/** @brief Defines the stereo routing configuration for the feedback path. */
enum class RouterMode {
    PARALLEL,  /**< L->L, R->R feedback. */
    PING_PONG, /**< L->R, R->L feedback. */
    SERIES,    /**< L->R, R->L feedback, with mono output. */
    COUNT,     /**< The number of available routing modes. */
};

/** @brief A struct holding raw, unprocessed parameters for the Router. */
struct RawRouterParams {
    bool bypass;     /**< True if the trails bypass is active. */
    float drive;     /**< The input drive amount. */
    float feedback;  /**< The feedback amount. */
    bool freeze;     /**< True if the delay buffer is frozen. */
    float mix;       /**< The dry/wet mix. */
    RouterMode mode; /**< The stereo routing mode. */
    float output;    /**< The final output level. */
    float tone;      /**< The tone control position. */
};

/**
 * @brief A struct holding the smoothed and discrete parameters for the Router.
 */
struct RouterParams {
    bool freeze; /**< The current freeze state. */
    bool bypass; /**< The current bypass state. */

    DiscreteParam<RouterMode> mode; /**< Discrete routing mode parameter. */

    SmoothedParam drive;    /**< Smoothed drive parameter. */
    SmoothedParam feedback; /**< Smoothed feedback parameter. */
    SmoothedParam mix;      /**< Smoothed mix parameter. */
    SmoothedParam output;   /**< Smoothed output level parameter. */
    SmoothedParam tone;     /**< Smoothed tone parameter. */

    void Init(float sample_rate, float smoothing_coeff) {
        drive.Init(1.0f, 1.0f, 4.0f, smoothing_coeff);
        feedback.Init(0.0f, 0.0f, 0.85f, smoothing_coeff);
        mix.Init(0.5f, 0.0f, 1.0f, smoothing_coeff);
        mode.Init(RouterMode::PARALLEL, RouterMode::COUNT);
        output.Init(1.0f, 0.0f, 1.0f, smoothing_coeff);
        tone.Init(0.5f, 0.0f, 1.0f, smoothing_coeff);
    }

    void Process(const RawRouterParams &raw) {
        drive.Process(raw.drive);
        feedback.Process(raw.feedback);
        mix.Process(raw.mix);
        mode.Set(static_cast<int>(raw.mode));
        output.Process(raw.output);
        tone.Process(raw.tone);

        freeze = raw.freeze;
        bypass = raw.bypass;
    }
};

/**
 * @brief A struct representing the audio inputs to the Router.
 */
struct RouterInput {
    float dry_L; /**< The dry left channel input. */
    float dry_R; /**< The dry right channel input. */
    float wet_L; /**< The wet (grain) left channel input. */
    float wet_R; /**< The wet (grain) right channel input. */
};

/**
 * @brief A struct representing the audio outputs from the Router.
 */
struct RouterOutput {
    float L;      /**< The final left channel output. */
    float R;      /**< The final right channel output. */
    bool clipped; /**< Flag indicating if clipping occurred. */
};

/**
 * @brief Configuration settings for initializing the Router.
 *
 * @tparam max_delay The maximum size of the delay lines in samples.
 */
template <size_t max_delay>
struct RouterConfig {
    float sample_rate;                     /**< The audio sample rate in Hz. */
    DelayLine<float, max_delay> *buffer_L; /**< Pointer to the left delay line. */
    DelayLine<float, max_delay> *buffer_R; /**< Pointer to the right delay line. */
    float smoothing_coeff;                 /**< The coefficient for parameter smoothing. */
};

/**
 * @brief Handles audio routing, feedback, filtering, and mixing.
 *
 * This class takes the dry input and the wet signal from the GrainManager,
 * processes them through a feedback loop with filtering and saturation, and
 * mixes them to produce the final output.
 *
 * @tparam max_delay The maximum size of the delay lines in samples.
 */
template <size_t max_delay>
class Router {
  public:
    Router() = default;
    ~Router() = default;

    void Init(const RouterConfig<max_delay> &config) {
        buffer_L_ = config.buffer_L;
        buffer_R_ = config.buffer_R;
        tone_filter_.Init(config.sample_rate);
        params_.Init(config.sample_rate, config.smoothing_coeff);
    }

    /** @brief Resets the tone filter and delay line buffers. */
    void Reset() {
        tone_filter_.Reset();
        buffer_L_->Reset();
        buffer_R_->Reset();
    }

    /**
     * @brief Updates the router's parameters from a raw parameter struct.
     * @param raw The struct containing the latest raw parameter values.
     */
    void UpdateParams(const RawRouterParams &raw) {
        params_.Process(raw);
        tone_filter_.SetPos(params_.tone);
    }

    /**
     * @brief Processes a single stereo audio sample.
     *
     * @param in The struct containing dry and wet audio inputs.
     * @return The processed stereo output sample.
     */
    RouterOutput Process(const RouterInput &in) {
        float fb_L = RouteFeedbackLeft(in);
        float fb_R = RouteFeedbackRight(in);
        bool clipped = false;

        if (!params_.freeze) {
            float filtered_L = tone_filter_.ProcessLeft(fb_L);
            float filtered_R = tone_filter_.ProcessRight(fb_R);

            float pre_saturate_L = filtered_L * params_.drive;
            float pre_saturate_R = filtered_R * params_.drive;

            if (fabsf(pre_saturate_L) >= 1.0f || fabsf(pre_saturate_R) >= 1.0f) {
                clipped = true;
            }

            buffer_L_->Write(Saturate(pre_saturate_L));
            buffer_R_->Write(Saturate(pre_saturate_R));
        }

        float out_L, out_R;

        if (params_.bypass) {
            out_L = in.dry_L + (in.wet_L * params_.mix);
            out_R = in.dry_R + (in.wet_R * params_.mix);
        } else {
            out_L = Mix(params_.mix, in.dry_L, in.wet_L);
            out_R = Mix(params_.mix, in.dry_R, in.wet_R);
        }
        out_L *= params_.output;
        out_R *= params_.output;

        if (params_.mode == RouterMode::SERIES) {
            out_R = out_L;
        }

        return {tanhf(out_L), tanhf(out_R), clipped};
    }

  private:
    /**
     * @brief A simple soft-clipping saturator.
     * @param in The input sample.
     * @return The saturated output sample.
     */
    float Saturate(float in) {
        return in / (1.0f + fabsf(in));
    }

    /**
     * @brief Routes the feedback signal for the left channel based on the current mode.
     * @return The combined feedback signal for the left channel.
     */
    float RouteFeedbackLeft(const RouterInput &in) {
        float input_L = params_.bypass ? 0.0f : in.dry_L;
        if (params_.mode == RouterMode::PING_PONG) {
            return input_L + (in.wet_R * params_.feedback);
        }
        if (params_.mode == RouterMode::SERIES) {
            return in.wet_R + (in.wet_L * params_.feedback);
        }
        return input_L + (in.wet_L * params_.feedback);
    }

    /**
     * @brief Routes the feedback signal for the right channel based on the current mode.
     * @return The combined feedback signal for the right channel.
     */
    float RouteFeedbackRight(const RouterInput &in) {
        float input_R = params_.bypass ? 0.0f : in.dry_R;
        if (params_.mode == RouterMode::PING_PONG) {
            return input_R + (in.wet_L * params_.feedback);
        }
        return input_R + (in.wet_R * params_.feedback);
    }

    /**
     * @brief Mixes between a dry and wet signal.
     * @return The mixed signal.
     */
    float Mix(float pos, float dry, float wet) {
        return (dry * (1.0f - pos)) + (wet * pos);
    }

    DelayLine<float, max_delay> *buffer_L_; /**< Pointer to the left delay line. */
    DelayLine<float, max_delay> *buffer_R_; /**< Pointer to the right delay line. */

    RouterParams params_;    /**< Internal parameter state. */
    ToneFilter tone_filter_; /**< The tone filter instance. */
};

} // namespace dsp

#endif
