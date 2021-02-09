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

bool UpdatePriorities(void *ctx, Thread *t);

/**
 * Scheduler is responsible for ensuring all runnable threads get CPU time. It does this by taking
 * threads from the head of each priority level's ready queue and running them until that queue is
 * empty, then continuing on to the queue of the next lowest priority, and so on.
 *
 * In essence, the scheduler implements a round-robin scheduling scheme inside priority bands, but
 * ensures that a higher priority task will ALWAYS receive CPU time before a lower priority one.
 *
 * Pre-emption works by having a tick handler that queues scheduler invocations and increments a
 * flag to indicate the number of ticks that took place. Likewise, unblocking threads will simply
 * add them to a queue and request a scheduler update.
 *
 * XXX: This will require a fair bit of re-architecting when we want to support multiprocessor
 *      machines. Probably making data structures shared is sufficient.
 */
class Scheduler {
    friend bool UpdatePriorities(void *, Thread *);
    friend void ::kernel_init();
    friend void ::platform_kern_tick(const uintptr_t);
    friend void ::platform_kern_scheduler_update(const uintptr_t);
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
        void markThreadAsRunnable(Thread *thread) {
            this->markThreadAsRunnable(thread, false);
        }

        /// Runs the scheduler.
        void run() __attribute__((noreturn));
        /// Yields the remainder of the current thread's CPU time.
        void yield() {
            yield(nullptr, nullptr);
        }

    private:
        static void init();

        // marks a thread as runnable, possibly by inserting it at the head of the run queue
        void markThreadAsRunnable(Thread *thread, const bool atHead, const bool immediate = false);

        /// updates the current CPU's running thread
        void setRunningThread(Thread *t) {
            this->running = t;
        }

        const PriorityGroup groupForThread(Thread *t, const bool withBoost = false) const;

        void tickCallback();

        Scheduler();
        ~Scheduler();

        void yield(void (*willSwitch)(void*), void *willSwitchCtx = nullptr);
        void requeueRunnable(Thread *);

        void adjustPriorities();
        void switchToRunnable(Thread *ignore = nullptr, bool requeueRunning = false,
                void (*willSwitch)(void*) = nullptr, void *willSwitchCtx = nullptr);
        bool handleBoostThread(Thread *thread);

        bool handleDeferredUpdates();
        void receivedDispatchIpi(const uintptr_t);

    private:
        /// Info on a thread that is runnable
        struct RunnableInfo {
            /// When set, the thread should be inserted at the head of the runnable queue
            bool atFront = false;
            /// Pointer to thread
            Thread *thread = nullptr;

            RunnableInfo() = default;
            RunnableInfo(Thread *_thread) : thread(_thread) {}
        };

    private:
        static Scheduler *gShared;

    private:
        /// the thread that is currently being executed
        Thread *running = nullptr;
        /// timer we increment every tick to govern when we boost threads' priorities
        uintptr_t priorityAdjTimer = 0;

        /// number of time ticks we have to handle for the next deferred update cycle
        uintptr_t ticksToHandle = 0;

        /// lock for the runnable threads queue
        DECLARE_SPINLOCK(runnableLock);
        /// runnable threads (per priority band)
        rt::Queue<Thread *> runnable[kPriorityGroupMax];

        /// lock protecting the below queueueuueueu
        DECLARE_SPINLOCK(newlyRunnableLock);
        /// queue of threads that have become runnable
        rt::Queue<RunnableInfo> newlyRunnable;

        /// idle worker
        IdleWorker *idle = nullptr;
};
}

#endif
