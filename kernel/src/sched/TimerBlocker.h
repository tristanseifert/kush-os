#ifndef KERNEL_SCHED_TIMERBLOCKER_H
#define KERNEL_SCHED_TIMERBLOCKER_H

#include <stdint.h>

#include "Blockable.h"

#include <platform.h>

namespace sched {
/**
 * Blocks a thread for a certain amount of time, based on the system time tick.
 */
class TimerBlocker: public Blockable {
    public:
        /**
         * Creates a new timer blocker, with the given number of nanoseconds set in the future.
         */
        TimerBlocker(const uint64_t nanos) : interval(nanos) {}

        /**
         * Ensures the timer is deallocated if it hasn't fired.
         */
        ~TimerBlocker() {
            this->reset();
        }

        /// We're signaled once the timer has fired.
        bool isSignalled() override {
            return this->hasFired;
        }
        /// Disables the timer, if set.
        void reset() override {
            if(!this->hasFired) {
                platform_timer_remove(this->timer);
            }
        }

        /// When we're going to start blocking, actually install the timer.
        void willBlockOn(const rt::SharedPtr<Thread> &t) override {
            Blockable::willBlockOn(t);

            // set the timer (multiples of 10ms). include a fudge factor
            //const auto interval = (this->interval / 10000000ULL) * 10000000ULL;
            const auto interval = this->interval;
            const auto deadline = platform_timer_now() + interval + 10000ULL;

            this->timer = platform_timer_add(deadline, [](const uintptr_t tok, void *ctx) {
                if(!ctx) return;
                auto blocker = reinterpret_cast<TimerBlocker *>(ctx);

                // log("timer blocker %p", ctx);
                blocker->timerFired(tok);
            }, this);

            REQUIRE(this->timer, "failed to allocate timer");
        }

    private:
        /**
         * Unblocks the task when the timer has fired.
         */
        void timerFired(const uintptr_t tok) {
            bool no = false, yes = true;
            if(__atomic_compare_exchange(&this->hasFired, &no, &yes, false, __ATOMIC_RELEASE,
                        __ATOMIC_RELAXED)) {
                // log("unblocking %p", this->blocker);

                // unblock the task
                this->unblock();
            }
        }

    private:
        /// timer token
        uintptr_t timer = 0;
        /// whether the timer has fired or not
        bool hasFired = false;

        /// interval for the timer (in ns)
        uint64_t interval = 0;
};
}

#endif
