#pragma once
#ifndef GD_GRAIN_TEST_H
#define GD_GRAIN_TEST_H

#include "daisysp.h"
#include "dripline/frng.h"
#include "grain.h"
#include <cassert>
#include <cmath>
#include <iostream>

using daisysp::DelayLine;
using dripline::FastRandom;

using namespace dsp;

void test_grain_lifecycle() {
    std::cout << "Testing Grain Lifecycle and Panning... ";

    Grain<1024> grain;
    DelayLine<float, 1024> bufL, bufR;

    // Initialize buffers to 1.0f so audio math returns predictable results
    bufL.Init();
    bufR.Init();
    for (int i = 0; i < 1024; i++) {
        bufL.Write(1.0f);
        bufR.Write(1.0f);
    }

    float window_table[] = {1.0f, 1.0f}; // Flat window
    Window win = {1, window_table};

    GrainParams p;
    p.duration = 10.0f; // Lasts 10 samples
    p.pan = 0.0f;       // Hard left (pan_L = 1.0, pan_R = 0.0)
    p.pitch = 1.0f;
    p.offset = 0.0f;
    p.window = &win;

    // 1. Verify inactive state returns silence
    auto out = grain.Process(&bufL, &bufR);
    assert(out.L == 0.0f && out.R == 0.0f);

    // 2. Trigger and verify sample output math
    grain.Trigger(p);
    out = grain.Process(&bufL, &bufR);

    // Expected: buffer(1.0) * window(1.0) * pan_L(1.0)
    assert(std::fabs(out.L - 1.0f) < 0.001f);
    assert(std::fabs(out.R - 0.0f) < 0.001f);

    // 3. Verify completion
    // Process 8 more samples (9 total processed so far)
    for (int i = 0; i < 8; i++) {
        grain.Process(&bufL, &bufR);
    }
    assert(grain.IsActive());
    grain.Process(&bufL, &bufR); // 10th sample
    assert(!grain.IsActive());

    std::cout << "Passed." << std::endl;
}

void test_grain_manager_polyphony() {
    std::cout << "Testing GrainManager Polyphony and Summing... ";

    static constexpr size_t kMaxDelay = 1024;
    static constexpr size_t kPoolSize = 4;
    static constexpr float kSampleRate = 48000.f;

    GrainManager<kMaxDelay, kPoolSize> manager;
    DelayLine<float, kMaxDelay> bufL, bufR;

    bufL.Init();
    bufR.Init();
    for (int i = 0; i < kMaxDelay; i++) {
        bufL.Write(1.0f);
        bufR.Write(1.0f);
    }

    FastRandom rng;
    rng.Init(123);
    WindowBank window_bank;
    window_bank.Init();
    GrainManagerConfig<kMaxDelay> mgr_cfg;
    mgr_cfg.buffer_L = &bufL;
    mgr_cfg.buffer_R = &bufR;
    mgr_cfg.rng = &rng;
    mgr_cfg.delay_min = 10.0f;
    mgr_cfg.window_bank = &window_bank;
    mgr_cfg.sample_rate = kSampleRate;
    mgr_cfg.smoothing_coeff = 1.f;
    manager.Init(mgr_cfg);

    RawGrainManagerParams raw_p = {};
    raw_p.duration = 0.f; // Value doesn't affect test, use min.
    raw_p.pan_spread = 0.0f;
    raw_p.time_spray = 0.0f;
    raw_p.window = WindowShape::SWELL;
    manager.UpdateParams(raw_p);

    // Trigger 2 grains via Process
    auto out = manager.Process(2);

    // Sum should be 2 * (buffer(1.0) * window(1.0) * pan_L(cos(0.5 * PI/2))) * 0.25 compensation
    // cos(0.785398) is approx 0.7071, result = 2 * 0.7071 * 0.25 = 0.35355
    // With SWELL window at phase 0, value is expf(-5.0f) ~= 0.006738
    // New expected value: 0.35355 * 0.006738 = 0.002382
    assert(std::fabs(out.L - 0.002382f) < 0.0001f);
    assert(std::fabs(out.R - 0.002382f) < 0.0001f);

    // Trigger more to fill the pool
    // Pool size is 4. We have 2 active, so this triggers 2 more and ignores the 5th.
    out = manager.Process(3);

    // 4 active grains * window * 0.7071 * 0.25 compensation = 0.7071 * window
    assert(std::fabs(out.L - 0.004764f) < 0.0001f);

    std::cout << "Passed." << std::endl;
}

