#ifndef KERN_SCHED_THREAD_H
#define KERN_SCHED_THREAD_H

#include <stddef.h>
#include <stdint.h>

#include <arch/thread.h>
#include <arch/rwlock.h>

namespace sched {
struct Task;

/**
 * Threads are the smallest units of execution in the kernel. They are the unit of work that the
 * scheduler concerns itself with.
 *
 * Each thread can either be ready to run, blocked, or paused. When the scheduler decides to run
 * the thread, its saved CPU state is loaded and the task executed. When the task returns to the
 * kernel (either through a syscall that blocks/context switches, or via its time quantum expiring)
 * its state is saved again.
 *
 * Depending on the nature of the thread's return to the kernel, it will be added back to the run
 * queue if it's ready to run again and not blocked. (This implies threads cannot change from
 * runnable to blocked if they're not currently executing.)
 */
struct Thread {
    /// Length of thread names
    constexpr static const size_t kNameLength = 32;

    public:
        enum class State {
            /// Thread can become runnable at any time, but only via an explicit API call
            Paused                      = 0,
            /// Thread requests to be scheduled as soon as possible
            Runnable                    = 1,
            /// Thread is waiting on some event to occur
            Blocked                     = 2,
            /// About to be destroyed; do not schedule or access.
            Zombie                      = 3,
        };

    public:
        /// Global thread id
        uint32_t tid = 0;
        /// task that owns us
        Task *task = nullptr;

        /// current thread state
        State state = State::Paused;
        /**
         * Determines whether the thread is run in the context of the kernel, or as an user-mode
         * thread. Platforms may treat userspace and kernel threads differently (from the
         * perspective of stacks, for example) but are not required to do so.
         *
         * The scheduler itself makes no core differentiation between user and kernel mode threads;
         * the only difference is that kernel threads may not make system calls.
         */
        bool kernelMode = false;

        /**
         * Priority of the thread; this should be a value between -100 and 100, with negative
         * values having the lowest priority. The scheduler internally converts this to whatever
         * priority system it uses.
         */
        int16_t priority = 0;
        /// Priority boost for this thread; incremented any time it's not scheduled
        int16_t priorityBoost = 0;

        // Number of ticks that the thread's quantum is in length. Ticks are usually 1ms.
        uint16_t quantumTicks = 10;
        // Ticks left in the thread's current quantum
        uint16_t quantum = 0;

        /// descriptive thread name, if desired
        char name[kNameLength] = {0};

        /**
         * The thread can be accessed read-only by multiple processes, but the scheduler will
         * always require write access, in order to be able to save processor state later.
         */
        DECLARE_RWLOCK(lock);

        /// bottom of the kernel stack of this thread
        void *stack = nullptr;

        /// architecture-specific thread state
        arch::ThreadState regs;

    public:
        /// Allocates a new kernel space thread
        static Thread *kernelThread(Task *task, void (*entry)(uintptr_t), const uintptr_t param);
        /// Releases a previously allocated thread struct
        static void free(Thread *);

        Thread(Task *task, const uintptr_t pc, const uintptr_t param,
                const bool kernelThread = true);
        ~Thread();

        /// Context switches to this thread.
        void switchTo();

        /// Sets the thread's name.
        void setName(const char *name);
        /// Sets the thread's state.
        void setState(const State newState) {
            RW_LOCK_WRITE_GUARD(this->lock);
            this->state = newState;
        }

        /// Give up the rest of this thread's CPU time
        static void yield();
        /// Terminates the thread
        static void terminate() __attribute__((noreturn));

    private:
        /// next thread id
        static uint32_t nextTid;

    private:
        static void initAllocator();
};
}

#endif
