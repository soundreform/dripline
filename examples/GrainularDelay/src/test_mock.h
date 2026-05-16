#pragma once
#ifndef GD_TEST_MOCK_H
#define GD_TEST_MOCK_H

#include "daisysp.h"

/**
 * Mock implementations for DaisySP Oscillator to satisfy the linker during host-side tests.
 * These are simple stubs that allow the Mod class to be tested in isolation.
 */
namespace daisysp {
inline float Oscillator::Process() {
    return 0.0f;
} // Returns a neutral value for LFO math
inline float Oscillator::CalcPhaseInc(float f) {
    return 0.0f;
}
} // namespace daisysp

#endif // GD_TEST_MOCK_H
