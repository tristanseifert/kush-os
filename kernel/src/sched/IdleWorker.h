#ifndef KERNEL_SCHED_IDLETHREAD_H
#define KERNEL_SCHED_IDLETHREAD_H

#include <stdint.h>

#include <arch/spinlock.h>
#include <runtime/Queue.h>

namespace sched {
void IdleEntry(uintptr_t arg);

class Scheduler;
struct Thread;

/**
 * The idle worker handles tasks such as deleting threads/tasks when they're no longer needed,
 * opportunistically zeroing memory pages, and other such background work.
 *
 * Basically, this is a thread at the lowest priority level that's always ready to run.
 */
class IdleWorker {
    friend void IdleEntry(uintptr_t);

    private:
        /// Idle thread priority; lowest in the system
        constexpr static const int kThreadPriority = -100;

        /// Type of work request (determines the precise handler to use)
        enum class Type {
            Unknown,

            /**
             * Deallocate the given thread. The payload pointer is a pointer to a `struct Thread`
             * that must be deallocated using the thread allocator.
             */
            DestroyThread,
        };
        /// Work unit pushed to the idle thread
        struct WorkItem {
            /// Type of work
            Type type = Type::Unknown;
            /// pointer to payload
            void *payload = nullptr;

            WorkItem() = default;
            WorkItem(Type _type) : type(_type), payload(nullptr) {}
            WorkItem(Type _type, void *_payload) : type(_type), payload(_payload) {}

            static WorkItem destroyThread(Thread *t) {
                return WorkItem(Type::DestroyThread, t);
            }
        };

    public:
        IdleWorker(Scheduler *);
        ~IdleWorker();

        /// Queues the given thread for deletion
        void queueDestroyThread(Thread *thread) {
            SPIN_LOCK_GUARD(this->workLock);
            this->work.push(WorkItem::destroyThread(thread));
        }

    private:
        void main();
        void checkWork();

        void destroyThread(Thread *);

    private:
        /// scheduler that owns us
        Scheduler *sched = nullptr;
        /// actual worker thread
        Thread *thread = nullptr;

        /// work queue
        rt::Queue<WorkItem> work;
        /// lock protecting the queue
        DECLARE_SPINLOCK(workLock);
};
}

#endif
