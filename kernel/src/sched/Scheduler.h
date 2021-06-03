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
#include "PeerList.h"

extern "C" void kernel_init();

namespace platform {
class ApicTimer;
}

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
    friend struct Thread;
    friend struct SleepDeadline;
    friend class IdleWorker;
    friend void PeerList::invalidateOthers();

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

        /// Default positive slack for deadlines (in ns)
        constexpr static const uint64_t kDeadlineSlack = 500;

    public:
        // return the scheduler for the current core
        static Scheduler *get();
        // initializes the scheduler data in the given thread object
        static void threadWasCreated(Thread &t);

        /// Returns this scheduler's core ID
        constexpr auto getCoreId() const {
            return this->coreId;
        }
        // return the thread running on the calling processor
        rt::SharedPtr<Thread> runningThread() const {
            return this->running;
        }

        /// adds the given thread to the runnable queue
        int markThreadAsRunnable(const rt::SharedPtr<Thread> &thread,
                const bool shouldSwitch = true);

        /// Runs the scheduler.
        void run() __attribute__((noreturn));
        /// Yields the remainder of the current thread's CPU time.
        void yield();

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
        /// Updates the time quantum used by the given thread
        bool updateQuantumUsed(const rt::SharedPtr<Thread> &thread);
        /// Calculate the total quantum length for a given thread
        void updateQuantumLength(const rt::SharedPtr<Thread> &thread);

        /// Updates the period of the scheduler timer
        void timerUpdate();
        /// Enqueue a scheduler IPI
        void sendIpi();

        /// updates the current CPU's running thread (from thread code)
        inline void setRunningThread(const rt::SharedPtr<Thread> &t) {
            this->running = t;
        }

        /// The given thread was unblocked
        void threadUnblocked(const rt::SharedPtr<Thread> &t);
        /// Schedules all valid unblocked threads
        void processUnblockedThreads();

        /// Processes any expired deadlines
        bool processDeadlines();

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
                    thread->sched.queuePushed++;
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

    private:
        /// whether pops/pushes to queue are logged
        constexpr static const bool kLogQueueOps = false;
        /// whether time quantum updates are logged
        constexpr static const bool kLogQuantum = false;
        /// whether deadline operations are logged
        constexpr static const bool kLogDeadlines = false;

        /// per level configuration
        static LevelInfo gLevelInfo[kNumLevels];

        /// the thread that is currently being executed
        rt::SharedPtr<Thread> running;
        /// Platform specific core ID to which this scheduler belongs
        uintptr_t coreId;

        /// timer for tracking CPU usage (how much to yeet quantum by)
        Oclock timer;

        /// list of other schedulers, sorted in ascending distance
        PeerList peers;

        /// Each of the levels that may contain a thread to run
        Level levels[kNumLevels];

        /// Unblocked, potentially runnable threads
        rt::LockFreeQueue<rt::SharedPtr<Thread>> unblocked;

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
         * If an event (thread preemption or deadline) occurs within this time period (in ns) the
         * scheduler will take an IPI directly rather than setting the core local timer.
         *
         * This value really needs to be tweaked so the scheduler doesn't get random "freezes"
         * where all activity in the system seems to stop. This is mostly an issue on
         * virtualized environments where the timer's resolution is rather shit, and it's
         * totally possible for us to get in the timer setup function with some whacky values
         * that cause a lost timer interrupt.
         */
        uint64_t timerMinInterval = 50000;

        /**
         * Level from which the currently executing thread was pulled, or `kNumLevels` if we're
         * executing the idle thread.
         */
        size_t currentLevel = kNumLevels;

        /**
         * This field is updated with the highest (lowest numeric value) priority of a thread that
         * is added to the run queue.
         *
         * Used to determine whether any higher priority threads are scheduled and whether an IPI
         * is required in response to timer events.
         */
        size_t maxScheduledLevel = kNumLevels;


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
