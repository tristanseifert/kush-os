/*
 * Various structures used by the scheduler, which become a part of various other system objects
 * (such as threads and tasks)
 */
#ifndef KERNEL_SCHED_SCHEDULERDATA_H
#define KERNEL_SCHED_SCHEDULERDATA_H

#include <cstddef>
#include <cstdint>

namespace sched {
/**
 * Scheduler specific data that becomes a part of every thread's info structure for use in
 * storing info like priorities.
 */
struct SchedulerThreadData {
    /// Current run queue level
    size_t level = 0;

    /// highest priority queue into which the thread may be scheduled (lower = higher priority)
    size_t maxLevel = 0;
    /// last level at which the thread was scheduled
    size_t lastLevel = UINTPTR_MAX;

    /// Actual number of nanoseconds of quantum time at this level
    uint64_t quantumTotal = 0;
    /// Number of nanoseconds of time quantum used at this level
    uint64_t quantumUsed = 0;

    /**
     * User specified priority; this is a number in [-100, 100] that affects the run queue level,
     * and also somewhat the quantum length.
     *
     * These values have no concrete meaning; only that values greater than zero will give the
     * thread a higher chance of running, and values that are less than zero will reduce the
     * thread's chance of getting scheduled.
     */
    int16_t priorityOffset = 0;

};

}

#endif
