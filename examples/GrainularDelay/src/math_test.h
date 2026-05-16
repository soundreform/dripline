#pragma once
#ifndef GD_MATH_TEST_H
#define GD_MATH_TEST_H

#include "math.h"
#include <cassert>
#include <cmath>
#include <iostream>

inline void test_math_fonepole_deadband() {
    std::cout << "Testing Math fonepole_deadband... ";

    float current = 0.0f;
    // 1. Outside deadband, value should interpolate
    fonepole_deadband(current, 1.0f, 0.5f);
    assert(std::fabs(current - 0.5f) < 1e-6);

    // 2. Inside deadband (diff < 0.001), value should not change
    current = 0.9995f;
    fonepole_deadband(current, 1.0f, 0.5f);
    assert(std::fabs(current - 0.9995f) < 1e-6);

    std::cout << "Passed." << std::endl;
}

#endif
