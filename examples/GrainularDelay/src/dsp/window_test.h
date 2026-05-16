#pragma once
#ifndef DSP_WINDOW_TEST_H
#define DSP_WINDOW_TEST_H

#include "window.h"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace dsp;

void test_window_interpolation() {
    std::cout << "Testing Window Interpolation... ";

    float table[] = {0.0f, 1.0f, 2.0f}; // A size 2 table (3 points total)
    Window win = {2, table};

    // Check exact indices
    assert(std::fabs(win.Read(0.0f) - 0.0f) < 0.001f);
    assert(std::fabs(win.Read(0.5f) - 1.0f) < 0.001f);
    assert(std::fabs(win.Read(1.0f) - 2.0f) < 0.001f);

    // Check linear interpolation midpoints
    assert(std::fabs(win.Read(0.25f) - 0.5f) < 0.001f);
    assert(std::fabs(win.Read(0.75f) - 1.5f) < 0.001f);

    std::cout << "Passed." << std::endl;
}

inline void test_window_tables() {
    std::cout << "Testing Window Tables... ";

    WindowBank window_bank;
    window_bank.Init();

    // --- Test Smooth (Hann) ---
    const Window *win_smooth = window_bank.Get(WindowShape::SMOOTH);
    assert(win_smooth->size == WindowBank::kTableSize);

    // Center peak
    assert(std::fabs(win_smooth->Read(0.5f) - 1.0f) < 0.001f);
    // Zero crossings
    assert(std::fabs(win_smooth->Read(0.0f)) < 0.001f);
    assert(std::fabs(win_smooth->Read(1.0f)) < 0.001f);

    // --- Test Pluck ---
    const Window *win_pluck = window_bank.Get(WindowShape::PLUCK);

    // 5% attack peak
    assert(std::fabs(win_pluck->Read(0.05f) - 1.0f) < 0.001f);
    // Fast attack starts at 0
    assert(std::fabs(win_pluck->Read(0.0f)) < 0.001f);
    // Long decay ends at a low non-zero exponential tail (~0.0067)
    assert(win_pluck->Read(1.0f) > 0.0f && win_pluck->Read(1.0f) < 0.01f);

    // Check linear interpolation correctness during Pluck's attack
    // At exactly 2.5%, the read head should be exactly halfway up the 5% ramp
    assert(std::fabs(win_pluck->Read(0.025f) - 0.5f) < 0.001f);

    // --- Test Swell ---
    const Window *win_swell = window_bank.Get(WindowShape::SWELL);

    // 95% attack peak
    assert(std::fabs(win_swell->Read(0.95f) - 1.0f) < 0.001f);
    // Fast decay ends at 0
    assert(std::fabs(win_swell->Read(1.0f)) < 0.001f);
    // Slow attack starts at a low non-zero exponential tail
    assert(win_swell->Read(0.0f) > 0.0f && win_swell->Read(0.0f) < 0.01f);

    // --- Test Safety Bounds ---
    // Ensure index read is strictly safe when phase hits or exceeds 1.0f
    float safe_val_at_1 = win_swell->Read(1.0f);
    float safe_val_above_1 = win_swell->Read(1.5f);
    assert(std::fabs(safe_val_at_1 - safe_val_above_1) < 0.0001f);

    std::cout << "Passed." << std::endl;
}

#endif
