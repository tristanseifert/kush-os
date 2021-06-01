#ifndef DEBUG_SCHEDULERSTATE_H
#define DEBUG_SCHEDULERSTATE_H

#include "runtime/SmartPointers.h"
#include "sched/Thread.h"

namespace debug {
void SchedulerStateEntry(uintptr_t arg);

/**
 * Graphical framebuffer console debugger for active tasks/threads and their ports.
 */
class SchedulerState {
    friend void SchedulerStateEntry(uintptr_t arg);

    public:
        SchedulerState();
        ~SchedulerState();

    private:
        void main();

    private:
        /// interval (in ms) to update
        uintptr_t updateInterval = 150;
        /// whether the worker thread is running
        bool run = true;

        /// thread for the scheduler display
        rt::SharedPtr<sched::Thread> thread;
};
};

#endif
