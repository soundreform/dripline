#pragma once
#ifndef GD_ROUTER_TEST_H
#define GD_ROUTER_TEST_H

#include "router.h"
#include <cassert>
#include <cmath>
#include <iostream>

using daisysp::DelayLine;

void test_router_routing_logic() {
    std::cout << "Testing Router routing logic... ";
    Router<1024> router;
    DelayLine<float, 1024> bufL, bufR;
    bufL.Init();
    bufR.Init();
    router.Init(48000.0f, &bufL, &bufR);

    RouterParams p;
    p.feedback = 0.5f;
    p.tone = 0.5f; // Neutral position
    p.mix = 1.0f;  // All wet
    p.freeze = false;
    p.drive = 1.0f;
    p.output = 1.0f;
    p.trails_bypass = false;

    RouterSignals s = {1.0f, 0.5f, 0.2f, 0.1f}; // dL, dR, wL, wR

    // Parallel: In_L = dL + wL*fb = 1.0 + 0.2*0.5 = 1.1
    router.Reset(); // Reset filter states for a clean test
    p.mode = RouterMode::PARALLEL;
    router.Process(p, s);
    // Expected engagement with tone filters at sample 0 (alphas lp~0.907, hp~0.0013)
    assert(std::fabs(bufL.Read(1.0f) - 0.499f) < 0.01f);
    assert(std::fabs(bufR.Read(1.0f) - 0.332f) < 0.01f);

    // Ping Pong: In_L = dL + wR*fb = 1.0 + 0.1*0.5 = 1.05
    router.Reset(); // Reset filter states for a clean test
    p.mode = RouterMode::PING_PONG;
    router.Process(p, s);
    assert(std::fabs(bufL.Read(1.0f) - 0.488f) < 0.01f);
    assert(std::fabs(bufR.Read(1.0f) - 0.352f) < 0.01f);

    std::cout << "Passed." << std::endl;
}

void test_router_mixer() {
    std::cout << "Testing Router mixing logic... ";
    Router<1024> router;
    DelayLine<float, 1024> bufL, bufR;
    bufL.Init();
    bufR.Init();
    router.Init(48000.0f, &bufL, &bufR);

    RouterParams p;
    p.feedback = 0.0f;
    p.tone = 0.5f;
    p.freeze = false;
    p.drive = 1.0f;
    p.output = 1.0f;
    p.trails_bypass = false;

    RouterSignals s = {1.0f, 0.0f, 0.0f, 1.0f}; // dL=1, dR=0, wL=0, wR=1

    // Mix = 0.0 (Dry)
    p.mix = 0.0f;
    auto out = router.Process(p, s);
    assert(std::fabs(out.L - 0.7616f) < 0.001f);
    assert(std::fabs(out.R - 0.0f) < 0.001f);

    // Mix = 1.0 (Wet)
    p.mix = 1.0f;
    out = router.Process(p, s);
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
    router.Init(48000.0f, &bufL, &bufR);

    bufL.Write(0.5f);
    bufR.Write(0.5f);

    RouterParams p;
    p.mix = 0.0f;
    p.tone = 0.5f;
    p.freeze = true;
    p.feedback = 0.0f;
    p.drive = 1.0f;
    p.output = 1.0f;
    p.trails_bypass = false;

    RouterSignals s = {1.0f, 1.0f, 0.0f, 0.0f};

    router.Process(p, s);
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
    router.Init(48000.0f, &bufL, &bufR);

    RouterParams p;
    p.mode = RouterMode::PARALLEL;
    p.feedback = 0.5f;
    p.tone = 0.5f;
    p.mix = 1.0f;
    p.freeze = false;
    p.drive = 1.0f;
    p.output = 1.0f;
    p.trails_bypass = false;

    // 1. Process a signal to dirty the internal state
    RouterSignals s = {1.0f, 1.0f, 0.0f, 0.0f};
    router.Process(p, s);

    // Verify that the buffer was written to
    assert(std::fabs(bufL.Read(1.0f)) > 0.001f);

    // 2. Perform Reset
    router.Reset();

    // 3. Verify buffers are zeroed
    assert(std::fabs(bufL.Read(1.0f)) < 0.001f);
    assert(std::fabs(bufR.Read(1.0f)) < 0.001f);

    // 4. Verify filter states are cleared by processing silence
    RouterSignals silence = {0.0f, 0.0f, 0.0f, 0.0f};
    auto out = router.Process(p, silence);
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
    router.Init(48000.0f, &bufL, &bufR);

    RouterParams p;
    p.mode = RouterMode::PARALLEL;
    p.feedback = 1.0f;
    p.tone = 0.5f; // Neutral position
    p.mix = 1.0f;
    p.freeze = false;
    p.output = 1.0f;
    p.trails_bypass = false;

    RouterSignals s = {1.0f, 1.0f, 0.0f, 0.0f}; // Input = 1.0

    // 1. Unity Drive: Result should be approx 0.47 (Saturate(0.906 * 1.0))
    router.Reset();
    p.drive = 1.0f;
    router.Process(p, s);
    float out1 = bufL.Read(1.0f);

    // 2. High Drive: Result should be significantly higher, approaching 1.0
    router.Reset();
    p.drive = 10.0f;
    router.Process(p, s);
    float out10 = bufL.Read(1.0f);

    assert(out10 > out1);
    assert(out10 < 1.0f);
    assert(out10 > 0.8f); // Pushed hard into the clipper

    std::cout << "Passed." << std::endl;
}

#endif
