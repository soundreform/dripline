#pragma once
#ifndef GD_ROUTER_TEST_H
#define GD_ROUTER_TEST_H

#include "router.h"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace dsp;
using daisysp::DelayLine;

void test_router_routing_logic() {
    std::cout << "Testing Router routing logic... ";
    Router<1024> router;
    DelayLine<float, 1024> bufL, bufR;
    bufL.Init();
    bufR.Init();
    RouterConfig<1024> router_cfg;
    router_cfg.sample_rate = 48000.0f;
    router_cfg.buffer_L = &bufL;
    router_cfg.buffer_R = &bufR;
    router_cfg.smoothing_coeff = 1.0f; // No smoothing for tests
    router.Init(router_cfg);

    RawRouterParams raw_p;
    raw_p.feedback = 0.5f / 0.85f; // 0.5 value
    raw_p.tone = 0.5f;             // Neutral position
    raw_p.mix = 1.0f;              // All wet
    raw_p.freeze = false;
    raw_p.drive = 0.0f; // 1.0 value
    raw_p.output = 1.0f;
    raw_p.bypass = false;

    float dry_L = 1.0f, dry_R = 0.5f, wet_L = 0.2f, wet_R = 0.1f;

    // Parallel: In_L = dL + wL*fb = 1.0 + 0.2*0.5 = 1.1
    router.Reset(); // Reset filter states for a clean test
    raw_p.mode = RouterMode::PARALLEL;
    router.UpdateParams(raw_p);
    router.Process({dry_L, dry_R, wet_L, wet_R});
    // With tone=0.5 (neutral), filter is bypassed. Value is Saturate(1.1 * drive=1.0) = 0.5238
    assert(std::fabs(bufL.Read(1.0f) - 0.5238f) < 0.01f);
    // Value is Saturate((0.5 + 0.1*0.5) * drive=1.0) = Saturate(0.55) = 0.3548
    assert(std::fabs(bufR.Read(1.0f) - 0.3548f) < 0.01f);

    // Ping Pong: In_L = dL + wR*fb = 1.0 + 0.1*0.5 = 1.05
    router.Reset(); // Reset filter states for a clean test
    raw_p.mode = RouterMode::PING_PONG;
    router.UpdateParams(raw_p);
    router.Process({dry_L, dry_R, wet_L, wet_R});
    // Value is Saturate(1.05 * drive=1.0) = 0.5122
    assert(std::fabs(bufL.Read(1.0f) - 0.5122f) < 0.01f);
    // Value is Saturate((0.5 + 0.2*0.5) * drive=1.0) = Saturate(0.6) = 0.375
    assert(std::fabs(bufR.Read(1.0f) - 0.375f) < 0.01f);

    std::cout << "Passed." << std::endl;
}

void test_router_mixer() {
    std::cout << "Testing Router mixing logic... ";
    Router<1024> router;
    DelayLine<float, 1024> bufL, bufR;
    bufL.Init();
    bufR.Init();
    RouterConfig<1024> router_cfg;
    router_cfg.sample_rate = 48000.0f;
    router_cfg.buffer_L = &bufL;
    router_cfg.buffer_R = &bufR;
    router_cfg.smoothing_coeff = 1.0f; // No smoothing for tests
    router.Init(router_cfg);

    RawRouterParams raw_p;
    raw_p.feedback = 0.0f;
    raw_p.tone = 0.5f;
    raw_p.freeze = false;
    raw_p.drive = 0.0f; // for 1.0f value
    raw_p.output = 1.0f;
    raw_p.bypass = false;
    raw_p.mode = RouterMode::PARALLEL;

    float dry_L = 1.0f, dry_R = 0.0f, wet_L = 0.0f, wet_R = 1.0f;

    // Mix = 0.0 (Dry)
    raw_p.mix = 0.0f;
    router.UpdateParams(raw_p);
    auto out = router.Process({dry_L, dry_R, wet_L, wet_R});
    assert(std::fabs(out.L - 0.7616f) < 0.001f);
    assert(std::fabs(out.R - 0.0f) < 0.001f);

    // Mix = 1.0 (Wet)
    raw_p.mix = 1.0f;
    router.UpdateParams(raw_p);
    out = router.Process({dry_L, dry_R, wet_L, wet_R});
    assert(std::fabs(out.L - 0.0f) < 0.001f);
    assert(std::fabs(out.R - 0.7616f) < 0.001f);

    std::cout << "Passed." << std::endl;
}

