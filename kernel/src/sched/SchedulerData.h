/*
 * Various structures used by the scheduler, which become a part of various other system objects
 * (such as threads and tasks)
 */
#ifndef KERNEL_SCHED_SCHEDULERDATA_H
#define KERNEL_SCHED_SCHEDULERDATA_H

#include <cstddef>
#include <cstdint>

#include <bitflags.h>

namespace sched {
ENUM_FLAGS_EX(SchedulerThreadDataFlags, uintptr_t);
enum class SchedulerThreadDataFlags: uintptr_t {
    /// No flags set
    None                                = 0,

    /**
     * Do not automatically reschedule the thread, if it becomes preempted
     */
    DoNotSchedule                       = (1 << 15),
    /**
     * Thread should be executed when idle; this means that it's not preempted as normal, but will
     * instead get to run as long as there's nothing else to do.
     *
     * Used internally by the scheduler.
     */
    Idle                                = (1 << 16),
};


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

    /// Total number of nanoseconds of CPU time this thread has received
    uint64_t cpuTime = 0;
    /// Actual number of nanoseconds of quantum time at this level
    uint64_t quantumTotal = 0;
    /// Number of nanoseconds of time quantum used at this level
    uint64_t quantumUsed = 0;

    /// number of times the thread has been inserted to a run queue
    size_t queuePushed = 0;
    /// number of times the thread has been popped from a run queue
    size_t queuePopped = 0;

    /// flags defining the thread's state and scheduler beahvior
    SchedulerThreadDataFlags flags = SchedulerThreadDataFlags::None;

    /**
     * User specified priority; this is a number in [-100, 100] that affects the run queue level,
     * and also somewhat the quantum length.
     *
     * These values have no concrete meaning; only that values greater than zero will give the
     * thread a higher chance of running, and values that are less than zero will reduce the
     * thread's chance of getting scheduled.
     */
    int16_t priorityOffset = 0;

    /// whether the thread was most recently preempted
    bool preempted = false;
};

}

#endif