void test_grain_manager_randomization() {
    std::cout << "Testing GrainManager Randomization (Spray/Spread)... ";

    static constexpr size_t kMaxDelay = 1024;
    static constexpr float kSampleRate = 48000.f;
    GrainManager<kMaxDelay, 2> manager;
    DelayLine<float, kMaxDelay> bufL, bufR;
    bufL.Init();
    bufR.Init();
    for (int i = 0; i < kMaxDelay; i++) {
        bufL.Write(1.0f);
        bufR.Write(1.0f);
    }

    FastRandom rng;
    rng.Init(456);
    WindowBank window_bank;
    window_bank.Init();
    GrainManagerConfig<kMaxDelay> mgr_cfg;
    mgr_cfg.buffer_L = &bufL;
    mgr_cfg.buffer_R = &bufR;
    mgr_cfg.rng = &rng;
    mgr_cfg.delay_min = 10.0f;
    mgr_cfg.window_bank = &window_bank;
    mgr_cfg.sample_rate = kSampleRate;
    mgr_cfg.smoothing_coeff = 1.f;
    manager.Init(mgr_cfg);

    RawGrainManagerParams raw_p = {};
    raw_p.duration = 0.f;
    raw_p.pan_spread = 1.0f;
    raw_p.time_spray = 50.0f / (kSampleRate * 0.5f);
    raw_p.window = WindowShape::SMOOTH;
    manager.UpdateParams(raw_p);

    // Trigger two grains and capture their outputs
    // Because pan and offset are randomized, the L/R ratio and values should differ
    manager.Process(1);
    // We can't easily peek inside, but we can verify the sum isn't perfectly
    // matched to a specific fixed pan if we trigger them separately.
    auto out1 = manager.Process(0); // Process existing

    manager.Process(1); // Trigger second
    auto out2 = manager.Process(0);

    // Highly unlikely to be identical with spray/spread active
    assert(out1.L != out2.L || out1.R != out2.R);

    std::cout << "Passed" << std::endl;
}

void test_grain_reset() {
    std::cout << "Testing Grain reset... ";
    Grain<1024> grain;
    DelayLine<float, 1024> bufL, bufR;
    bufL.Init();
    bufR.Init();
    for (int i = 0; i < 1024; i++) {
        bufL.Write(1.0f);
        bufR.Write(1.0f);
    }

    float window_table[] = {1.0f, 1.0f};
    Window win = {1, window_table};
    GrainParams p;
    p.duration = 10.0f;
    p.pan = 0.5f;
    p.pitch = 1.0f;
    p.offset = 0.0f;
    p.window = &win;

    grain.Trigger(p);
    assert(grain.IsActive());

    grain.Reset();
    assert(!grain.IsActive());
    auto out = grain.Process(&bufL, &bufR);
    assert(out.L == 0.0f && out.R == 0.0f);

    std::cout << "Passed." << std::endl;
}

