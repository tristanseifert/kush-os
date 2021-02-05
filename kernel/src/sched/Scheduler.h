#ifndef KERNEL_SCHED_SCHEDULER_H
#define KERNEL_SCHED_SCHEDULER_H

#include <stdint.h>

extern "C" void kernel_init();

namespace sched {
struct Thread;

/**
 * Scheduler is responsible for ensuring all runnable threads get CPU time. It does this by taking
 * threads from the head of each priority level's ready queue and running them until that queue is
 * empty, then continuing on to the queue of the next lowest priority, and so on.
 *
 * In essence, the scheduler implements a round-robin scheduling scheme inside priority bands, but
 * ensures that a higher priority task will ALWAYS receive CPU time before a lower priority one.
 *
 * XXX: This will require a fair bit of re-architecting when we want to support multiprocessor
 *      machines. Probably making data structures shared is sufficient.
 */
class Scheduler {
    friend void ::kernel_init();
    friend struct Thread;

    public:
        // return the shared scheduler instance
        static Scheduler *get() {
            return gShared;
        }

        // return the thread running on the current processor
        Thread *runningThread() const {
            return this->running;
        }

    private:
        /// updates the current CPU's running thread
        void setRunningThread(Thread *t) {
            this->running = t;
        }

    private:
        static void init();

        Scheduler();

    private:
        static Scheduler *gShared;

    private:
        /// the thread that is currently being executed
        Thread *running = nullptr;
};
}

#endif
