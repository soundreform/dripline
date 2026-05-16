#pragma once
#ifndef GD_TONE_TEST_H
#define GD_TONE_TEST_H

#include "tone.h"
#include <cassert>
#include <cmath>
#include <iostream>

void test_tone_mapping() {
    std::cout << "Testing Tone class frequency mapping... ";
    Tone tone;
    const float sr = 48000.0f;
    tone.Init(sr);

    // 1. Neutral Position (0.5)
    // lp_param = 1.0, hp_param = 0.0
    // lp_freq = 200 + 18000 = 18200Hz
    // hp_freq = 10 + 0 = 10Hz
    auto out = tone.Process(0.5f);
    float expected_lp_alpha = 1.0f - expf(-6.28318530717958647692f * 18200.0f / sr);
    float expected_hp_alpha = 1.0f - expf(-6.28318530717958647692f * 10.0f / sr);

    assert(std::fabs(out.lp - expected_lp_alpha) < 0.001f);
    assert(std::fabs(out.hp - expected_hp_alpha) < 0.001f);

    // 2. Fully Counter-Clockwise (0.0)
    // lp_param = 0.0, hp_param = 0.0
    // lp_freq = 200Hz, hp_freq = 10Hz
    out = tone.Process(0.0f);
    expected_lp_alpha = 1.0f - expf(-6.28318530717958647692f * 200.0f / sr);
    expected_hp_alpha = 1.0f - expf(-6.28318530717958647692f * 10.0f / sr);
    assert(std::fabs(out.lp - expected_lp_alpha) < 0.001f);
    assert(std::fabs(out.hp - expected_hp_alpha) < 0.001f);

    // 3. Fully Clockwise (1.0)
    // lp_param = 1.0, hp_param = 1.0
    // lp_freq = 18200Hz, hp_freq = 10 + 3000 = 3010Hz
    out = tone.Process(1.0f);
    expected_lp_alpha = 1.0f - expf(-6.28318530717958647692f * 18200.0f / sr);
    expected_hp_alpha = 1.0f - expf(-6.28318530717958647692f * 3010.0f / sr);
    assert(std::fabs(out.lp - expected_lp_alpha) < 0.001f);
    assert(std::fabs(out.hp - expected_hp_alpha) < 0.001f);

    std::cout << "Passed." << std::endl;
}

void test_tone_filter_stateful() {
    std::cout << "Testing Tone class stateful filtering... ";
    Tone tone;
    tone.Init(48000.0f);

    Tone::Output coeffs;
    coeffs.lp = 0.1f;
    coeffs.hp = 0.0f; // HP bypassed

    // 1. Test LP response to step input (asymptotic rise)
    float out = tone.Filter(1.0f, coeffs);
    assert(std::fabs(out - 0.1f) < 0.001f);
    out = tone.Filter(1.0f, coeffs);
    assert(std::fabs(out - 0.19f) < 0.001f);

    // 2. Test Reset (clears internal state)
    tone.Reset();
    out = tone.Filter(1.0f, coeffs);
    assert(std::fabs(out - 0.1f) < 0.001f);

    // 3. Test HP DC Rejection (asymptotic decay to zero)
    tone.Reset();
    coeffs.lp = 1.0f; // LP open
    coeffs.hp = 0.1f; // HP active

    // First sample: hp_state becomes 0.1, hp_out is 0.9, lp_state is 0.9
    out = tone.Filter(1.0f, coeffs);
    assert(std::fabs(out - 0.9f) < 0.001f);

    // HP output should decay towards 0 for constant DC input
    for (int i = 0; i < 100; i++) {
        out = tone.Filter(1.0f, coeffs);
    }
    assert(out < 0.1f);

    std::cout << "Passed." << std::endl;
}
#endif
