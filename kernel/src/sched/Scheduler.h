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
 * Provides multitasking abilities for multiple cores. Each scheduler handles the workload for a
 * particular core, optionally stealing work from other cores if it becomes idle.
 *
 * Internally, the scheduler can be described as a basic multi-level feedback queue. Each level of
 * the queue has its own scheduling properties (quantum length and quantum expiration or sleep
 * return levels) that are applied.
 *
 * Additionally, there are global variables, such as the interval of periodic priority boosts, and
 * the default priority of new threads.
 *
 * Threads will always be preempted at the latest when the time quantum finishes up. Before we
 * switch to a new thread, the timer is configured to fire either at the end of the time quantum,
 * or when a registered timer expires.
 */
class Scheduler {
    friend void ::kernel_init();
    friend void ::platform_kern_tick(const uintptr_t);
    friend void ::platform_kern_scheduler_update(const uintptr_t);
    friend struct Thread;

    public:
        // return the shared scheduler instance
        static Scheduler *get();

        // return the thread running on the current processor
        Thread *runningThread() const {
            return this->running;
        }

        /// schedules all runnable threads in the given task
        void scheduleRunnable(Task *task);
        /// adds the given thread to the runnable queue
        bool markThreadAsRunnable(Thread *thread, const bool shouldSwitch = true);

        /// Runs the scheduler.
        void run() __attribute__((noreturn));
        /// Yields the remainder of the current thread's CPU time.
        void yield(const Thread::State state = Thread::State::Runnable);

        /// If the run queue has been dirtied, send a scheduler IPI
        void lazyDispatch();

    private:
        static void Init();
        static void InitAp();

        /// updates the current CPU's running thread
        void setRunningThread(Thread *t) {
            this->running = t;
        }

        Scheduler();
        ~Scheduler();

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
        /// the thread that is currently being executed
        Thread *running = nullptr;

        /**
         * Whether the run queue has been modified. This is set whenever a new thread is made
         * runnable. 
         *
         * This is used to perform a scheduler dispatch as needded at the end of interrupt
         * routines. That way, the ISRs can just unconditionally call the `LazyDispatch()` method
         * and we'll send a dispatch IPI if the run queue has changed.
         *
         * We'll clear this flag when we send an IPI, and when we otherwise take a trip into the
         * dispatch machinery. (This covers the case where a thread is unblocked, but the caller's
         * time quantum expires before a dispatch IPI can be generated.)
         */
        bool isDirty = false;

    public:
        /// idle worker
        IdleWorker *idle = nullptr;
};
}

#endif
