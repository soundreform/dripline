#pragma once
#ifndef GD_QUANTIZER_TEST_H
#define GD_QUANTIZER_TEST_H

#include "quantizer.h"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace dsp;

// Using a small epsilon for float comparisons
constexpr float kEpsilon = 0.0001f;

void test_quantizer_free_mode() {
    std::cout << "Testing Quantizer FREE mode... ";
    Quantizer quantizer;
    QuantizeMode mode = QuantizeMode::FREE;

    float pitch = 1.0f;
    assert(std::fabs(quantizer.Process(mode, pitch) - 1.0f) < kEpsilon);

    pitch = 0.5f;
    assert(std::fabs(quantizer.Process(mode, pitch) - 0.5f) < kEpsilon);

    pitch = 2.0f;
    assert(std::fabs(quantizer.Process(mode, pitch) - 2.0f) < kEpsilon);

    pitch = -0.75f;
    assert(std::fabs(quantizer.Process(mode, pitch) - -0.75f) < kEpsilon);

    std::cout << "Passed." << std::endl;
}

void test_quantizer_semitones_mode() {
    std::cout << "Testing Quantizer SEMITONES mode... ";
    Quantizer quantizer;
    QuantizeMode mode = QuantizeMode::SEMITONES;

    // 1.0f is 0 semitones
    float pitch = 1.0f;
    assert(std::fabs(quantizer.Process(mode, pitch) - 1.0f) < kEpsilon);

    // Test values around 0.5 semitone mark (midpoint between 0 and 1 semitone)
    // 2^(0.5/12) approx 1.0293
    pitch = 1.02f; // Closer to 0 semitones
    assert(std::fabs(quantizer.Process(mode, pitch) - 1.0f) < kEpsilon);
    pitch = 1.03f; // Closer to 1 semitone
    assert(std::fabs(quantizer.Process(mode, pitch) - std::pow(2.0f, 1.0f / 12.0f)) < kEpsilon);

    // 2.0f is 12 semitones up (2^1)
    // 2^(11.5/12) approx 1.966
    pitch = 1.9f; // Closer to 11 semitones
    assert(std::fabs(quantizer.Process(mode, pitch) - std::pow(2.0f, 11.0f / 12.0f)) < kEpsilon);
    pitch = 1.98f; // Closer to 12 semitones
    assert(std::fabs(quantizer.Process(mode, pitch) - 2.0f) < kEpsilon);

    // Negative pitches
    pitch = -1.02f; // Closer to 0 semitones
    assert(std::fabs(quantizer.Process(mode, pitch) - -1.0f) < kEpsilon);
    pitch = -1.03f; // Closer to 1 semitone
    assert(std::fabs(quantizer.Process(mode, pitch) - -std::pow(2.0f, 1.0f / 12.0f)) < kEpsilon);

    pitch = -0.95f; // Closer to -1 semitone
    assert(std::fabs(quantizer.Process(mode, pitch) - -std::pow(2.0f, -1.0f / 12.0f)) < kEpsilon);

    std::cout << "Passed." << std::endl;
}

void test_quantizer_octaves_fifths_mode() {
    std::cout << "Testing Quantizer OCTAVES_FIFTHS mode... ";
    Quantizer quantizer;
    QuantizeMode mode = QuantizeMode::OCTAVES_FIFTHS;

    // 1.0f is 0 semitones (root)
    float pitch = 1.0f;
    assert(std::fabs(quantizer.Process(mode, pitch) - 1.0f) < kEpsilon);

    // Pitches near 0 semitones (snaps to 0)
    pitch = std::pow(2.0f, 1.0f / 12.0f); // 1 semitone
    assert(std::fabs(quantizer.Process(mode, pitch) - 1.0f) < kEpsilon);
    pitch = std::pow(2.0f, 3.0f / 12.0f); // 3 semitones
    assert(std::fabs(quantizer.Process(mode, pitch) - 1.0f) < kEpsilon);

    // Pitches near 7 semitones (perfect fifth)
    pitch = std::pow(2.0f, 4.0f / 12.0f); // 4 semitones (major third) - snaps to 7
    assert(std::fabs(quantizer.Process(mode, pitch) - std::pow(2.0f, 7.0f / 12.0f)) < kEpsilon);
    pitch = std::pow(2.0f, 6.0f / 12.0f); // 6 semitones (tritone) - snaps to 7
    assert(std::fabs(quantizer.Process(mode, pitch) - std::pow(2.0f, 7.0f / 12.0f)) < kEpsilon);
    pitch = std::pow(2.0f, 9.0f / 12.0f); // 9 semitones (major sixth) - snaps to 7
    assert(std::fabs(quantizer.Process(mode, pitch) - std::pow(2.0f, 7.0f / 12.0f)) < kEpsilon);

    // Pitches near 12 semitones (octave)
    pitch = std::pow(2.0f, 10.0f / 12.0f); // 10 semitones (minor seventh) - snaps to 12
    assert(std::fabs(quantizer.Process(mode, pitch) - 2.0f) < kEpsilon);
    pitch = std::pow(2.0f, 11.0f / 12.0f); // 11 semitones (major seventh) - snaps to 12
    assert(std::fabs(quantizer.Process(mode, pitch) - 2.0f) < kEpsilon);

    // Negative pitches
    pitch = -std::pow(2.0f, 1.0f / 12.0f); // -1 semitone - snaps to 0
    assert(std::fabs(quantizer.Process(mode, pitch) - -1.0f) < kEpsilon);

    pitch = -std::pow(2.0f, 4.0f / 12.0f); // -4 semitones - snaps to -5th (which is -7 semitones)
    assert(std::fabs(quantizer.Process(mode, pitch) - -std::pow(2.0f, 7.0f / 12.0f)) < kEpsilon);

    pitch = -std::pow(2.0f, 10.0f / 12.0f); // -10 semitones - snaps to -12 semitones (octave down)
    assert(std::fabs(quantizer.Process(mode, pitch) - -2.0f) < kEpsilon);

    std::cout << "Passed." << std::endl;
}

