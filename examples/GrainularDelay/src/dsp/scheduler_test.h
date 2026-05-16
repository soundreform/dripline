#pragma once
#ifndef DSP_SCHEDULER_TEST_H
#define DSP_SCHEDULER_TEST_H

#include "dripline/frng.h"
#include "scheduler.h"
#include <cassert>
#include <iostream>

using namespace dsp;
using dripline::FastRandom;

void test_scheduler_synced_mode() {
    std::cout << "Testing Scheduler SYNCED mode... ";
    Scheduler scheduler;
    FastRandom rng;
    rng.Init(123);

    // Sample rate 1000Hz, Density 100Hz -> should trigger every 10 samples
    SchedulerConfig sched_cfg = {1000.0f, &rng, 1.0f};
    scheduler.Init(sched_cfg);

    RawSchedulerParams raw_p;
    raw_p.rhythm = SchedulerRhythm::SYNCED;
    // Set density to 100Hz via tapped tempo
    raw_p.manual_freq = 0.0f; // Corresponds to Subdivision::QUARTER
    raw_p.tapped_freq = 1.0f; // Corresponds to 100Hz
    scheduler.UpdateParams(raw_p);

    // Accumulate 9 samples (100Hz / 1000Hz * 9 = 0.9)
    for (int i = 0; i < 9; i++) {
        assert(scheduler.Process() == 0);
    }

    // 10th sample reaches threshold 1.0. SYNCED mode triggers 2 voices with default mod_scaler=1.0.
    assert(scheduler.Process() == 2);

    std::cout << "Passed." << std::endl;
}

void test_scheduler_burst_mode() {
    std::cout << "Testing Scheduler BURST mode... ";
    Scheduler scheduler;
    FastRandom rng;
    rng.Init(456);
    // With max density of 400Hz, use a 400Hz sample rate to get 1.0 increment per sample
    SchedulerConfig sched_cfg = {400.0f, &rng, 1.0f};
    scheduler.Init(sched_cfg);

    RawSchedulerParams raw_p;
    raw_p.rhythm = SchedulerRhythm::BURST;
    // Set max density (400Hz)
    raw_p.manual_freq = 1.0f; // Corresponds to Subdivision::SIXTEENTH
    raw_p.tapped_freq = 1.0f; // Corresponds to 100Hz
    scheduler.UpdateParams(raw_p);
    // BURST mode triggers a cluster based on modulation depth
    assert(scheduler.Process(0.0f) == 1); // NONE
    assert(scheduler.Process(0.5f) == 3); // SOME
    assert(scheduler.Process(1.0f) == 6); // MORE

    std::cout << "Passed." << std::endl;
}

void test_scheduler_free_mode() {
    std::cout << "Testing Scheduler FREE mode... ";
    Scheduler scheduler;
    FastRandom rng;
    rng.Init(789);
    // Use SR=200Hz with max density 100Hz to get 0.5 increment per sample
    SchedulerConfig sched_cfg = {200.0f, &rng, 1.0f};
    scheduler.Init(sched_cfg);

    RawSchedulerParams raw_p;
    raw_p.rhythm = SchedulerRhythm::FREE;
    // Set density to 100Hz (max possible in FREE mode)
    raw_p.manual_freq = 1.0f; // Corresponds to 100Hz
    raw_p.tapped_freq = 0.0f; // Not used in FREE mode
    scheduler.UpdateParams(raw_p);

    // threshold starts at 1.0. Accumulates 0.5 -> 1.0
    assert(scheduler.Process() == 0); // Accumulator = 0.5
    assert(scheduler.Process() == 1); // Accumulator = 1.0, Triggered!

    std::cout << "Passed." << std::endl;
}

void test_scheduler_reset() {
    std::cout << "Testing Scheduler reset... ";
    Scheduler scheduler;
    FastRandom rng;
    rng.Init(123);
    // Use SR=800Hz with max density 400Hz to get 0.5 increment per sample
    SchedulerConfig sched_cfg = {800.0f, &rng, 1.0f};
    scheduler.Init(sched_cfg);

    RawSchedulerParams raw_p;
    raw_p.rhythm = SchedulerRhythm::SYNCED;
    // Set max density (400Hz)
    raw_p.manual_freq = 1.0f; // Corresponds to Subdivision::SIXTEENTH
    raw_p.tapped_freq = 1.0f; // Corresponds to 100Hz
    scheduler.UpdateParams(raw_p);
    // 1. Accumulate partially
    assert(scheduler.Process() == 0); // Accumulator = 0.5

    // 2. Reset
    scheduler.Reset();

    // 3. Verify it starts from zero
    assert(scheduler.Process() == 0); // Accumulator would have been 1.0 and triggered without reset
    assert(scheduler.Process() == 2); // Now it reaches 1.0 and triggers (SYNCED mode = 2)

    // 4. Test threshold reset for FREE mode
    // Re-init for 1.0 increment per sample
    sched_cfg.sample_rate = 100.0f;
    scheduler.Init(sched_cfg);
    raw_p.rhythm = SchedulerRhythm::FREE;
    raw_p.manual_freq = 1.0f; // 100Hz
    scheduler.UpdateParams(raw_p);
    // Trigger once to change threshold from 1.0 to a random value
    scheduler.Process();

    scheduler.Reset();
    // After reset, threshold must be exactly 1.0.
    // With density 100Hz and SR 100Hz, 1 sample should trigger immediately.
    assert(scheduler.Process() == 1);

    std::cout << "Passed." << std::endl;
}

void test_scheduler_modulation() {
    std::cout << "Testing Scheduler modulation... ";

    Scheduler scheduler;
    FastRandom rng;
    rng.Init(123);
    SchedulerConfig cfg;
    // Set SR to match max density to ensure a trigger on every Process() call
    cfg.sample_rate = 400.f;
    cfg.rng = &rng;
    cfg.smoothing_coeff = 1.0f;
    scheduler.Init(cfg);

    RawSchedulerParams raw_p = {};
    raw_p.manual_freq = 1.0f; // Corresponds to Subdivision::SIXTEENTH
    raw_p.tapped_freq = 1.0f; // Corresponds to 100Hz, giving density of 400Hz

    // Test SYNCED mode trigger scaling
    raw_p.rhythm = SchedulerRhythm::SYNCED;
    scheduler.UpdateParams(raw_p);
    assert(scheduler.Process(0.0f) == 1); // mod_scaler=0.0 -> 1 + 1*0 = 1
    assert(scheduler.Process(0.5f) == 1); // mod_scaler=0.5 -> 1 + 1*0.5 = 1 (int cast)
    assert(scheduler.Process(1.0f) == 2); // mod_scaler=1.0 -> 1 + 1*1 = 2

    // Test BURST mode trigger scaling
    raw_p.rhythm = SchedulerRhythm::BURST;
    scheduler.UpdateParams(raw_p);
    assert(scheduler.Process(0.0f) == 1); // mod_scaler=0.0 -> 1 + 5*0 = 1
    assert(scheduler.Process(0.5f) == 3); // mod_scaler=0.5 -> 1 + 5*0.5 = 3 (int cast)
    assert(scheduler.Process(1.0f) == 6); // mod_scaler=1.0 -> 1 + 5*1.0 = 6

    std::cout << "Passed." << std::endl;
}

#endif
