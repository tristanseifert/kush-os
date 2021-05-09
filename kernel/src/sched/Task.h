#ifndef KERNEL_SCHED_TASK_H
#define KERNEL_SCHED_TASK_H

#include <stddef.h>
#include <stdint.h>

#include <arch/ThreadState.h>
#include <arch/rwlock.h>

#include <runtime/List.h>
#include <runtime/Vector.h>
#include <handle/Manager.h>


namespace vm {
class Map;
class MapEntry;
}

namespace ipc {
class Port;
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

        /// parent task
        Task *parent = nullptr;

        /// virtual memory mappings for this task
        vm::Map *vm = nullptr;
        /// whether we own the VM maps
        bool ownsVm = false;

        /// handle to the task
        Handle handle;

        /// when set, skip deleting threads on dealloc
        bool skipThreadDealloc = false;
        /// whether the task is in the global registry
        bool registered = false;

        /// number of physical pages owned by this task
        uintptr_t physPagesOwned = 0;

        /// task lock
        DECLARE_RWLOCK(lock);

        /// List of threads belonging to this task; must have at least one
        rt::Vector<Thread *> threads;
        /// ports owned by this task
        rt::List<ipc::Port *> ports;
        /// VM objects we own
        rt::List<vm::MapEntry *> ownedRegions;

        /// architecture-specific task state
        arch::TaskState archState __attribute__((aligned(16)));

    public:
        /// Allocates a new task from the task struct pool
        static Task *alloc(vm::Map *map = nullptr);
        /// Releases a previously allocated task struct
        static void free(Task *);

        Task(vm::Map *map);
        ~Task();

        /// Adds the given VM map object to the list
        void addOwnedVmRegion(vm::MapEntry *region);

        /// Sets the task's name.
        void setName(const char *name, const size_t length = 0);
        /// Adds a thread to the task.
        void addThread(Thread *t);
        /// Detaches the given thread from the task.
        void detachThread(Thread *t);

        /// Registers a new port to the task.
        void addPort(ipc::Port *port);
        /// Removes a port from the task.
        bool removePort(ipc::Port *port);
        /// Do we own the given port?
        bool ownsPort(ipc::Port *port);

        /// Terminates this task with the given return code
        int terminate(int status);

        /// Returns a handle to the currently executing task.
        static Task *current();
        /// Returns the kernel task handle.
        inline static Task *kern() {
            extern Task *gKernelTask;
            return gKernelTask;
        }

    private:
        void notifyExit(int);

        static uint32_t nextPid;
};
};

#endif