void test_quantizer_zero_pitch() {
    std::cout << "Testing Quantizer with zero pitch... ";
    Quantizer quantizer;

    QuantizeMode mode = QuantizeMode::FREE;
    float pitch = 0.0f;
    assert(std::fabs(quantizer.Process(mode, pitch) - 0.0f) < kEpsilon);

    mode = QuantizeMode::SEMITONES;
    pitch = 0.0f;
    assert(std::fabs(quantizer.Process(mode, pitch) - 0.0f) < kEpsilon);

    mode = QuantizeMode::OCTAVES_FIFTHS;
    pitch = 0.0f;
    assert(std::fabs(quantizer.Process(mode, pitch) - 0.0f) < kEpsilon);

    // Test values inside the dead zone ( < 0.001), they should return 0.0
    mode = QuantizeMode::SEMITONES;
    pitch = 0.00001f;
    assert(std::fabs(quantizer.Process(mode, pitch) - 0.0f) < kEpsilon);
    pitch = -0.00001f;
    assert(std::fabs(quantizer.Process(mode, pitch) - 0.0f) < kEpsilon);

    std::cout << "Passed." << std::endl;
}

void test_quantizer_values() {
    std::cout << "Testing Quantizer value ranges... ";

    Quantizer q;

    // Test OCTAVES_FIFTHS mode boundaries
    // Values are in semitones, expected results are in semitones

    // Test root snapping (rel < 4)
    float pitch_in_root = exp2f(2.0f * kOneTwelfth); // 2 semitones
    float quantized_root = q.Process(QuantizeMode::OCTAVES_FIFTHS, pitch_in_root);
    assert(std::fabs(quantized_root - 1.0f) < 0.001f); // Should snap to 0 semitones (ratio 1.0)

    // Test fifth snapping (4 <= rel < 10)
    float pitch_in_fifth = exp2f(6.0f * kOneTwelfth); // 6 semitones
    float quantized_fifth = q.Process(QuantizeMode::OCTAVES_FIFTHS, pitch_in_fifth);
    float expected_fifth = exp2f(7.0f * kOneTwelfth); // Should snap to 7 semitones
    assert(std::fabs(quantized_fifth - expected_fifth) < 0.001f);

    // Test octave snapping (rel >= 10)
    float pitch_in_octave = exp2f(11.0f * kOneTwelfth); // 11 semitones
    float quantized_octave = q.Process(QuantizeMode::OCTAVES_FIFTHS, pitch_in_octave);
    assert(std::fabs(quantized_octave - 2.0f) < 0.001f); // Should snap to 12 semitones (ratio 2.0)

    // Test negative pitch
    float pitch_in_neg = -exp2f(14.0f * kOneTwelfth); // -14 semitones
    float quantized_neg = q.Process(QuantizeMode::OCTAVES_FIFTHS, pitch_in_neg);
    float expected_neg = -exp2f(12.0f * kOneTwelfth); // Should snap to -12 semitones
    assert(std::fabs(quantized_neg - expected_neg) < 0.001f);

    std::cout << "Passed." << std::endl;
}

#endif