void test_router_freeze() {
    std::cout << "Testing Router freeze behavior... ";
    Router<1024> router;
    DelayLine<float, 1024> bufL, bufR;
    bufL.Init();
    bufR.Init();
    RouterConfig<1024> router_cfg;
    router_cfg.sample_rate = 48000.0f;
    router_cfg.buffer_L = &bufL;
    router_cfg.buffer_R = &bufR;
    router_cfg.smoothing_coeff = 1.0f; // No smoothing for tests
    router.Init(router_cfg);

    bufL.Write(0.5f);
    bufR.Write(0.5f);

    RawRouterParams raw_p;
    raw_p.mix = 0.0f;
    raw_p.tone = 0.5f;
    raw_p.freeze = true;
    raw_p.feedback = 0.0f;
    raw_p.drive = 0.0f; // for 1.0f value
    raw_p.output = 1.0f;
    raw_p.bypass = false;
    router.UpdateParams(raw_p);

    float dry_L = 1.0f, dry_R = 1.0f, wet_L = 0.0f, wet_R = 0.0f;

    router.Process({dry_L, dry_R, wet_L, wet_R});
    assert(std::fabs(bufL.Read(1.0f) - 0.5f) < 0.001f);
    assert(std::fabs(bufR.Read(1.0f) - 0.5f) < 0.001f);

    std::cout << "Passed." << std::endl;
}

void test_router_reset() {
    std::cout << "Testing Router reset... ";
    Router<1024> router;
    DelayLine<float, 1024> bufL, bufR;
    bufL.Init();
    bufR.Init();
    RouterConfig<1024> router_cfg;
    router_cfg.sample_rate = 48000.0f;
    router_cfg.buffer_L = &bufL;
    router_cfg.buffer_R = &bufR;
    router_cfg.smoothing_coeff = 1.0f; // No smoothing for tests
    router.Init(router_cfg);

    RawRouterParams raw_p;
    raw_p.mode = RouterMode::PARALLEL;
    raw_p.feedback = 0.5f / 0.85f;
    raw_p.tone = 0.5f;
    raw_p.mix = 1.0f;
    raw_p.freeze = false;
    raw_p.drive = 0.0f; // for 1.0f value
    raw_p.output = 1.0f;
    raw_p.bypass = false;
    router.UpdateParams(raw_p);

    // 1. Process a signal to dirty the internal state
    router.Process({1.0f, 0.0f, 0.0f});

    // Verify that the buffer was written to
    assert(std::fabs(bufL.Read(1.0f)) > 0.001f);

    // 2. Perform Reset
    router.Reset();

    // 3. Verify buffers are zeroed
    assert(std::fabs(bufL.Read(1.0f)) < 0.001f);
    assert(std::fabs(bufR.Read(1.0f)) < 0.001f);

    // 4. Verify filter states are cleared by processing silence
    auto out = router.Process({0.0f, 0.0f, 0.0f, 0.0f});
    assert(std::fabs(out.L) < 0.001f);
    assert(std::fabs(out.R) < 0.001f);

    std::cout << "Passed." << std::endl;
}

void test_router_drive_saturation() {
    std::cout << "Testing Router drive saturation... ";
    Router<1024> router;
    DelayLine<float, 1024> bufL, bufR;
    bufL.Init();
    bufR.Init();
    RouterConfig<1024> router_cfg;
    router_cfg.sample_rate = 48000.0f;
    router_cfg.buffer_L = &bufL;
    router_cfg.buffer_R = &bufR;
    router_cfg.smoothing_coeff = 1.0f; // No smoothing for tests
    router.Init(router_cfg);

    RawRouterParams raw_p;
    raw_p.mode = RouterMode::PARALLEL;
    raw_p.feedback = 1.0f; // for 0.85f value
    raw_p.tone = 0.5f;     // Neutral position
    raw_p.mix = 1.0f;
    raw_p.freeze = false;
    raw_p.output = 1.0f;
    raw_p.bypass = false;

    float dry_L = 1.0f, dry_R = 1.0f, wet_L = 0.0f, wet_R = 0.0f; // Input = 1.0

    // 1. Min Drive (1.0): Result should be Saturate(1.0 * 1.0) = 0.5
    router.Reset();
    raw_p.drive = 0.0f; // Corresponds to 1.0f
    router.UpdateParams(raw_p);
    router.Process({dry_L, dry_R, wet_L, wet_R});
    float out1 = bufL.Read(1.0f);
    assert(std::fabs(out1 - 0.5f) < 0.001f);

    // 2. Max Drive (4.0): Result should be Saturate(1.0 * 4.0) = 0.8
    router.Reset();
    raw_p.drive = 1.0f; // Corresponds to 4.0f
    router.UpdateParams(raw_p);
    router.Process({dry_L, dry_R, wet_L, wet_R});
    float out4 = bufL.Read(1.0f);

    assert(out4 > out1);
    assert(std::fabs(out4 - 0.8f) < 0.001f);

    std::cout << "Passed." << std::endl;
}

#endif
