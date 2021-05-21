#ifndef KERNEL_SCHED_TIMERBLOCKER_H
#define KERNEL_SCHED_TIMERBLOCKER_H

#include <stdint.h>

#include "Blockable.h"
#include "Deadline.h"

#include <platform.h>

namespace sched {
/**
 * Blocks a thread for a certain amount of time, based on the system time tick.
 */
class TimerBlocker: public Blockable {
    public:
        static rt::SharedPtr<TimerBlocker> make(const uint64_t nanos) {
            rt::SharedPtr<TimerBlocker> ptr(new TimerBlocker(nanos));
            ptr->us = rt::WeakPtr<TimerBlocker>(ptr);
            return ptr;
        }

    private:
        /**
         * Deadline object for timed waits (sleeps)
         *
         * Note that the pointer to the blocker is stored as a plain pointer; as long as the
         * blocker is valid, we'll exist. At deallocation, if still outstanding, the deadline is
         * removed.
         */
        struct TimerDeadline: public Deadline {
            TimerDeadline(const uint64_t _when, TimerBlocker* _blocker) : Deadline(_when),
                blocker(_blocker) {}

            /**
             * On expiration, call back into the blocker object
             */
            void operator()() override {
                this->blocker->timerFired();
            }

            /// the timer blocker to yeet when the deadline expires
            TimerBlocker *blocker;
        };

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
            if(auto death = this->deadline.lock()) {
                sched::Scheduler::get()->removeDeadline(death);
                this->deadline = nullptr;
            }
        }

        /// When we're going to start blocking, actually install the timer.
        void willBlockOn(const rt::SharedPtr<Thread> &t) override {
            Blockable::willBlockOn(t);

            // set the timer (multiples of 10ms). include a fudge factor
            //const auto interval = (this->interval / 10000000ULL) * 10000000ULL;
            const auto interval = this->interval;
            const auto dueAt = platform_timer_now() + interval + 10000ULL;

            // create deadline
            auto death = rt::MakeShared<TimerDeadline>(dueAt, this);
            REQUIRE(death, "failed to allocate timer deadline object");

            this->deadline = rt::WeakPtr<TimerDeadline>(death);

            // add to scheduler
            sched::Scheduler::get()->addDeadline(death);
        }

    private:
        /**
         * Unblocks the task when the timer has fired.
         */
        void timerFired() {
            bool no = false, yes = true;
            if(__atomic_compare_exchange(&this->hasFired, &no, &yes, false, __ATOMIC_RELEASE,
                        __ATOMIC_RELAXED)) {
                // unblock the task
                this->blocker->unblock(this->us.lock());
            }
        }

    private:
        rt::WeakPtr<TimerBlocker> us;

        /// the associated deadline
        rt::WeakPtr<TimerDeadline> deadline;
        /// whether the timer has fired or not
        bool hasFired = false;

        /// interval for the timer (in ns)
        uint64_t interval = 0;
};
}

#endif
