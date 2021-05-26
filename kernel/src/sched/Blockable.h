#ifndef KERNEL_SCHED_BLOCKABLE_H
#define KERNEL_SCHED_BLOCKABLE_H

#include "Thread.h"

#include "runtime/SmartPointers.h"

namespace sched {
/**
 * Abstract interface of an object on which threads may block.
 *
 * Each blockable object, when blocked on, holds a reference to the thread and can wake it up to
 * place it back in the runnable state.
 */
class Blockable: public rt::SharedFromThis<Blockable> {
    public:
        // this does nothing
        virtual ~Blockable() {
            //REQUIRE(!this->blocker, "cannot deallocate blockable %p while thread %p is waiting",
            //        this, static_cast<void *>(this->blocker));
            this->blocker = nullptr;
        }

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
            return !!this->blocker;
        }

        /**
         * We're about to block the current thread on this object.
         *
         * @return Status code; a nonzero value will abort the block
         */
        virtual int willBlockOn(const rt::SharedPtr<Thread> &t) {
            this->blocker = t;
            return 0;
        }

        /**
         * We've just been unblocked, possibly because of this object.
         */
        virtual void didUnblock() {
            this->blocker = nullptr;
        }

    private:
        /**
         * Internal method called when the blockable object has signalled and wants to unblock any
         * thread waiting on it.
         */
        void unblock() {
            REQUIRE(this->blocker, "cannot unblock object without blocker");
            this->blocker->unblock(this->sharedFromThis());
        }

    protected:
        /// thread currently blocking on us, if any
        rt::SharedPtr<Thread> blocker = nullptr;
};
}

#endif
