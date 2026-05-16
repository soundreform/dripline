#pragma once
#ifndef GD_MODULATOR_TEST_H
#define GD_MODULATOR_TEST_H

#include "modulator.h"
#include <cassert>
#include <iostream>

using namespace dsp;

void test_modulator_depth_none() {
    std::cout << "Testing Modulator depth NONE... ";
    Modulator mod;
    ModulatorConfig mod_cfg = {48000.0f, 1.0f};
    mod.Init(mod_cfg);

    RawModulatorParams raw_params;
    raw_params.depth = ModulatorDepth::NONE;
    mod.UpdateParams(raw_params);
    for (int i = 0; i < 1000; i++) {
        float scaler = mod.Process();
        assert(scaler == 0.0f);
    }
    std::cout << "Passed." << std::endl;
}

void test_modulator_depth_some() {
    std::cout << "Testing Modulator depth SOME... ";
    Modulator mod;
    ModulatorConfig mod_cfg = {48000.0f, 1.0f};
    mod.Init(mod_cfg);

    RawModulatorParams raw_params;
    raw_params.depth = ModulatorDepth::SOME;
    mod.UpdateParams(raw_params);
    for (int i = 0; i < 1000; i++) {
        float scaler = mod.Process();
        assert(scaler >= 0.0f && scaler <= 0.5f);
    }
    std::cout << "Passed." << std::endl;
}

void test_modulator_depth_more() {
    std::cout << "Testing Modulator depth MORE... ";
    Modulator mod;
    ModulatorConfig mod_cfg = {48000.0f, 1.0f};
    mod.Init(mod_cfg);

    RawModulatorParams raw_params;
    raw_params.depth = ModulatorDepth::MORE;
    mod.UpdateParams(raw_params);
    for (int i = 0; i < 1000; i++) {
        float scaler = mod.Process();
        assert(scaler >= 0.0f && scaler <= 1.0f);
    }
    std::cout << "Passed." << std::endl;
}
#endif
