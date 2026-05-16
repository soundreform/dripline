#pragma once
#ifndef GD_MAPPER_TEST_H
#define GD_MAPPER_TEST_H

#include "mapper.h"
#include <cassert>
#include <cmath>
#include <iostream>

inline void test_mapper_initialization() {
    std::cout << "Testing Mapper Initialization... ";
    Mapper mapper;
    mapper.Init(48000.0f);
    assert(mapper.GetExpressionTarget() == KnobId::UNASSIGNED);
    std::cout << "Passed." << std::endl;
}

inline void test_mapper_expression_assignment() {
    std::cout << "Testing Mapper Expression Assignment... ";
    Mapper mapper;
    mapper.Init(48000.0f);

    HardwareState hw = {};
    hw.fs_2_held = true;
    hw.expr_target_bin = 1; // KnobId::PITCH

    EngineParams p = {};
    bool new_assignment = mapper.Map(hw, p);

    assert(new_assignment == true);
    assert(mapper.GetExpressionTarget() == KnobId::PITCH);

    // Call again, should be false since the target hasn't changed
    new_assignment = mapper.Map(hw, p);
    assert(new_assignment == false);

    std::cout << "Passed." << std::endl;
}

inline void test_mapper_effective_knob() {
    std::cout << "Testing Mapper Effective Knob... ";
    Mapper mapper;
    mapper.Init(48000.0f);

    HardwareState hw = {};
    hw.fs_2_held = true;
    hw.expr_target_bin = 1; // KnobId::PITCH

    EngineParams p = {};
    mapper.Map(hw, p); // Assign expression target to PITCH

    hw.fs_2_held = false;
    hw.knobs[static_cast<int>(KnobId::PITCH)] = 0.5f;
    hw.knobs[static_cast<int>(KnobId::EXPRESSION)] = 1.0f; // depth = 1.0f
    hw.expression = 0.25f;                                 // Should add 0.25f to PITCH

    mapper.Map(hw, p);

    // Effective pitch should be 0.5 + 0.25 * ((1.0 - 0.5) * 2.0) = 0.75
    // pitch knob mapped: 0.75.
    // GetPitch: direction = 0 (DOWN), pitch = 0.75 * 4.0 - 2.0 = 1.0. direction 0 means -abs(1.0) = -1.0.
    assert(std::fabs(p.grain.pitch - (-1.0f)) < 1e-5);

    std::cout << "Passed." << std::endl;
}

inline void test_mapper_switches() {
    std::cout << "Testing Mapper Switches... ";
    Mapper mapper;
    mapper.Init(48000.0f);

    HardwareState hw = {};
    EngineParams p = {};

    // ROUTING
    hw.switches[static_cast<int>(SwitchId::ROUTING)] = 0; // DOWN
    mapper.Map(hw, p);
    assert(p.router.mode == RouterMode::SERIES);

    hw.switches[static_cast<int>(SwitchId::ROUTING)] = 1; // CENTER
    mapper.Map(hw, p);
    assert(p.router.mode == RouterMode::PING_PONG);

    hw.switches[static_cast<int>(SwitchId::ROUTING)] = 2; // UP
    mapper.Map(hw, p);
    assert(p.router.mode == RouterMode::PARALLEL);

    // QUANTIZE
    hw.switches[static_cast<int>(SwitchId::QUANTIZE)] = 0;
    mapper.Map(hw, p);
    assert(p.grain.quantize_mode == QuantizeMode::OCTAVES_FIFTHS);

    hw.switches[static_cast<int>(SwitchId::QUANTIZE)] = 1;
    mapper.Map(hw, p);
    assert(p.grain.quantize_mode == QuantizeMode::SEMITONES);

    hw.switches[static_cast<int>(SwitchId::QUANTIZE)] = 2;
    mapper.Map(hw, p);
    assert(p.grain.quantize_mode == QuantizeMode::FREE);

    // DEPTH
    hw.switches[static_cast<int>(SwitchId::DEPTH)] = 0;
    mapper.Map(hw, p);
    assert(p.mod.depth == ModDepth::MORE);

    hw.switches[static_cast<int>(SwitchId::DEPTH)] = 1;
    mapper.Map(hw, p);
    assert(p.mod.depth == ModDepth::NONE);

    hw.switches[static_cast<int>(SwitchId::DEPTH)] = 2;
    mapper.Map(hw, p);
    assert(p.mod.depth == ModDepth::SOME);

    std::cout << "Passed." << std::endl;
}

inline void test_mapper_density() {
    std::cout << "Testing Mapper Density... ";
    Mapper mapper;
    mapper.Init(48000.0f);

    HardwareState hw = {};
    EngineParams p = {};

    // Test tap tempo mapping
    hw.tap_tempo_hz = 2.0f;
    hw.knobs[static_cast<int>(KnobId::DENSITY)] = 0.5f; // 1/8 note -> multiplier 2.0
    mapper.Map(hw, p);
    assert(std::fabs(p.scheduler.density - 4.0f) < 1e-5); // 2.0 Hz * 2.0 = 4.0 Hz

    // Test free mapping
    hw.tap_tempo_hz = 0.0f;
    hw.knobs[static_cast<int>(KnobId::DENSITY)] = 0.0f;
    mapper.Map(hw, p);
    assert(std::fabs(p.scheduler.density - 1.0f) < 1e-5); // exp(0) = 1.0 Hz

    std::cout << "Passed." << std::endl;
}

#endif // GD_MAPPER_TEST_H
