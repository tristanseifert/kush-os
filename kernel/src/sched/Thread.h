#ifndef KERN_SCHED_THREAD_H
#define KERN_SCHED_THREAD_H

#include <stddef.h>
#include <stdint.h>

#include <runtime/List.h>
#include <runtime/Queue.h>
#include <runtime/Vector.h>
#include <runtime/SmartPointers.h>
#include <handle/Manager.h>

#include <arch/ThreadState.h>
#include <arch/rwlock.h>

#include "SchedulerData.h"

namespace ipc {
class IrqHandler;
}

namespace sched {
class Blockable;
class SignalFlag;
struct Task;
struct BlockWait;

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
struct Thread: public rt::SharedFromThis<Thread> {
    friend class Scheduler;
    friend class Blockable;
    friend class IdleWorker;
    friend struct BlockWait;

    /// Length of thread names
    constexpr static const size_t kNameLength = 32;

    private:
        /// Info on a DPC to execute
        struct DpcInfo {
            void (*handler)(Thread *, void *);
            void *context = nullptr;
        };
        /// why a block returned
        enum class BlockState {
            None                        = 0,
            /// Currently blocking
            Blocking                    = 1,
            /// The blocking condition(s) were signalled
            Unblocked                   = 2,
            /// The block was timed, and the timeout has expired
            Timeout                     = 3,
            /// One of the blockables aborted the attempt to go to sleep
            Aborted                     = 4,
        };

    public:
        enum class State {
            /// Thread can become runnable at any time, but only via an explicit API call
            Paused                      = 0,
            /// Thread requests to be scheduled as soon as possible
            Runnable                    = 1,
            /// Thread is waiting on some event to occur
            Blocked                     = 2,
            /// Blocked on sleep (may end early)
            Sleeping                    = 3,
            /// Waiting for notification
            NotifyWait                  = 4,
            /// About to be destroyed; do not schedule or access.
            Zombie                      = 5,
        };

        /// return codes for blockOn
        enum class BlockOnReturn {
            /// Unknown error
            Error                       = -1,
            /// thread unblocked
            Unblocked                   = 0,
            /// block timed out
            Timeout                     = 1,
            /// block aborted
            Aborted                     = 2,
        };

        /**
         * Defines the types of faults that a thread may receive. These are synchronous-ish events
         * generated by the processor, indicating events such as an illegal opcode, or invalid math
         * operation.
         */
        enum class FaultType {
            /// General fault; this always terminates the task this thread is in
            General                     = 0,
            /// The instruction executed is invalid; context is pointer to the pc in exc frame
            InvalidInstruction          = 1,
            /// A page fault was unhandled (pc = faulting address; context = pc exception frame)
            UnhandledPagefault          = 2,
            /// Protection violation
            ProtectionViolation         = 3,
        };

    public:
        /// Global thread id
        uintptr_t tid = 0;
        /// task that owns us
        rt::SharedPtr<Task> task = nullptr;
        /// when set, we're attached to the given task
        bool attachedToTask = false;

        /// handle to the thread
        Handle handle;

        /// descriptive thread name, if desired
        char name[kNameLength] = {0};

        /// current thread state
        State state = State::Paused;
        /**
         * Threads marked as kernel mode are treated a bit specially by the scheduler, in that
         * only kernel threads may be placed in the highest priority run queues.
         */
        bool kernelMode = false;

        /**
         * Flag set when the scheduler has assigned the thread to a processor and it is executing;
         * it will be cleared immediately after the thread is switched out.
         *
         * This flag is the responsibility of the arch context switching code.
         */
        bool isActive = false;
        /// when set, this thread should kill itself when switched out
        bool needsToDie = false;
        /// Timestamp at which the thread was switched to
        uint64_t lastSwitchedTo = 0;

        /// number of the last syscall this thread performed
        uintptr_t lastSyscall = UINTPTR_MAX;

        /// scheduler data
        SchedulerThreadData sched;

        /**
         * Priority of the thread; this should be a value between -100 and 100, with negative
         * values having the lowest priority. The scheduler internally converts this to whatever
         * priority system it uses.
         */
        int16_t priority = 0;

        /**
         * Notification value and mask for the thread.
         *
         * Notifications are an asynchronous signalling mechanism that can be used to signal a
         * thread that a paritcular event occurred, without any additional auxiliary information;
         * this makes it ideal for things like interrupt handlers.
         *
         * Each thread defines a notification mask, which indicates on which bits (set) of the
         * notification set the thread is interested in; when the notification mask is updated, it
         * is compared against the mask, and if any bits are set, the thread can be unblocked.
         *
         * The notifications flag is set whenever a thread is blocking on notification bits. It
         * should be signalled whenever a notification is received that activates the thread.
         */
        uintptr_t notifications = 0;
        uintptr_t notificationMask = 0;
        bool notified = false;
        rt::SharedPtr <SignalFlag> notificationsFlag;

        /**
         * Objects this thread is currently blocking on
         */
        rt::List<rt::SharedPtr<Blockable>> blockingOn;

