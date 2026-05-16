#pragma once
#ifndef GD_ENGINE_TEST_H
#define GD_ENGINE_TEST_H

#include "engine.h"
#include <cassert>
#include <cmath>
#include <iostream>

// For DelayLines used by Engine
#include "daisysp.h"
#include "dsp/window.h"

using daisysp::DelayLine;

using namespace dsp;

void test_engine_end_to_end_signal_flow() {
    std::cout << "Testing Engine end-to-end signal flow... ";

    constexpr size_t kMaxDelay = 48000;
    constexpr size_t kGrainSize = 16;
    constexpr float kSampleRate = 48000.0f;
    constexpr float kControlRate = 1000.0f;

    Engine<kMaxDelay, kGrainSize> engine;
    DelayLine<float, kMaxDelay> buf_L, buf_R;
    WindowBank wint;

    // Initialize and fill buffers with a constant signal
    buf_L.Init();
    buf_R.Init();
    for (size_t i = 0; i < kMaxDelay; ++i) {
        buf_L.Write(1.0f);
        buf_R.Write(1.0f);
    }

    wint.Init();

    // Configure and initialize the engine
    EngineConfig<kMaxDelay> engine_cfg;
    engine_cfg.sample_rate = kSampleRate;
    engine_cfg.control_rate = kControlRate;
    engine_cfg.L = &buf_L;
    engine_cfg.R = &buf_R;
    engine_cfg.window_bank = &wint;
    engine.Init(engine_cfg);

    // 1. Set up parameters for a predictable trigger
    RawEngineParams raw_params = {};
    raw_params.scheduler.rhythm = SchedulerRhythm::FREE;
    raw_params.scheduler.manual_freq = 1.0f; // Corresponds to 100Hz
    raw_params.router.mix = 1.0f;            // 100% wet

    // Call UpdateParams repeatedly to allow the smoothed density parameter to
    // settle at its target value (100Hz), bypassing the smoothing delay for
    // a predictable test.
    for (int i = 0; i < 500; ++i) {
        engine.UpdateParams(raw_params);
    }

    // 2. Process audio until just before the trigger
    // With SR=48000 and density=~100Hz, a trigger should happen on sample 481.
    EngineOutput out;
    for (size_t i = 0; i < 480; ++i) {
        out = engine.Process({0.f, 0.f});
        assert(out.L == 0.0f);
        assert(out.R == 0.0f);
    }

    // 3. This process call (sample 481) should cause the scheduler to trigger a grain.
    out = engine.Process({0.f, 0.f}); // Output is 0 because window starts at 0.

    // 4. The grain is active, but its window value is 0 on the first sample
    //    (using the default SMOOTH window). Process one more sample (482) to
    //    get a non-zero window value.
    out = engine.Process({0.f, 0.f}); // Output is now non-zero.

    // 5. Verify that the triggered grain produced audio output.
    assert(out.L != 0.0f);
    assert(out.R != 0.0f);

    std::cout << "Passed." << std::endl;
}

void test_engine_reset() {
    std::cout << "Testing Engine reset... ";

    constexpr size_t kMaxDelay = 48000;
    constexpr size_t kGrainSize = 16;
    constexpr float kSampleRate = 48000.0f;
    constexpr float kControlRate = 1000.0f;

    Engine<kMaxDelay, kGrainSize> engine;
    DelayLine<float, kMaxDelay> buf_L, buf_R;
    WindowBank wint;

    // Initialize and fill buffers with a constant signal
    buf_L.Init();
    buf_R.Init();
    for (size_t i = 0; i < kMaxDelay; ++i) {
        buf_L.Write(1.0f);
        buf_R.Write(1.0f);
    }

    wint.Init();

    // Configure and initialize the engine
    EngineConfig<kMaxDelay> engine_cfg;
    engine_cfg.sample_rate = kSampleRate;
    engine_cfg.control_rate = kControlRate;
    engine_cfg.L = &buf_L;
    engine_cfg.R = &buf_R;
    engine_cfg.window_bank = &wint;
    engine.Init(engine_cfg);

    // 1. Set up parameters for a predictable trigger
    RawEngineParams raw_params = {};
    raw_params.scheduler.rhythm = SchedulerRhythm::FREE;
    raw_params.scheduler.manual_freq = 1.0f; // Corresponds to 100Hz
    raw_params.router.mix = 1.0f;            // 100% wet
    for (int i = 0; i < 500; ++i) {
        engine.UpdateParams(raw_params);
    }

    // 2. Process audio until a grain is active and producing sound
    // With SR=48000 and density=~100Hz, a trigger should happen on sample 481.
    for (size_t i = 0; i < 482; ++i) {
        engine.Process({0.f, 0.f});
    }
    assert(engine.GetActiveGrainCount() > 0);
    EngineOutput out = engine.Process({0.f, 0.f});
    assert(out.L != 0.0f || out.R != 0.0f); // Should be producing sound

    // 3. Call Reset
    engine.Reset();

    // 4. Verify that the engine is now in a reset state
    assert(engine.GetActiveGrainCount() == 0);
    out = engine.Process({0.f, 0.f});
    assert(out.L == 0.0f && out.R == 0.0f);

    std::cout << "Passed." << std::endl;
}

#endif // GD_ENGINE_TEST_H
