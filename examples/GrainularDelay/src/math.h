#pragma once
#ifndef GD_MATH_H
#define GD_MATH_H

#ifndef UNIT_TEST
#include "arm_math.h"
#else
#include <cmath>

#define arm_cos_f32 cosf
#define arm_sin_f32 sinf
#define arm_sqrt_f32 sqrtf

#endif

inline void fonepole_deadband(float &current, float target, float alpha) {
    if (fabsf(target - current) > 0.001f) {
        current += alpha * (target - current);
    }
}

#endif
