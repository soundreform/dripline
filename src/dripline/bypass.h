#pragma once
#ifndef DL_BYPASS_H
#define DL_BYPASS_H

#include "daisy.h"
#include "dripline.h"
#include "ramp.h"
#include <cmath>
#include <cstddef>

using daisy::GPIO;
using dripline::Dripline;
using dripline::Ramp;

namespace dripline {
/**
 * @brief Manages audio bypass logic using hardware relays and mutes.
 *
 * This class handles the transition between bypassed (hard-wired input to output)
 * and active processing states. It uses gain ramping
 * and timed muting to prevent pops and clicks during switching.
 */
class AudioBypass {
  public:
    /** @brief Default constructor. */
    AudioBypass() = default;

    /** @brief Default destructor. */
    ~AudioBypass() = default;

    /**
     * @brief Initializes the bypass using a Dripline hardware instance.
     * @param hw      Pointer to the hardware interface.
     * @param channel The audio channel to manage.
     */
    void Init(Dripline *hw, Dripline::Channel channel) {
        Init(hw->AudioSampleRate(), &hw->relays[channel], &hw->mutes[channel]);
    }

    /**
     * @brief Initializes the bypass with specific hardware pins.
     * @param sample_rate The system audio sample rate.
     * @param relay       GPIO for the bypass relay.
     * @param mute        GPIO for the audio mute circuit.
     */
    void Init(float sample_rate, GPIO *relay, GPIO *mute) {
        relay_ = relay;
        mute_ = mute;

        bypassed_ = true;
        transitioning_ = false;
        gain_ = 0.0f;

        mute_->Write(false);
        relay_->Write(false);

        fade_in_ramp_.Init(sample_rate, 10.0f, Ramp::Dir::UP);
        fade_out_ramp_.Init(sample_rate, 2.0f, Ramp::Dir::DOWN);
        mute_wait_ramp_.Init(sample_rate, 3.0f, Ramp::Dir::UP);
        relay_wait_ramp_.Init(sample_rate, 2.0f, Ramp::Dir::UP);
    }

    /**
     * @brief Updates the bypass gain ramp and advances hardware state logic.
     *
     * This should be called once per audio sample to handle the timing sequence for
     * the relay and mute circuits.
     *
     * @return The current gain multiplier (0.0 to 1.0) to be applied to audio.
     */
    float Process() {
        TransitionHardware();
        return gain_;
    }

    /**
     * @brief Retrieves the current gain value.
     * @return The current gain multiplier in the range [0.0, 1.0].
     */
    float Value() {
        return gain_;
    }

    /**
     * @brief Sets the bypass state.
     * @param bypass True to bypass, false to enable processing.
     */
    void SetBypass(bool bypass) {
        if (bypassed_ != bypass) {
            bypassed_ = bypass;
            transitioning_ = true;
        }
    }

  private:
    Dripline *hw_;         /**< Pointer to the Dripline hardware abstraction. */
    GPIO *relay_;          /**< Relay control pin. */
    GPIO *mute_;           /**< Mute control pin. */
    Ramp fade_in_ramp_;    /**< Ramp for smooth gain cross-fading. */
    Ramp fade_out_ramp_;   /**< Ramp for smooth gain cross-fading. */
    Ramp mute_wait_ramp_;  /**< Timing ramp for hardware transitions. */
    Ramp relay_wait_ramp_; /**< Timing ramp for hardware transitions. */
    bool bypassed_;        /**< Current user-requested bypass state. */
    bool transitioning_;   /**< True if a hardware switch is in progress. */
    float gain_;           /**< Calculated gain for audio processing. */

    /**
     * @brief State machine for handling timed hardware transitions.
     */
    void TransitionHardware() {
        if (!transitioning_) {
            return;
        }

        if (bypassed_ && fade_out_ramp_.Process() > 0.0f) {
            gain_ = fade_out_ramp_.Value();
            return;
        }

        mute_->Write(true);

        if (relay_wait_ramp_.Process() < 1.0f) {
            return;
        }

        relay_->Write(!bypassed_);

        if (mute_wait_ramp_.Process() < 1.0f) {
            return;
        }

        mute_->Write(false);

        if (!bypassed_ && fade_in_ramp_.Process() < 1.0f) {
            gain_ = fade_in_ramp_.Value();
            return;
        }

        fade_in_ramp_.Reset();
        fade_out_ramp_.Reset();
        mute_wait_ramp_.Reset();
        relay_wait_ramp_.Reset();

        gain_ = bypassed_ ? 0.0f : 1.0f;
        transitioning_ = false;
    }
};
}; // namespace dripline

#endif
