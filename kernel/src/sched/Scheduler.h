#ifndef KERNEL_SCHED_SCHEDULER_H
#define KERNEL_SCHED_SCHEDULER_H

#include <stdint.h>

#include <bitflags.h>
#include <arch/rwlock.h>
#include <runtime/MinHeap.h>
#include <runtime/LockFreeQueue.h>
#include <runtime/SmartPointers.h>

#include "Deadline.h"
#include "Thread.h"
#include "Oclock.h"

extern "C" void kernel_init();

namespace platform {
class ApicTimer;
}

namespace sched {
class IdleWorker;
struct Task;

/**
 * Bitfield indicating what work a scheduler IPI has to do
 */
ENUM_FLAGS_EX(IpiWorkFlags, uintptr_t);
enum class IpiWorkFlags: uintptr_t {
    /// No work to be done (default value)
    None                        = 0,

    /// The current thread should be preempted (quantum expired)
    QuantumExpired              = (1 << 0),
    /// Work has been enqueued on the scheduler's queue from another core
    WorkGifted                  = (1 << 1),
};

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
    friend struct Thread;
    friend struct SleepDeadline;

    public:
        /**
         * Total number of levels the scheduler is configured for.
         *
         * Each level has its own run queue, which can be individually locked (to optimize the
         * case where other cores steal work from us) as we try to find work.
         */
        constexpr static const size_t kNumLevels = 32;

        /// First run queue for user threads
        constexpr static const size_t kUserPriorityLevel = 4;

        /**
         * Interval at which the scheduler will wake the core to check for new work if it enters
         * the idle state, in nanoseconds.
         */
        constexpr static const uintptr_t kIdleWakeupInterval = (1000000 * 100); // 100ms

        /// Default positive slack for deadlines
        constexpr static const uint64_t kDeadlineSlack = 500;

    public:
        // return the scheduler for the current core
        static Scheduler *get();
        // initializes the scheduler data in the given thread object
        static void threadWasCreated(Thread &t);

        // return the thread running on the calling processor
        rt::SharedPtr<Thread> runningThread() const {
            return this->running;
        }

        /// schedules all runnable threads in the given task
        void scheduleRunnable(rt::SharedPtr<Task> &task);
        /// adds the given thread to the runnable queue
        int markThreadAsRunnable(const rt::SharedPtr<Thread> &thread,
                const bool shouldSwitch = true);

        /// Runs the scheduler.
        void run() __attribute__((noreturn));
        /// Yields the remainder of the current thread's CPU time.
        void yield(const Thread::State state = Thread::State::Runnable);

        /// The specified thread is leaving the processor
        void willSwitchFrom(const rt::SharedPtr<Thread> &from);
        /// The specified thread is about to be switched in
        void willSwitchTo(const rt::SharedPtr<Thread> &to);

        /// Core local timer has expired
        void timerFired();
        /// Scheduler IPI fired
        void handleIpi(void (*ackIrq)(void *), void *ackCtx);

        /// Adds a deadline to the scheduler
        void addDeadline(const rt::SharedPtr<Deadline> &deadline);
        /// Removes an existing deadline, if it hasn't expired yet
        bool removeDeadline(const rt::SharedPtr<Deadline> &deadline);

    private:
        static void Init();
        static void InitAp();

        /// Finds the highest priority runnable thread on this core
        rt::SharedPtr<Thread> findRunnableThread();
        /// Inserts the given thread into the appropriate run queue
        int schedule(const rt::SharedPtr<Thread> &thread);
        /// Returns the run queue level to which the given thread belongs
        size_t getLevelFor(const rt::SharedPtr<Thread> &thread);
        /// Calculate the total quantum length for a given thread
        void updateQuantumLength(const rt::SharedPtr<Thread> &thread);

        /// Enqueue a scheduler IPI
        void sendIpi();
        /// Updates the time quantum used by the given thread
        bool updateQuantumUsed(const rt::SharedPtr<Thread> &thread);

        /// Updates the scheduler timer for the next deadline
        void updateTimer();
        /// Switch to the given thread
        void switchTo(const rt::SharedPtr<Thread> &thread, const bool fake = false);

        /// updates the current CPU's running thread
        void setRunningThread(const rt::SharedPtr<Thread> &t) {
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
                rt::LockFreeQueue<rt::SharedPtr<Thread>> storage;

                /// Last time a thread from this level was scheduled
                uint64_t lastScheduledTsc = 0;

            public:
                /// Push a new thread into the run queue
                bool push(const rt::SharedPtr<Thread> &thread) {
                    return (this->storage.insert(thread) != 0);
                }
        };

