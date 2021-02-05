#ifndef KERNEL_SCHED_TASK_H
#define KERNEL_SCHED_TASK_H

#include <stddef.h>
#include <stdint.h>

#include <arch/rwlock.h>

#include "runtime/Vector.h"


namespace vm {
class Map;
}

namespace sched {
class Scheduler;
struct Thread;

/**
 * Tasks are the basic 
 */
struct Task {
    friend class Scheduler;

    /// Length of process names
    constexpr static const size_t kNameLength = 32;

    public:
        /// Current state of a task
        enum class State {
            /**
             * Initialization work is taking place on the task.
             */
            Initializing                = 0,
            /**
             * The task is runnable (e.g. contains at least one thread) and can be scheduled; this
             * does not mean that there are runnbale threads, however.
             */
            Runnable                    = 1,
            /**
             * The task should not be scheduled; it will go away soon.
             */
            Zombie                      = 2,
        };

    public:
        /// Process ID
        uint32_t pid = 0;
        /// Process name
        char name[kNameLength] = {0};

        /// current task state
        State state = State::Initializing;

        /// virtual memory mappings for this task
        vm::Map *vm = nullptr;

        /// task lock
        DECLARE_RWLOCK(lock);

        /// List of threads belonging to this task; must have at least one
        rt::Vector<Thread *> threads;

    public:
        /// Allocates a new task from the task struct pool
        static Task *alloc();
        /// Releases a previously allocated task struct
        static void free(Task *);

        Task();
        ~Task();

        /// Sets the task's name.
        void setName(const char *name);

    private:
        static void initAllocator();
};
};

#endif
