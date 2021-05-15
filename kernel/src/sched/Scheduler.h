#ifndef KERNEL_SCHED_SCHEDULER_H
#define KERNEL_SCHED_SCHEDULER_H

#include <stdint.h>

#include <platform.h>
#include <arch/spinlock.h>
#include <runtime/Queue.h>
#include <runtime/LockFreeQueue.h>

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
        /**
         * Total number of levels the scheduler is configured for.
         *
         * Each level has its own run queue, which can be individually locked (to optimize the
         * case where other cores steal work from us) as we try to find work.
         */
        constexpr static const size_t kNumLevels = 32;

    public:
        // return the scheduler for the current core
        static Scheduler *get();

        // return the thread running on the calling processor
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

        /// Check if a higher priority thread is runnable and prepare to switch to it
        bool lazyDispatch();

    private:
        static void Init();
        static void InitAp();

        /// Finds the highest priority runnable thread on this core
        Thread *findRunnableThread();

        /// updates the current CPU's running thread
        void setRunningThread(Thread *t) {
            this->running = t;
        }

        /// invalidates all schedulers' peer lists
        static void invalidateAllPeerList(Scheduler *skip = nullptr);
        /// invalidates the peer list of this scheduler
        inline void invalidatePeerList() {
            __atomic_store_n(&this->peersDirty, true, __ATOMIC_RELEASE);
        }
        /// rebuilds the peer map
        void buildPeerList();

        Scheduler();
        ~Scheduler();

    private:
        /**
         * Each level contains a lock free FIFO queue that contains the list of threads that are
         * runnable. This mechanism supports multiple producers and consumers, allowing for low
         * overhead work stealing when a core becomes idle.
         *
         * Additionally, some metadata and timestamps (in core local time) are stored that allow
         * the scheduler to prevent resource starvation.
         */
        struct Level {
            public:
                /// Queue of runnable threads in this level
                rt::LockFreeQueue<Thread *> storage;

                /// Last time a thread from this level was scheduled
                uint64_t lastScheduledTsc = 0;
        };

        /**
         * Describes information on a particular core's scheduler instance. This consists of a
         * pointer to its scheduler instance as well as some core identification information.
         *
         * This is primarily accessed by idle cores when looking for work to steal. Each core's
         * local scheduler will build a list of cores to steal from, in ascending order of some
         * platform defined "cost" of migrating a thread off of that source core. This allows us
         * to be aware of things like cache structures, SMP, and so forth.
         */
        struct InstanceInfo {
            /// core ID (platform specific)
            uintptr_t coreId = 0;
            /// scheduler running on this core
            Scheduler *instance = nullptr;

            InstanceInfo() = default;
            InstanceInfo(Scheduler *_instance) : instance(_instance) {}
        };

    private:
        /// all schedulers on the system. used for work stealing
        static rt::Vector<InstanceInfo> *gSchedulers;

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

        /// Each of the levels that may contain a thread to run
        Level levels[kNumLevels];

        /**
         * Epoch for the run queues. This is incremented any time the levels' queues are modified,
         * and is used when selecting a runnable thread so that if a queue was updated during a
         * run queue scan, we'll restart the scan if no results were found instead of scheduling
         * the idle thread.
         *
         * There is no actual meaning behind the value other than it is a monotonically increasing
         * value.
         */
        uint64_t levelEpoch = 0;
        /**
         * Maximum number of times to repeat the search for a runnable thread if we detect that
         * another core modified our run queues.
         */
        size_t levelChangeMaxLoops = 5;

        /**
         * List of other cores' schedulers, ordered by ascending cost of migrating a thread from
         * that core. This list is used when looking for work to steal when we become idle.
         *
         * The list is built lazily when we're idle and the dirty flag is set.
         */
        rt::Vector<InstanceInfo> peers;
        /// when set, the peer map is dirty and must be updated
        bool peersDirty = true;

    public:
        /// idle worker
        IdleWorker *idle = nullptr;
};
}

#endif