        /**
         * Information about the scheduler properties of a particular level. These are shared by
         * all scheduler instances, and are reasonably static.
         */
        struct LevelInfo {
            /// length of this level's quantum, in nanoseconds
            uint64_t quantumLength = 0;
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

        /**
         * Information consumed during a scheduler IPI to determine what work is to be done
         *
         * This is a separate structure, allocated separately, so that it may be aligned to the
         * size of a cache line. This decreases the overhead of sharing it between cores, and also
         * avoids false sharing of the regular scheduler data structure.
         */
        struct IpiInfo {
            /// What work is there to be done?
            IpiWorkFlags work;
        } __attribute__((aligned(64)));

        /**
         * Small wrapper around a polymorphic Deadline shared pointer, to provide the appropriate
         * operators to make it work in the minheap
         */
        struct DeadlineWrapper {
            DeadlineWrapper() = default;
            DeadlineWrapper(const rt::SharedPtr<Deadline> &_deadline) : deadline(_deadline) {}

            /// deadline object we represent
            rt::SharedPtr<Deadline> deadline;

            /// get expiration time or 0 if invalid
            inline uint64_t expires() const {
                if(!deadline) return 0;
                return deadline->expires;
            }

            /// call through to the object
            inline void operator()() {
                (*this->deadline)();
            }

            bool operator ==(const DeadlineWrapper &d) const {
                return this->expires() == d.expires();
            }
            bool operator !=(const DeadlineWrapper &d) const {
                return this->expires() != d.expires();
            }
            bool operator <(const DeadlineWrapper &d) const {
                return this->expires() < d.expires();
            }
            bool operator <=(const DeadlineWrapper &d) const {
                return this->expires() <= d.expires();
            }
            bool operator >(const DeadlineWrapper &d) const {
                return this->expires() > d.expires();
            }
            bool operator >=(const DeadlineWrapper &d) const {
                return this->expires() >= d.expires();
            }
        };

        /// Reasons why we received a timer interrupt
        enum class TimerReason {
            /// There is no scheduled timer interrupt
            None,
            /// Current thread's time quantum expired
            QuantumExpired,
            /// A deadline has arrived
            Deadline,
        };

    private:
        /// whether pops/pushes to queue are logged
        constexpr static const bool kLogQueueOps = false;
        /// whether time quantum updates are logged
        constexpr static const bool kLogQuantum = false;
        /// whether deadline operations are logged
        constexpr static const bool kLogDeadlines = false;

        /// all schedulers on the system. used for work stealing
        static rt::Vector<InstanceInfo> *gSchedulers;
        /// per level configuration
        static LevelInfo gLevelInfo[kNumLevels];

        /// the thread that is currently being executed
        rt::SharedPtr<Thread> running = nullptr;

        /// timer for tracking CPU usage (how much to yeet quantum by)
        Oclock timer;
        /// what was the intent behind the last timer interrupt we set?
        TimerReason timerReason = TimerReason::None;

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
         * When a deadline is evaluated, it may lie up to this amount of time in the future and
         * still be considered expired, in nanoseconds. Higher values will reduce the number of
         * scheduler invocations at the cost of timing resolution.
         */
        uint64_t deadlineSlack = kDeadlineSlack;

        /**
         * Level from which the currently executing thread was pulled, or `kNumLevels` if we're
         * executing the idle thread.
         */
        size_t currentLevel = kNumLevels;

        /**
         * List of other cores' schedulers, ordered by ascending cost of migrating a thread from
         * that core. This list is used when looking for work to steal when we become idle.
         *
         * The list is built lazily when we're idle and the dirty flag is set.
         */
        rt::Vector<InstanceInfo> peers;
        /// when set, the peer map is dirty and must be updated
        bool peersDirty = true;

        /// Platform specific core ID to which this scheduler belongs
        uintptr_t coreId;
        /// IPI information structure
        IpiInfo *ipi = nullptr;

        /// lock for the deadline objects
        DECLARE_RWLOCK(deadlinesLock);
        /**
         * All upcoming deadlines for this core; they are processed at every scheduler invocation
         * if they've passed or are about to occur (to avoid some overhead)
         */
        rt::MinHeap<DeadlineWrapper> deadlines;

    public:
        /// idle worker
        IdleWorker *idle = nullptr;
};
}

#endif
