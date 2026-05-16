#pragma once
#ifndef DL_BYPASS_H
#define DL_BYPASS_H

#include <cstddef>
#include "daisy.h"
#include "dripline.h"
#include "ramp.h"

using daisy::GPIO;
using dripline::Dripline;
using dripline::Ramp;

namespace dripline
{
    /**
     * @brief Manages audio bypass logic using hardware relays and mutes.
     *
     * This class handles the transition between bypassed (hard-wired input to output)
     * and active processing states. It uses gain ramping
     * and timed muting to prevent pops and clicks during switching.
     */
    class AudioBypass
    {
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
        void Init(Dripline *hw, Dripline::Channel channel);

        /**
         * @brief Initializes the bypass with specific hardware pins.
         * @param sample_rate The system audio sample rate.
         * @param relay       GPIO for the bypass relay.
         * @param mute        GPIO for the audio mute circuit.
         */
        void Init(float sample_rate, GPIO *relay, GPIO *mute);

        /**
         * @brief Updates the bypass gain ramp and advances hardware state logic.
         *
         * This should be called once per audio sample to handle the timing sequence for
         * the relay and mute circuits.
         *
         * @return The current gain multiplier (0.0 to 1.0) to be applied to audio.
         */
        float Process();

        /**
         * @brief Retrieves the current gain value.
         * @return The current gain multiplier in the range [0.0, 1.0].
         */
        float Value();

        /**
         * @brief Sets the bypass state.
         * @param bypass True to bypass, false to enable processing.
         */
        void SetBypass(bool bypass);

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
        void TransitionHardware();
    };
};

#endif