        /// Interrupt handlers owned by the thread; they're removed when we deallocate
        rt::List<rt::SharedPtr<ipc::IrqHandler>> irqHandlers;

        /**
         * The thread can be accessed read-only by multiple processes, but the scheduler will
         * always require write access, in order to be able to save processor state later.
         */
        DECLARE_RWLOCK(lock);

        /// notification flags to signal when terminating
        rt::List<rt::SharedPtr<SignalFlag>> terminateSignals;

        /// DPCs queued for the thread
        rt::Queue<DpcInfo> dpcs;
        /// When set, there are DPCs pending.
        bool dpcsPending = false;

        /// size of the thread's kernel stack (in bytes)
        size_t stackSize = 0;
        /// bottom of the kernel stack of this thread
        void *stack = nullptr;

        /// architecture-specific thread state
        arch::ThreadState regs __attribute__((aligned(16)));

    private:
        /// whether we're blocking and why the block finished
        BlockState blockState = BlockState::None;

    public:
        /// Allocates a new kernel thread
        static rt::SharedPtr<Thread> kernelThread(rt::SharedPtr<Task> &task,
                void (*entry)(uintptr_t), const uintptr_t param = 0);
        /// Allocates a new userspace thread
        static rt::SharedPtr<Thread> userThread(rt::SharedPtr<Task> &task,
                void (*entry)(uintptr_t), const uintptr_t param = 0);

        Thread(rt::SharedPtr<Task> &task, const uintptr_t pc, const uintptr_t param,
                const bool kernelThread = false);
        ~Thread();

        /// Context switches to this thread.
        void switchTo();
        /// Returns to user mode, with the specified program counter and stack.
        void returnToUser(const uintptr_t pc, const uintptr_t stack, const uintptr_t arg = 0) __attribute__((noreturn));

        /// Updates the notification mask; set bits are unmasked (i.e. will occur)
        inline void setNotificationMask(uintptr_t newMask) {
            __atomic_store(&this->notificationMask, &newMask, __ATOMIC_RELEASE);
        }

        /// Return thread handle
        constexpr auto getHandle() const {
            return this->handle;
        }

        /// Sets the thread's name.
        void setName(const char *name, const size_t length = 0);
        /// Sets the thread's scheduling priority
        inline void setPriority(int16_t priority) {
            __atomic_store(&this->priority, &priority, __ATOMIC_RELEASE);
        }
        /// Sets the thread's state.
        void setState(State newState) {
            if(this->state == State::Blocked && newState == State::Runnable) {
                REQUIRE(this->blockingOn.empty(), "cannot be runnable while blocking");
            }

            __atomic_store(&this->state, &newState, __ATOMIC_RELEASE);
        }
        /// Atomically reads the current state.
        const State getState() const {
            State s;
            __atomic_load(&this->state, &s, __ATOMIC_RELAXED);
            return s;
        }

        /// Blocks the thread on the given object.
        BlockOnReturn blockOn(const rt::SharedPtr<Blockable> &b, const uint64_t until = 0);
        /// Unblocks the thread due to the given blockable
        void unblock(const rt::SharedPtr<Blockable> &b);

        /// Sets the given notification bits.
        void notify(const uintptr_t bits);
        /// Blocks the thread waiting to receive notifications, optionally with a mask.
        uintptr_t blockNotify(const uintptr_t mask = 0);

        /// Adds a DPC to the thread's queue.
        int addDpc(void (*handler)(Thread *, void *), void *context = nullptr);
        /// Drains the DPC queue.
        void runDpcs();

        /// Waits for the thread to be terminated.
        int waitOn(const uint64_t waitUntil = 0);
        /// Terminates this thread.
        void terminate(bool release = true);

        /// Handles a fault of the given type. Called from fault handlers.
        void handleFault(const FaultType type, const uintptr_t pc, void *context, const void *arch);

        /// Returns a handle to the currently executing thread.
        static rt::SharedPtr<Thread> current();
        /// Blocks the current thread for the given number of nanoseconds.
        static void sleep(const uint64_t nanos);
        /// Give up the rest of this thread's CPU time
        static void yield();
        /// Terminates the calling thread
        static void die() __attribute__((noreturn));

    private:
        /// next thread id
        static uintptr_t nextTid;

        /// whether creation/deallocation of threads is logged
        static bool gLogLifecycle;

    private:
        /// Called when this thread is switching out
        void switchFrom();

        /// Called on context switch out to complete termination.
        void deferredTerminate();
        /// Invoke all termination handlers
        void callTerminators();

        /// scheduler is attempting to determine if thread should be runnable again
        void schedTestUnblock();
        /// any pending blocks should be cancelled
        void blockExpired();
        /// set thread state with optionally idsabling validation
        void setState(State newState, const bool validate) {
            if(validate) {
                return this->setState(newState);
            }

            __atomic_store(&this->state, &newState, __ATOMIC_RELEASE);
        }
};
}

#endif
