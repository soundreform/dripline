#pragma once
#ifndef GD_QUANTIZER_H
#define GD_QUANTIZER_H

#include "daisysp.h"

using daisysp::fastlog2f;
using daisysp::kOneTwelfth;

enum class QuantizeMode {
    FREE,
    SEMITONES,
    OCTAVES_FIFTHS,
};

class Quantizer {
  public:
    Quantizer() = default;
    ~Quantizer() = default;

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
        }

        // Convert back to linear pitch ratio
        return p_sign * exp2f(quantized_st * kOneTwelfth);
    }

  private:
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

#endif
