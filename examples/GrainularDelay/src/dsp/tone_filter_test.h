#pragma once
#ifndef DSP_TONE_FILTER_TEST_H
#define DSP_TONE_FILTER_TEST_H

#include "tone_filter.h"
#include <cassert>
#include <cmath>
#include <iostream>

using namespace dsp;

void test_tone_filter_stateful() {
    std::cout << "Testing ToneFilter stateful filtering... ";
    ToneFilter filter;
    filter.Init(48000.0f);

    filter.SetPos(0.25f); // Some filtering
    float out1_L = filter.ProcessLeft(1.0f);
    float out1_R = filter.ProcessRight(1.0f);
    float out2_L = filter.ProcessLeft(1.0f);
    float out2_R = filter.ProcessRight(1.0f);

    // State should change the output
    assert(out1_L != out2_L);
    assert(out1_R != out2_R);

    // And left and right channels should be identical with identical input
    assert(std::fabs(out1_L - out1_R) < 1e-5);
    assert(std::fabs(out2_L - out2_R) < 1e-5);

    std::cout << "Passed." << std::endl;
}

void test_tone_filter_stereo_independence() {
    std::cout << "Testing ToneFilter stereo independence... ";
    ToneFilter filter;
    filter.Init(48000.0f);

    // Process different signals on L and R
    filter.SetPos(0.25f);
    float out1_L = filter.ProcessLeft(1.0f);
    float out1_R = filter.ProcessRight(0.5f);

    // Now process with L and R swapped after reset
    filter.Reset();
    filter.SetPos(0.25f);
    float out2_L = filter.ProcessLeft(0.5f);
    float out2_R = filter.ProcessRight(1.0f);

    // The outputs should be swapped compared to out1
    assert(std::fabs(out2_L - out1_R) < 1e-5);
    assert(std::fabs(out2_R - out1_L) < 1e-5);

    std::cout << "Passed." << std::endl;
}

#endif // GD_TONE_FILTER_TEST_H
