#pragma once
#ifndef GD_MOD_TEST_H
#define GD_MOD_TEST_H

#include "mod.h"
#include <cassert>
#include <iostream>

void test_mod_depth_none() {
    std::cout << "Testing Mod depth NONE... ";
    Mod mod;
    mod.Init(48000.0f);
    ModParams params;
    params.depth = ModDepth::NONE;

    for (int i = 0; i < 1000; i++) {
        float scaler = mod.Process(params);
        assert(scaler == 0.0f);
    }
    std::cout << "Passed." << std::endl;
}

void test_mod_depth_some() {
    std::cout << "Testing Mod depth SOME... ";
    Mod mod;
    mod.Init(48000.0f);
    ModParams params;
    params.depth = ModDepth::SOME;

    for (int i = 0; i < 1000; i++) {
        float scaler = mod.Process(params);
        assert(scaler >= 0.0f && scaler <= 0.5f);
    }
    std::cout << "Passed." << std::endl;
}

void test_mod_depth_more() {
    std::cout << "Testing Mod depth MORE... ";
    Mod mod;
    mod.Init(48000.0f);
    ModParams params;
    params.depth = ModDepth::MORE;

    for (int i = 0; i < 1000; i++) {
        float scaler = mod.Process(params);
        assert(scaler >= 0.0f && scaler <= 1.0f);
    }
    std::cout << "Passed." << std::endl;
}
#endif
