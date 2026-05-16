#pragma once
#ifndef GD_ROUTER_H
#define GD_ROUTER_H

#include "daisysp.h"
#include "tone.h"
#include <cmath>

using daisysp::DelayLine;

enum class RouterMode {
    PARALLEL,
    PING_PONG,
    SERIES,
};

struct RouterParams {
    RouterMode mode;
    float feedback;
    float tone;
    float mix;
    bool freeze;
    float drive;
    float output;
    bool trails_bypass;
};

struct RouterSignals {
    float dry_L;
    float dry_R;
    float wet_L;
    float wet_R;
};

struct RouterOutput {
    float L;
    float R;
};

template <size_t max_delay>
class Router {
  public:
    Router() = default;
    ~Router() = default;

    void Init(float sample_rate, DelayLine<float, max_delay> *L, DelayLine<float, max_delay> *R) {
        buffer_L_ = L;
        buffer_R_ = R;
        tone_.Init(sample_rate);
        chan_L_.Init(sample_rate);
        chan_R_.Init(sample_rate);
    }

    void Reset() {
        chan_L_.Reset();
        chan_R_.Reset();
        buffer_L_->Reset();
        buffer_R_->Reset();
    }

    RouterOutput Process(const RouterParams &params, const RouterSignals &signals) {
        float fb_L = RouteFeedbackLeft(params, signals);
        float fb_R = RouteFeedbackRight(params, signals);

        Tone::Output tone = tone_.Process(params.tone);

        if (!params.freeze) {
            buffer_L_->Write(chan_L_.Process(fb_L, tone, params.drive));
            buffer_R_->Write(chan_R_.Process(fb_R, tone, params.drive));
        }

        float out_L, out_R;

        if (params.trails_bypass) {
            out_L = signals.dry_L + (signals.wet_L * params.mix);
            out_R = signals.dry_R + (signals.wet_R * params.mix);
        } else {
            out_L = Mix(params.mix, signals.dry_L, signals.wet_L);
            out_R = Mix(params.mix, signals.dry_R, signals.wet_R);
        }
        out_L *= params.output;
        out_R *= params.output;

        if (params.mode == RouterMode::SERIES) {
            out_R = out_L;
        }

        return {tanhf(out_L), tanhf(out_R)};
    }

  private:
    struct FeedbackChannel {
        void Init(float sample_rate) {
            tone_.Init(sample_rate);
        }

        float Process(float in, const Tone::Output &coeffs, float drive) {
            return Saturate(tone_.Filter(in, coeffs) * drive);
        }

        void Reset() {
            tone_.Reset();
        }

      private:
        float Saturate(float in) {
            return in / (1.0f + fabsf(in));
        }

        Tone tone_;
    };

    float RouteFeedbackLeft(const RouterParams &params, const RouterSignals &signals) {
        float input_L = params.trails_bypass ? 0.0f : signals.dry_L;
        if (params.mode == RouterMode::PING_PONG) {
            return input_L + (signals.wet_R * params.feedback);
        }
        if (params.mode == RouterMode::SERIES) {
            return signals.wet_R + (signals.wet_L * params.feedback);
        }
        return input_L + (signals.wet_L * params.feedback);
    }

    float RouteFeedbackRight(const RouterParams &params, const RouterSignals &signals) {
        float input_R = params.trails_bypass ? 0.0f : signals.dry_R;
        if (params.mode == RouterMode::PING_PONG) {
            return input_R + (signals.wet_L * params.feedback);
        }
        return input_R + (signals.wet_R * params.feedback);
    }

    float Mix(float pos, float dry, float wet) {
        return (dry * (1.0f - pos)) + (wet * pos);
    }

    DelayLine<float, max_delay> *buffer_L_;
    DelayLine<float, max_delay> *buffer_R_;

    FeedbackChannel chan_L_;
    FeedbackChannel chan_R_;

    Tone tone_;
};

#endif
