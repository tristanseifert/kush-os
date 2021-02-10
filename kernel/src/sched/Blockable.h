#ifndef KERNEL_SCHED_BLOCKABLE_H
#define KERNEL_SCHED_BLOCKABLE_H

#include "Thread.h"

namespace sched {
/**
 * Abstract interface of an object on which threads may block.
 *
 * Each blockable object, when blocked on, holds a reference to the thread and can wake it up to
 * place it back in the runnable state.
 */
class Blockable {
    public:
        // this does nothing
        virtual ~Blockable() = default;

        /**
         * Returns whether this blockable object has been signalled, i.e. whether any blocking
         * thread should transition from the blocked state back to being runnable. For example,
         * this could be a message arriving on a port.
         */
        virtual bool isSignalled() = 0;

        /**
         * Resets the signaled flag of this blockable object. This is called immediately before we
         * return to the thread we're blocking on.
         */
        virtual void reset() = 0;

    public:
        /**
         * Determines if there's a thread currently blocking on us.
         */
        virtual bool hasBlocker() {
            Thread *temp = nullptr;
            __atomic_load(&this->blocker, &temp, __ATOMIC_CONSUME);
            return (temp != nullptr);
        }

        /**
         * We're about to block the current thread on this object.
         */
        virtual void willBlockOn(Thread *t) {
            //this->blocker = t;
            __atomic_store(&this->blocker, &t, __ATOMIC_RELEASE);
        }

        /**
         * We've just been unblocked, possibly because of this object.
         */
        virtual void didUnblock() {
            Thread *prev = nullptr, *zero = nullptr;
            __atomic_exchange(&this->blocker, &zero, &prev, __ATOMIC_RELEASE);
        }

    protected:
        /**
         * Internal method called when the blockable object has signalled and wants to unblock any
         * thread waiting on it.
         */
        virtual void unblock() {
            REQUIRE(this->blocker, "cannot unblock object without blocker");
            this->blocker->unblock(this);
        }

    private:
        /// thread currently blocking on us, if any
        Thread *blocker = nullptr;
};
}

#endif