void test_grain_manager_reset() {
    std::cout << "Testing GrainManager reset... ";
    GrainManager<1024, 4> manager;
    DelayLine<float, 1024> bufL, bufR;
    bufL.Init();
    bufR.Init();
    FastRandom rng;
    rng.Init(456);
    WindowBank window_bank;
    window_bank.Init();
    GrainManagerConfig<1024> mgr_cfg;
    mgr_cfg.buffer_L = &bufL;
    mgr_cfg.buffer_R = &bufR;
    mgr_cfg.rng = &rng;
    mgr_cfg.window_bank = &window_bank;
    mgr_cfg.delay_min = 0.0f;
    mgr_cfg.sample_rate = 48000.f;
    mgr_cfg.smoothing_coeff = 1.f;
    manager.Init(mgr_cfg);

    RawGrainManagerParams raw_p = {};
    raw_p.duration = 0.f;
    raw_p.pan_spread = 0.0f;
    raw_p.time_spray = 0.0f;
    manager.UpdateParams(raw_p);

    manager.Process(2);
    manager.Reset();
    auto out = manager.Process(0);
    assert(out.L == 0.0f && out.R == 0.0f);

    std::cout << "Passed." << std::endl;
}

void test_grain_manager_trigger_smoke() {
    std::cout << "Testing GrainManager trigger wiring (smoke test)... ";

    static constexpr size_t kMaxDelay = 1024;
    static constexpr size_t kPoolSize = 4;
    static constexpr float kSampleRate = 48000.f;

    GrainManager<kMaxDelay, kPoolSize> manager;
    DelayLine<float, kMaxDelay> bufL, bufR;
    FastRandom rng;
    rng.Init(123);
    WindowBank window_bank;
    window_bank.Init();

    GrainManagerConfig<kMaxDelay> mgr_cfg;
    mgr_cfg.buffer_L = &bufL;
    mgr_cfg.buffer_R = &bufR;
    mgr_cfg.rng = &rng;
    mgr_cfg.delay_min = 10.0f;
    mgr_cfg.window_bank = &window_bank;
    mgr_cfg.sample_rate = kSampleRate;
    mgr_cfg.smoothing_coeff = 1.f;
    manager.Init(mgr_cfg);

    // 1. Check initial state: no active grains
    assert(manager.GetActiveGrainCount() == 0);

    // 2. Trigger one grain
    manager.Process(1);

    // 3. Verify one grain is now active
    assert(manager.GetActiveGrainCount() == 1);

    // 4. Process with no new triggers, should still be active (for a while)
    manager.Process(0);
    assert(manager.GetActiveGrainCount() == 1);

    // 5. Fill the pool
    manager.Process(kPoolSize); // triggers 3 more
    assert(manager.GetActiveGrainCount() == kPoolSize);

    // 6. Over-trigger, should still be full
    manager.Process(1);
    assert(manager.GetActiveGrainCount() == kPoolSize);

    std::cout << "Passed." << std::endl;
}

void test_grain_param_helpers() {
    std::cout << "Testing grain param helper functions... ";

    // CalculatePan
    assert(std::fabs(CalculatePan(0.5f, 0.0f) - 0.5f) < 0.001f); // no spread
    assert(std::fabs(CalculatePan(0.0f, 1.0f) - 0.0f) < 0.001f); // full spread, min
    assert(std::fabs(CalculatePan(1.0f, 1.0f) - 1.0f) < 0.001f); // full spread, max

    // CalculatePitch
    assert(std::fabs(CalculatePitch(0.5f, 1.5f, 0.0f) - 1.5f) < 0.001f); // no spray
    assert(std::fabs(CalculatePitch(0.0f, 1.0f, 1.0f) - 0.0f) < 0.001f); // full spray, min
    assert(std::fabs(CalculatePitch(1.0f, 1.0f, 1.0f) - 2.0f) < 0.001f); // full spray, max

    // RedirectPitch
    assert(std::fabs(RedirectPitch(0.7f, 1.5f, GrainDirection::FORWARD) - 1.5f) < 0.001f);
    assert(std::fabs(RedirectPitch(0.7f, 1.5f, GrainDirection::REVERSE) - (-1.5f)) < 0.001f);
    assert(std::fabs(RedirectPitch(0.6f, 1.5f, GrainDirection::RANDOM) - (-1.5f)) < 0.001f); // rand > 0.5

    // CalculateOffset
    assert(std::fabs(CalculateOffset(0.5f, 10.f, 100.f) - 60.f) < 0.001f);

    std::cout << "Passed." << std::endl;
}

