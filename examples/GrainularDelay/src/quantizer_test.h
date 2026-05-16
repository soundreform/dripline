#pragma once
#ifndef GD_QUANTIZER_TEST_H
#define GD_QUANTIZER_TEST_H

#include "quantizer.h"
#include <cassert>
#include <cmath>
#include <iostream>

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

#endif
