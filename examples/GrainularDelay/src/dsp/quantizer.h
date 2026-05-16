#pragma once
#ifndef GD_QUANTIZER_H
#define GD_QUANTIZER_H

#include "daisysp.h"

using daisysp::fastlog2f;
using daisysp::kOneTwelfth;

namespace dsp {

/** @brief Defines the pitch quantization mode. */
enum class QuantizeMode {
    FREE,           /**< No quantization. */
    SEMITONES,      /**< Quantize to the nearest semitone. */
    OCTAVES_FIFTHS, /**< Quantize to the nearest root, octave, or fifth. */
    COUNT,          /**< The number of available quantization modes. */
};

/**
 * @brief A class for quantizing pitch values to musical intervals.
 *
 * This class takes a linear pitch ratio and snaps it to the nearest note in a
 * given scale, defined by the QuantizeMode.
 */
class Quantizer {
  public:
    Quantizer() = default;
    ~Quantizer() = default;

    /**
     * @brief Processes a pitch value, quantizing it based on the selected mode.
     *
     * @param mode The quantization mode to use.
     * @param pitch The input pitch ratio.
     * @return The quantized pitch ratio.
     */
    float Process(const QuantizeMode mode, const float pitch) {
        float p_abs = fabsf(pitch);
        float p_sign = (pitch >= 0.0f) ? 1.0f : -1.0f;

        // Dead zone to prevent mathematical instability at near-zero ratios
        if (p_abs < 0.001f) {
            return 0.0f;
        }

        // Convert linear pitch ratio to logarithmic semitones
        float semitones = 12.0f * fastlog2f(p_abs);
        float quantized_st = 0.0f;

        switch (mode) {
        case QuantizeMode::SEMITONES:
            quantized_st = roundf(semitones);
            break;
        case QuantizeMode::OCTAVES_FIFTHS:
            quantized_st = QuantizeToOctaveOrFifth(semitones);
            break;
        case QuantizeMode::FREE:
            return pitch;
        case QuantizeMode::COUNT:
            break;
        }

        // Convert back to linear pitch ratio
        return p_sign * exp2f(quantized_st * kOneTwelfth);
    }

  private:
    /**
     * @brief Quantizes a pitch value in semitones to the nearest root, octave, or fifth.
     *
     * @param semitones The input pitch in semitones.
     * @return The quantized pitch in semitones.
     */
    float QuantizeToOctaveOrFifth(float semitones) {
        int st_rounded = (int)roundf(semitones);
        int octave = st_rounded / 12;

        // Correct for integer division floor behavior for negative numbers
        if (st_rounded < 0 && st_rounded % 12 != 0) {
            octave--;
        }

        int rel = st_rounded - (octave * 12);

        // Snap relative semitones (0-11) to Root (0), Perfect Fifth (7), or Octave (12)
        // Thresholds are roughly at the major third (4) and minor seventh (10)
        int snapped_rel = (rel < 4) ? 0 : (rel < 10) ? 7 : 12;

        return (float)(octave * 12 + snapped_rel);
    }
};

} // namespace dsp

#endif
