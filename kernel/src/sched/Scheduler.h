#ifndef KERNEL_SCHED_SCHEDULER_H
#define KERNEL_SCHED_SCHEDULER_H

#include <stdint.h>

#include <platform.h>
#include <arch/spinlock.h>
#include <runtime/Queue.h>

#include "Thread.h"

extern "C" void kernel_init();

namespace sched {
class IdleWorker;
struct Task;

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
        void yield(const Thread::State state = Thread::State::Runnable) {
            yield(nullptr, nullptr);
        }

        void registerTask(Task *task);
        void unregisterTask(Task *task);
        void iterateTasks(void (*callback)(Task *));

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

        void yield(const platform::Irql restoreIrql);
        void yield(void (*willSwitch)(void*), void *willSwitchCtx = nullptr);

        bool switchToRunnable(Thread *ignore = nullptr, bool requeueRunning = false,
                void (*willSwitch)(void*) = nullptr, void *willSwitchCtx = nullptr);

        bool handleDeferredUpdates();
        void receivedDispatchIpi(const uintptr_t);

        void removeThreadFromRunQueue(Thread *);

    private:
        /// Info on a thread that is runnable
        struct RunnableInfo {
            /// When set, the thread should be inserted at the head of the runnable queue
            bool atFront = false;
            /// Pointer to thread
            Thread *thread = nullptr;

            RunnableInfo() = default;
            RunnableInfo(Thread *_thread) : atFront(false), thread(_thread) {}
            RunnableInfo(Thread *_thread, const bool _atFront) : atFront(_atFront), 
                thread(_thread) {}
        };

    private:
        static Scheduler *gShared;

    private:
        /// the thread that is currently being executed
        Thread *running = nullptr;

        /// number of time ticks we have to handle for the next deferred update cycle
        uintptr_t ticksToHandle = 0;

        /// runnable threads (per priority band)
        rt::Queue<Thread *> runnable[kPriorityGroupMax];
        /// queue of threads that have become runnable
        rt::Queue<RunnableInfo> newlyRunnable;

        /// all active tasks
        rt::List<Task *> tasks;

    public:
        /// idle worker
        IdleWorker *idle = nullptr;
};
}

#endif
