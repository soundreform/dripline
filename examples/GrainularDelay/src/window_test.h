#pragma once
#ifndef GD_WINDOW_TEST_H
#define GD_WINDOW_TEST_H

#include "grain.h"
#include "window.h"
#include <cassert>
#include <cmath>
#include <iostream>

inline void test_window_tables() {
    std::cout << "Testing Window Tables... ";

    WindowTable window_table;
    window_table.Init();

    GrainWindow gw;
    gw.size = WindowTable::kTableSize;

    // --- Test Smooth (Hann) ---
    gw.table = window_table.GetSmooth();

    // Center peak
    assert(std::fabs(gw.Read(0.5f) - 1.0f) < 0.001f);
    // Zero crossings
    assert(std::fabs(gw.Read(0.0f)) < 0.001f);
    assert(std::fabs(gw.Read(1.0f)) < 0.001f);

    // --- Test Pluck ---
    gw.table = window_table.GetPluck();

    // 5% attack peak
    assert(std::fabs(gw.Read(0.05f) - 1.0f) < 0.001f);
    // Fast attack starts at 0
    assert(std::fabs(gw.Read(0.0f)) < 0.001f);
    // Long decay ends at a low non-zero exponential tail (~0.0067)
    assert(gw.Read(1.0f) > 0.0f && gw.Read(1.0f) < 0.01f);

    // Check linear interpolation correctness during Pluck's attack
    // At exactly 2.5%, the read head should be exactly halfway up the 5% ramp
    assert(std::fabs(gw.Read(0.025f) - 0.5f) < 0.001f);

    // --- Test Swell ---
    gw.table = window_table.GetSwell();

    // 95% attack peak
    assert(std::fabs(gw.Read(0.95f) - 1.0f) < 0.001f);
    // Fast decay ends at 0
    assert(std::fabs(gw.Read(1.0f)) < 0.001f);
    // Slow attack starts at a low non-zero exponential tail
    assert(gw.Read(0.0f) > 0.0f && gw.Read(0.0f) < 0.01f);

    // --- Test Safety Bounds ---
    // Ensure index read is strictly safe when phase hits or exceeds 1.0f
    float safe_val_at_1 = gw.Read(1.0f);
    float safe_val_above_1 = gw.Read(1.5f);
    assert(std::fabs(safe_val_at_1 - safe_val_above_1) < 0.0001f);

    std::cout << "Passed." << std::endl;
}

#endif
