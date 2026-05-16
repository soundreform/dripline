#pragma once
#ifndef GD_MODULATOR_H
#define GD_MODULATOR_H

#include "daisysp.h"
#include "param.h"

using daisysp::Oscillator;

namespace dsp {

/** @brief Defines the depth of the LFO modulation. */
enum class ModulatorDepth {
    NONE,  /**< No modulation. */
    SOME,  /**< Moderate modulation depth. */
    MORE,  /**< Full modulation depth. */
    COUNT, /**< The number of available depth settings. */
};

/** @brief A struct holding raw, unprocessed parameters for the Modulator. */
struct RawModulatorParams {
    ModulatorDepth depth; /**< The raw modulation depth setting. */
};

/**
 * @brief A struct holding the discrete parameters for the Modulator.
 */
struct ModulatorParams {
    DiscreteParam<ModulatorDepth> depth; /**< The discrete modulation depth parameter. */

    /** @brief Initializes the modulator parameters. */
    void Init(float smoothing_coeff) {
        // smoothing_coeff is unused for discrete params, but kept for API consistency.
        (void)smoothing_coeff;
        depth.Init(ModulatorDepth::NONE, ModulatorDepth::COUNT);
    }

    /** @brief Updates the parameters from a raw parameter struct. */
    void Process(const RawModulatorParams &raw) {
        depth.Set(static_cast<int>(raw.depth));
    }
};

/** @brief Configuration settings for initializing the Modulator. */
struct ModulatorConfig {
    float sample_rate;     /**< The audio sample rate in Hz. */
    float smoothing_coeff; /**< The coefficient for parameter smoothing. */
};

/**
 * @brief A simple LFO for modulation effects.
 *
 * This class generates a triangle wave LFO that can be used to modulate other
 * parameters in the audio engine. Its output is a scaler value from 0.0 to 1.0.
 */
class Modulator {
  public:
    Modulator() = default;
    ~Modulator() = default;

    /**
     * @brief Initializes the modulator and its internal LFO.
     * @param config The configuration settings for the modulator.
     */
    void Init(const ModulatorConfig &config) {
        lfo_.Init(config.sample_rate);
        lfo_.SetWaveform(daisysp::Oscillator::WAVE_POLYBLEP_TRI);
        lfo_.SetFreq(0.1f);
        lfo_.SetAmp(1.0f);
        params_.Init(config.smoothing_coeff);
    }

    /**
     * @brief Updates the modulator's parameters from a raw parameter struct.
     * @param raw The struct containing the latest raw parameter values.
     */
    void UpdateParams(const RawModulatorParams &raw) {
        params_.Process(raw);
    }

    /**
     * @brief Processes one sample of the LFO.
     *
     * @return The current output value of the LFO, scaled by the depth setting.
     */
    float Process() {
        float lfo_freq = 0.1f;
        if (params_.depth.Value() == ModulatorDepth::MORE) {
            lfo_freq = 0.35f;
        }
        lfo_.SetFreq(lfo_freq);

        float lfo_val = (lfo_.Process() + 1.0f) * 0.5f;

        float mod_scaler = 0.0f;
        if (params_.depth.Value() == ModulatorDepth::SOME) {
            mod_scaler = lfo_val * 0.5f;
        } else if (params_.depth.Value() == ModulatorDepth::MORE) {
            mod_scaler = lfo_val;
        }

        return mod_scaler;
    }

  private:
    ModulatorParams params_; /**< Internal parameter state. */
    Oscillator lfo_;         /**< The internal LFO instance. */
};

} // namespace dsp

#endif
