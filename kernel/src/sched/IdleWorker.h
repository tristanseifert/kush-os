#ifndef KERNEL_SCHED_IDLETHREAD_H
#define KERNEL_SCHED_IDLETHREAD_H

#include <stdint.h>

#include <log.h>
#include <runtime/SmartPointers.h>
#include <runtime/LockFreeQueue.h>

namespace sched {
void IdleEntry(uintptr_t arg);

class Scheduler;
struct Thread;
struct Task;

/**
 * The idle worker handles tasks such as deleting threads/tasks when they're no longer needed,
 * opportunistically zeroing memory pages, and other such background work.
 *
 * Basically, this is a thread at the lowest priority level that's always ready to run.
 */
class IdleWorker {
    friend class Scheduler;
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

            /**
             * Deallocate the given task. The payload pointer is to a `struct Task` that we must
             * release.
             */
            DestroyTask,
        };
        /// Work unit pushed to the idle thread
        struct WorkItem {
            /// Type of work
            Type type = Type::Unknown;

            WorkItem() = default;
            WorkItem(Type _type) : type(_type) {}
            virtual ~WorkItem() = default;

            /// invoked to actually do the task
            virtual void operator()() = 0;
        };

        /// Work item for deleting a thread
        struct DeleteThreadItem: public WorkItem {
            DeleteThreadItem(const rt::SharedPtr<Thread> &_thread) : WorkItem(Type::DestroyThread),
                thread(_thread) {}

            void operator()() override;

            /// thread to be destroyed
            rt::SharedPtr<Thread> thread;
            /// whether thread deletion is logged
            static const bool gLog = true;
        };
        /// Work item for deleting a task
        struct DeleteTaskItem: public WorkItem {
            DeleteTaskItem(const rt::SharedPtr<Task> &_task) : WorkItem(Type::DestroyTask),
                task(_task) {}

            void operator()() override;

            /// thread to be destroyed
            rt::SharedPtr<Task> task;
            /// whether task deletion is logged
            static const bool gLog = true;
        };

    public:
        IdleWorker(Scheduler *);
        ~IdleWorker() = default;

        /// Queues the given thread for deletion
        void queueDestroyThread(const rt::SharedPtr<Thread> &thread) {
            auto info = new DeleteThreadItem(thread);
            REQUIRE(info, "failed to allocate %s", "destroy thread message");

            const auto inserted = this->work.insert(info);
            REQUIRE(inserted, "failed to insert %s", "destroy thread message");
        }

        /// Queues the given task for deletion
        void queueDestroyTask(const rt::SharedPtr<Task> &task) {
            auto info = new DeleteTaskItem(task);
            REQUIRE(info, "failed to allocate %s", "destroy task message");

            const auto inserted = this->work.insert(info);
            REQUIRE(inserted, "failed to insert %s", "destroy task message");
        }

    private:
        void main();
        void checkWork();

        void destroyThread(Thread *);
        void destroyTask(Task *);

    private:
    public:
        /// scheduler that owns us
        Scheduler *sched = nullptr;
        /// actual worker thread
        rt::SharedPtr<Thread> thread;

        /// work queue
        rt::LockFreeQueue<WorkItem *> work;
};
}

#endif
