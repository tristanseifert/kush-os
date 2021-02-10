#ifndef KERNEL_SCHED_TIMERBLOCKER_H
#define KERNEL_SCHED_TIMERBLOCKER_H

#include <stdint.h>

#include "Blockable.h"

#include <platform.h>

namespace sched {
static void _TBTimerExpired(const uintptr_t, void *);

/**
 * Blocks a thread for a certain amount of time, based on the system time tick.
 */
class TimerBlocker: public Blockable {
    friend void _TBTimerExpired(const uintptr_t, void *);

    public:
        /**
         * Creates a new timer blocker, with the given number of nanoseconds set in the future.
         */
        TimerBlocker(const uint64_t nanos) : interval(nanos) {}

        /**
         * Ensures the timer is deallocated if it hasn't fired.
         */
        ~TimerBlocker() {
            if(!this->hasFired) {
                platform_timer_remove(this->timer);
            }
        }

        /// We're signaled once the timer has fired.
        bool isSignalled() override {
            return this->hasFired;
        }
        /// Do nothing, since this is a one shot timer.
        void reset() override {

        }

        /// When we're going to start blocking, actually install the timer.
        void willBlockOn(Thread *t) override {
            Blockable::willBlockOn(t);

            // set the timer
            const auto deadline = platform_timer_now() + this->interval;
            this->timer = platform_timer_add(deadline, [](const uintptr_t, void *ctx) {
                reinterpret_cast<TimerBlocker *>(ctx)->timerFired();
            }, this);

            REQUIRE(this->timer, "failed to allocate timer");
        }

    private:
        /**
         * Unblocks the task when the timer has fired.
         */
        void timerFired() {
            // set the flag
            bool yes = true;
            __atomic_store(&this->hasFired, &yes, __ATOMIC_RELEASE);

            // unblock the task
            this->unblock();
        }

    private:
        /// timer token
        uintptr_t timer = 0;
        /// whether the timer has fired or not
        bool hasFired = false;

        /// interval for the timer (in ns)
        uint64_t interval = 0;
};

/// Simply forward into a class member function
static inline void _TBTimerExpired(const uintptr_t, void *ctx) {
    reinterpret_cast<TimerBlocker *>(ctx)->timerFired();
}
}

#endif
