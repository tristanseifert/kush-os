#ifndef KERNEL_SCHED_SLEEPDEADLINE_H
#define KERNEL_SCHED_SLEEPDEADLINE_H

#include <stdint.h>

#include "Deadline.h"

#include <platform.h>

namespace sched {
/**
 * Deadline object for timed waits (sleeps)
 *
 * When a thread goes to sleep (for the sole purpose of sleeping) it enters the Sleeping state,
 * and when the deadline expires, the thread is set back to the runnable state and placed on the
 * scheduler run queue.
 */
struct SleepDeadline: public Deadline {
    /**
     * Creates a new sleep deadline that will expire at the given time point.
     */
    SleepDeadline(const uint64_t _when, const rt::SharedPtr<Thread> &_thread) : Deadline(_when),
        thread(_thread) {}

    /**
     * On expiration, call back into the blocker object
     */
    void operator()() override {
        this->thread->setState(Thread::State::Runnable);
        sched::Scheduler::get()->schedule(this->thread);
    }

    /// thread that will be resumed
    rt::SharedPtr<Thread> thread;
};
}

#endif
