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
            // set flag
            bool yes = true;
            __atomic_store(&this->signalled, &yes, __ATOMIC_RELEASE);

            // unblock the waiting threads
            this->unblock();
        }

    private:
        /// signal flag
        bool signalled = false;
};
}

#endif
