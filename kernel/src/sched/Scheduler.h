#ifndef KERNEL_SCHED_SCHEDULER_H
#define KERNEL_SCHED_SCHEDULER_H

#include <stdint.h>

#include <platform.h>
#include <arch/spinlock.h>
#include <runtime/Queue.h>

extern "C" void kernel_init();

namespace sched {
class IdleWorker;
struct Task;
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
    friend void ::platform_kern_tick(const uintptr_t);
    friend struct Thread;

    public:
        /// Defines the priority groups tasks can fall into
        enum PriorityGroup: uint8_t {
            Highest                         = 0,
            AboveNormal                     = 1,
            Normal                          = 2,
            BelowNormal                     = 3,
            Idle                            = 4,

            kPriorityGroupMax
        };

    public:
        // return the shared scheduler instance
        static Scheduler *get() {
            return gShared;
        }

        // return the thread running on the current processor
        Thread *runningThread() const {
            return this->running;
        }

        /// schedules all runnable threads in the given task
        void scheduleRunnable(Task *task);
        /// adds the given thread to the runnable queue
        void markThreadAsRunnable(Thread *thread);

        /// Runs the scheduler.
        void run() __attribute__((noreturn));
        /// Yields the remainder of the current thread's CPU time.
        void yield();

    private:
        static void init();

        /// updates the current CPU's running thread
        void setRunningThread(Thread *t) {
            this->running = t;
        }

        const PriorityGroup groupForThread(Thread *t, const bool withBoost = false) const;

        void tickCallback(const uintptr_t irqToken);

        Scheduler();
        ~Scheduler();

        void adjustPriorities();
        void switchToRunnable(Thread *ignore = nullptr);

    private:
        static Scheduler *gShared;

    private:
        /// the thread that is currently being executed
        Thread *running = nullptr;
        /// timer we increment every tick to govern when we boost threads' priorities
        uintptr_t priorityAdjTimer = 0;

        /// lock for the runnable threads queue
        DECLARE_SPINLOCK(runnableLock);
        /// runnable threads (per priority band)
        rt::Queue<Thread *> runnable[kPriorityGroupMax];

        /// idle worker
        IdleWorker *idle = nullptr;
};
}

#endif
