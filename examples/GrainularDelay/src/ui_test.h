#pragma once
#ifndef GD_UI_TEST_H
#define GD_UI_TEST_H

#include "ui.h"
#include <cassert>
#include <cmath>
#include <iostream>

inline void test_ui_manager_priorities() {
    std::cout << "Testing UiManager priorities... ";
    UiManager ui;
    ui.Init(1000.0f); // 1ms control rate

    HardwareState hw = {};
    EngineParams p = {};
    UiEvents events = {};

    hw.bypass_state = false; // Active
    auto out = ui.Process(hw, p, events, 0, 0.0f);
    assert(out.status.g == 1.0f && out.status.r == 0.0f && out.status.b == 0.0f);

    // Freeze overrides Active
    p.router.freeze = true;
    out = ui.Process(hw, p, events, 0, 0.0f);
    assert(out.status.b == 1.0f && out.status.r == 0.0f && out.status.g == 0.0f);

    // Clipping overrides Freeze
    events.clipping = true;
    out = ui.Process(hw, p, events, 0, 0.0f);
    assert(out.status.r == 1.0f && out.status.g == 0.0f && out.status.b == 0.0f);
    events.clipping = false; // Clear event

    // Panic overrides everything for both status and tempo outputs
    events.buffer_cleared = true;
    out = ui.Process(hw, p, events, 0, 0.0f);
    assert(out.status.r == 1.0f && out.status.g == 0.0f);
    assert(out.tempo.r == 1.0f && out.tempo.g == 0.0f);

    std::cout << "Passed." << std::endl;
}

inline void test_ui_manager_timers() {
    std::cout << "Testing UiManager timers... ";
    UiManager ui;
    ui.Init(1000.0f);

    HardwareState hw = {};
    EngineParams p = {};
    UiEvents events = {};

    events.buffer_cleared = true;
    ui.Process(hw, p, events, 0, 0.0f);
    events.buffer_cleared = false;

    // Advance time by 0.25s (250 blocks at 1000Hz)
    for (int i = 0; i < 250; i++) {
        ui.Process(hw, p, events, 0, 0.0f);
    }

    // Timer should be expired, status goes back to normal (Green Active)
    auto out = ui.Process(hw, p, events, 0, 0.0f);
    assert(out.status.g == 1.0f && out.status.r == 0.0f);

    std::cout << "Passed." << std::endl;
}

inline void test_ui_manager_tempo_color() {
    std::cout << "Testing UiManager tempo color... ";
    UiManager ui;
    ui.Init(1000.0f);

    HardwareState hw = {};
    hw.knobs[static_cast<int>(KnobId::DENSITY)] = 0.1f; // 1/4 -> White
    EngineParams p = {};
    UiEvents events = {};

    // 16 active grains, phase 1.0
    auto out = ui.Process(hw, p, events, 16, 1.0f);

    // Pulse should exponentially decay to near-zero (exp(-5) ~ 0.0067)
    assert(std::fabs(out.tempo.r - 0.0067f) < 0.01f);

    std::cout << "Passed." << std::endl;
}

#endif