void test_clamp_offset() {
    std::cout << "Testing ClampOffset... ";

    constexpr size_t kMaxDelay = 2048;
    constexpr float kDuration = 500.0f;
    constexpr float kPadding = 100.0f;

    // 1. Forward pitch, target is within safe range
    float offset = ClampOffset(500.0f, kDuration, 1.0f, kPadding, kMaxDelay);
    // read_length=500. min=100, max=2048-500-100=1448. target=500.
    assert(std::fabs(offset - 500.0f) < 0.001f);

    // 2. Forward pitch, target is below safe range
    offset = ClampOffset(50.0f, kDuration, 1.0f, kPadding, kMaxDelay);
    assert(std::fabs(offset - 100.0f) < 0.001f);

    // 3. Reverse pitch, target is below safe range
    offset = ClampOffset(1000.0f, kDuration, -2.0f, kPadding, kMaxDelay);
    // read_length=1000. min=1100, max=1948. target=1000.
    assert(std::fabs(offset - 1100.0f) < 0.001f);

    // 4. Forward pitch, very long duration (edge case where max < min)
    offset = ClampOffset(500.0f, 2000.0f, 1.0f, kPadding, kMaxDelay);
    // read_length=2000. min=100, max=2048-2000-100=-52. Clamped to min.
    assert(std::fabs(offset - 100.0f) < 0.001f);

    std::cout << "Passed." << std::endl;
}

class GrainManagerTester {
  public:
    static void test_modulation() {
        std::cout << "Testing GrainManager modulation... ";

        // 1. Setup
        constexpr size_t kMaxDelay = 4096;
        constexpr size_t kPoolSize = 2;
        constexpr float kSampleRate = 48000.f;

        GrainManager<kMaxDelay, kPoolSize> manager;
        DelayLine<float, kMaxDelay> bufL, bufR;
        bufL.Init();
        bufR.Init();

        FastRandom rng;
        WindowBank window_bank;
        window_bank.Init();

        GrainManagerConfig<kMaxDelay> mgr_cfg;
        mgr_cfg.buffer_L = &bufL;
        mgr_cfg.buffer_R = &bufR;
        mgr_cfg.rng = &rng;
        mgr_cfg.delay_min = 10.0f;
        mgr_cfg.window_bank = &window_bank;
        mgr_cfg.sample_rate = kSampleRate;
        mgr_cfg.smoothing_coeff = 1.f;
        manager.Init(mgr_cfg);

        RawGrainManagerParams raw_p = {};
        const float base_pitch = 1.5f;
        raw_p.pitch = base_pitch;
        raw_p.pitch_spray = 0.2f;
        raw_p.time_spray = 100.f / (kSampleRate * 0.5f);
        raw_p.duration = 0.01f;
        manager.UpdateParams(raw_p);

        // 2. Get params with no modulation by calling private BuildGrainParams
        manager.rng_->Init(12345);
        GrainParams params_no_mod = manager.BuildGrainParams(0.0f);

        // 3. Get params with full modulation
        manager.rng_->Init(12345);
        GrainParams params_full_mod = manager.BuildGrainParams(1.0f);

        // 4. Assertions
        float pitch_deviation1 = params_no_mod.pitch - base_pitch;
        float pitch_deviation2 = params_full_mod.pitch - base_pitch;
        assert(std::fabs(pitch_deviation2 - (2.0f * pitch_deviation1)) < 0.0001f);

        float offset_random_part1 = params_no_mod.offset - mgr_cfg.delay_min;
        float offset_random_part2 = params_full_mod.offset - mgr_cfg.delay_min;
        assert(offset_random_part1 > 0.f);
        assert(std::fabs(offset_random_part2 - (2.0f * offset_random_part1)) < 0.0001f);

        std::cout << "Passed." << std::endl;
    }
};

#endif
