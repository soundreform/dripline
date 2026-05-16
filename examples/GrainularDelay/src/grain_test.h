#include "daisysp.h"
#include "frng.h"
#include "grain.h"
#include <cassert>
#include <cmath>
#include <iostream>

using daisysp::DelayLine;
using dripline::FastRandom;

void test_window_interpolation() {
    std::cout << "Testing Window Interpolation... ";

    float table[] = {0.0f, 1.0f, 2.0f}; // A size 2 table (3 points total)
    GrainWindow win = {2, table};

    // Check exact indices
    assert(std::fabs(win.Read(0.0f) - 0.0f) < 0.001f);
    assert(std::fabs(win.Read(0.5f) - 1.0f) < 0.001f);
    assert(std::fabs(win.Read(1.0f) - 2.0f) < 0.001f);

    // Check linear interpolation midpoints
    assert(std::fabs(win.Read(0.25f) - 0.5f) < 0.001f);
    assert(std::fabs(win.Read(0.75f) - 1.5f) < 0.001f);

    std::cout << "Passed." << std::endl;
}

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
    GrainWindow win = {1, window_table};

    GrainParams p;
    p.duration = 10.0f; // Lasts 10 samples
    p.pan = 0.0f;       // Hard left (pan_L = 1.0, pan_R = 0.0)
    p.pitch = 1.0f;
    p.offset = 0.0f;
    p.window = win;

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
    manager.Init(&bufL, &bufR, &rng);

    float window_table[] = {1.0f, 1.0f};
    GrainWindow win = {1, window_table};

    GrainManagerParams p;
    p.duration = 100.0f;
    p.delay_min = 10.0f;
    p.pan_spread = 0.0f; // All grains at center (pan=0.5)
    p.time_spray = 0.0f;
    p.window = win;

    // Trigger 2 grains via Process
    auto out = manager.Process(p, 2);

    // Sum should be 2 * (buffer(1.0) * window(1.0) * pan_L(cos(0.5 * PI/2))) * 0.25 compensation
    // cos(0.785398) is approx 0.7071, result = 2 * 0.7071 * 0.25 = 0.35355
    assert(std::fabs(out.L - 0.35355f) < 0.001f);
    assert(std::fabs(out.R - 0.35355f) < 0.001f);

    // Trigger more to fill the pool
    // Pool size is 4. We have 2 active, so this triggers 2 more and ignores the 5th.
    out = manager.Process(p, 3);

    // 4 active grains * 0.7071 * 0.25 compensation = 0.7071
    assert(std::fabs(out.L - 0.7071f) < 0.001f);

    std::cout << "Passed." << std::endl;
}

void test_grain_manager_randomization() {
    std::cout << "Testing GrainManager Randomization (Spray/Spread)... ";

    static constexpr size_t kMaxDelay = 1024;
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
    manager.Init(&bufL, &bufR, &rng);

    float window_table[] = {1.0f, 1.0f};
    GrainWindow win = {1, window_table};

    GrainManagerParams p;
    p.duration = 100.0f;
    p.delay_min = 10.0f;
    p.pan_spread = 1.0f; // Full randomization
    p.time_spray = 50.0f;
    p.window = win;

    // Trigger two grains and capture their outputs
    // Because pan and offset are randomized, the L/R ratio and values should differ
    manager.Process(p, 1);
    // We can't easily peek inside, but we can verify the sum isn't perfectly
    // matched to a specific fixed pan if we trigger them separately.
    auto out1 = manager.Process(p, 0); // Process existing

    manager.Process(p, 1); // Trigger second
    auto out2 = manager.Process(p, 0);

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
    GrainWindow win = {1, window_table};
    GrainParams p;
    p.duration = 10.0f;
    p.pan = 0.5f;
    p.pitch = 1.0f;
    p.offset = 0.0f;
    p.window = win;

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
    manager.Init(&bufL, &bufR, &rng);

    float window_table[] = {1.0f, 1.0f};
    GrainWindow win = {1, window_table};
    GrainManagerParams p;
    p.duration = 10.0f;
    p.delay_min = 0.0f;
    p.pan_spread = 0.0f;
    p.time_spray = 0.0f;
    p.window = win;

    manager.Process(p, 2);
    manager.Reset();
    auto out = manager.Process(p, 0);
    assert(out.L == 0.0f && out.R == 0.0f);

    std::cout << "Passed." << std::endl;
}

void test_grain_manager_safety_clamping() {
    std::cout << "Testing GrainManager Safety Clamping (Section 5)... ";

    static constexpr size_t kMaxDelay = 1024;
    GrainManager<kMaxDelay, 2> manager;
    DelayLine<float, kMaxDelay> bufL, bufR;
    bufL.Init();
    bufR.Init();

    // Fill buffers with a ramp where Read(D) returns D
    // To achieve this: buffer[i] = WriteHeadPos - i
    for (int i = 0; i < kMaxDelay; i++) {
        bufL.Write((float)kMaxDelay - (float)i);
        bufR.Write((float)kMaxDelay - (float)i);
    }

    FastRandom rng;
    rng.Init(123);
    manager.Init(&bufL, &bufR, &rng);

    float window_table[] = {1.0f, 1.0f}; // Flat window
    GrainWindow win = {1, window_table};

    GrainManagerParams p;
    p.duration = 100.0f;
    p.pan_spread = 0.0f; // Center
    p.time_spray = 0.0f; // Exact offset testing
    p.pitch = 1.0f;
    p.pitch_spray = 0.0f;
    p.window = win;
    p.quantize_mode = QuantizeMode::FREE;

    // 1. Test Min Clamp: Request delay_min = 0
    // Expected min_safe = (100 * 1.0) + 0 + 100 (padding) = 200
    p.delay_min = 0.0f;
    auto out = manager.Process(p, 1);
    // Result = 200 * 0.7071 (pan) * 0.25 (comp) = 35.355
    assert(std::fabs(out.L - 35.355f) < 0.1f);

    // 2. Test Max Clamp: Request delay_min = 1024 (past buffer)
    // Expected max_safe = 1024 - 100 (dur) - 0 (spray) - 100 (padding) = 824
    manager.Reset();
    p.delay_min = 1024.0f;
    out = manager.Process(p, 1);
    // Result = 824 * 0.7071 * 0.25 = 145.66
    assert(std::fabs(out.L - 145.66f) < 0.1f);

    // 3. Test Pitch Factor scaling
    // If pitch is 2.0, min_safe = (100 * 2.0) + 0 + 100 = 300
    manager.Reset();
    p.delay_min = 0.0f;
    p.pitch = 2.0f;
    out = manager.Process(p, 1);
    // Result = 300 * 0.7071 * 0.25 = 53.03
    assert(std::fabs(out.L - 53.03f) < 0.1f);

    std::cout << "Passed." << std::endl;
}
