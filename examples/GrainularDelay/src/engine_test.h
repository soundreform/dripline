#pragma once
#ifndef GD_ENGINE_TEST_H
#define GD_ENGINE_TEST_H

#include "engine.h"
#include <cassert>
#include <cmath>
#include <iostream>

void test_engine_parameter_smoothing() {
    std::cout << "Testing Engine parameter smoothing... ";

    daisysp::DelayLine<float, 1024> l, r;
    Engine<1024, 2> engine;
    engine.Init(48000.0f, &l, &r);

    EngineParams targets = engine.GetParams();

    // 1. Test Smoothing (kAlpha = 0.05f)
    // Initial pitch is 1.0f. Set target to 2.0f.
    targets.grain.pitch = 2.0f;
    engine.UpdateParameters(targets);

    // Expected after 1 block: current + 0.05 * (2.0 - 1.0) = 1.05
    assert(std::fabs(engine.GetParams().grain.pitch - 1.05f) < 0.0001f);

    // 2. Test Convergence
    for (int i = 0; i < 500; i++) {
        engine.UpdateParameters(targets);
    }
    assert(std::fabs(engine.GetParams().grain.pitch - 2.0f) < 0.001f);

    // 3. Test Deadbanding (change < 0.001f should be ignored)
    float current_pitch = engine.GetParams().grain.pitch;
    targets.grain.pitch = current_pitch + 0.0005f;
    engine.UpdateParameters(targets);
    assert(engine.GetParams().grain.pitch == current_pitch);

    std::cout << "Passed." << std::endl;
}

void test_engine_immediate_updates() {
    std::cout << "Testing Engine immediate parameter updates... ";

    daisysp::DelayLine<float, 1024> l, r;
    Engine<1024, 2> engine;
    engine.Init(48000.0f, &l, &r);

    EngineParams targets = engine.GetParams();
    targets.router.mode = RouterMode::SERIES;
    targets.grain.quantize_mode = QuantizeMode::OCTAVES_FIFTHS;

    engine.UpdateParameters(targets);

    assert(engine.GetParams().router.mode == RouterMode::SERIES);
    assert(engine.GetParams().grain.quantize_mode == QuantizeMode::OCTAVES_FIFTHS);

    std::cout << "Passed." << std::endl;
}

void test_engine_tone_mapping() {
    std::cout << "Testing Engine tone mapping orchestration... ";

    daisysp::DelayLine<float, 1024> l, r;
    Engine<1024, 2> engine;
    const float sr = 48000.0f;
    engine.Init(sr, &l, &r);

    EngineParams targets = engine.GetParams();

    // 1. Test Low-pass region (tone = 0.25)
    // Expected freq = 200 + (0.5^2 * 18000) = 4700Hz
    targets.router.tone = 0.25f;
    for (int i = 0; i < 500; i++) {
        engine.UpdateParameters(targets);
    }

    Tone tone_mapper;
    tone_mapper.Init(sr);
    auto coeffs = tone_mapper.Process(engine.GetParams().router.tone);
    float expected_lp_alpha = 1.0f - expf(-6.28318530717958647692f * 4700.0f / sr);
    assert(std::fabs(coeffs.lp - expected_lp_alpha) < 0.005f);

    // 2. Test High-pass region (tone = 0.75)
    // Expected freq = 10 + (0.5^2 * 3000) = 760Hz
    targets.router.tone = 0.75f;
    for (int i = 0; i < 500; i++) {
        engine.UpdateParameters(targets);
    }

    coeffs = tone_mapper.Process(engine.GetParams().router.tone);
    float expected_hp_alpha = 1.0f - expf(-6.28318530717958647692f * 760.0f / sr);
    assert(std::fabs(coeffs.hp - expected_hp_alpha) < 0.005f);

    std::cout << "Passed." << std::endl;
}
#endif
