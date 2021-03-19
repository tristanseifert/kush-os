#ifndef KERNEL_SCHED_GLOBALSTATE_H
#define KERNEL_SCHED_GLOBALSTATE_H

#include <arch/spinlock.h>

#include <runtime/List.h>

namespace sched {
struct Task;

    /**
 * Some scheduler state is shared between all cores; this is stored inside the global state
 * structure.
 */
class GlobalState {
    friend class Scheduler;

    public:
        static void Init();

        static auto the() {
            return gShared;
        }

        void registerTask(Task *task);
        void unregisterTask(Task *task);
        void iterateTasks(void (*callback)(Task *));

    private:
        /// global (shared between processors) global state instance
        static GlobalState *gShared;

    private:
        /// lock protecting tasks
        DECLARE_SPINLOCK(tasksLock);
        /// all active tasks
        rt::List<Task *> tasks;
} __attribute__((aligned(64)));
}

#endif
