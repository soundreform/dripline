#pragma once
#ifndef GD_SCHEDULER_TEST_H
#define GD_SCHEDULER_TEST_H

#include "frng.h"
#include "scheduler.h"
#include <cassert>
#include <iostream>

using dripline::FastRandom;

void test_scheduler_synced_mode() {
    std::cout << "Testing Scheduler SYNCED mode... ";
    Scheduler scheduler;
    FastRandom rng;
    rng.Init(123);

    // Sample rate 1000Hz, Density 100Hz -> should trigger every 10 samples
    scheduler.Init(1000.0f, &rng);

    SchedulerParams p;
    p.mode = SchedulerMode::SYNCED;
    p.density = 100.0f;

    // Accumulate 9 samples (0.1 * 9 = 0.9)
    for (int i = 0; i < 9; i++) {
        assert(scheduler.Process(p) == 0);
    }

    // 10th sample reaches threshold 1.0. SYNCED mode triggers 2 voices.
    assert(scheduler.Process(p) == 2);

    std::cout << "Passed." << std::endl;
}

void test_scheduler_burst_mode() {
    std::cout << "Testing Scheduler BURST mode... ";
    Scheduler scheduler;
    FastRandom rng;
    rng.Init(456);
    scheduler.Init(1000.0f, &rng);

    SchedulerParams p;
    p.mode = SchedulerMode::BURST;
    p.density = 1000.0f; // 1.0 per sample

    // BURST mode triggers a cluster based on modulation depth
    assert(scheduler.Process(p, 0.0f) == 1); // NONE
    assert(scheduler.Process(p, 0.5f) == 3); // SOME
    assert(scheduler.Process(p, 1.0f) == 6); // MORE

    std::cout << "Passed." << std::endl;
}

void test_scheduler_free_mode() {
    std::cout << "Testing Scheduler FREE mode... ";
    Scheduler scheduler;
    FastRandom rng;
    rng.Init(789);
    scheduler.Init(1000.0f, &rng);

    SchedulerParams p;
    p.mode = SchedulerMode::FREE;
    p.density = 500.0f; // 0.5 per sample

    // threshold starts at 1.0. Accumulates 0.5 -> 1.0
    assert(scheduler.Process(p) == 0); // Accumulator = 0.5
    assert(scheduler.Process(p) == 1); // Accumulator = 1.0, Triggered!

    std::cout << "Passed." << std::endl;
}

void test_scheduler_reset() {
    std::cout << "Testing Scheduler reset... ";
    Scheduler scheduler;
    FastRandom rng;
    rng.Init(123);
    scheduler.Init(1000.0f, &rng);

    SchedulerParams p;
    p.mode = SchedulerMode::SYNCED;
    p.density = 500.0f; // 0.5 per sample

    // 1. Accumulate partially
    assert(scheduler.Process(p) == 0); // Accumulator = 0.5

    // 2. Reset
    scheduler.Reset();

    // 3. Verify it starts from zero
    assert(scheduler.Process(p) == 0); // Accumulator would have been 1.0 and triggered without reset
    assert(scheduler.Process(p) == 2); // Now it reaches 1.0 and triggers (SYNCED mode = 2)

    // 4. Test threshold reset for FREE mode
    p.mode = SchedulerMode::FREE;
    p.density = 1000.0f; // 1.0 per sample

    // Trigger once to change threshold from 1.0 to a random value
    scheduler.Process(p);

    scheduler.Reset();
    // After reset, threshold must be exactly 1.0.
    // With density 1000 and SR 1000, 1 sample should trigger immediately.
    assert(scheduler.Process(p) == 1);

    std::cout << "Passed." << std::endl;
}

#endif
