#ifndef KERNEL_SCHED_SIGNALFLAG_H
#define KERNEL_SCHED_SIGNALFLAG_H

#include "Blockable.h"

namespace sched {
/**
 * Signal flags are similar to binary semaphores, but are one-shot.
 *
 * These are used internally for events such as waiting on task and thread termination.
 */
class SignalFlag: public Blockable {
    public:
        static rt::SharedPtr<SignalFlag> make() {
            rt::SharedPtr<SignalFlag> ptr(new SignalFlag);
            ptr->us = rt::WeakPtr<SignalFlag>(ptr);
            return ptr;
        }

    private:
        SignalFlag() = default;

    public:
        /// We're signalled if the signal flag is set.
        bool isSignalled() override {
            return this->signalled;
        }
        /// Resets the signal state.
        void reset() override {
            // we don't do anything; we cannot be reset.
        }
        /// Prepare to block on the flag.
        // void willBlockOn(Thread *t) override {}

        /**
         * Signals the flag, waking any threads that are pending on it.
         */
        void signal() {
            // set flag and unblock if this is the first time we're to unblock
            bool no = false, yes = true;
            if(__atomic_compare_exchange(&this->signalled, &no, &yes, false, __ATOMIC_RELEASE,
                        __ATOMIC_RELAXED)) {
                // unblock the task
                this->blocker->unblock(this->us.lock());
            }
        }

    private:
        rt::WeakPtr<SignalFlag> us;

        /// signal flag
        bool signalled = false;
};
}

#endif
