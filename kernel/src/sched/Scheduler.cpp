#include "Scheduler.h"
#include "Task.h"
#include "Thread.h"

#include "mem/SlabAllocator.h"

#include <new>
#include <log.h>

using namespace sched;

static char gSharedBuf[sizeof(Scheduler)];
Scheduler *Scheduler::gShared = nullptr;

/**
 * Initializes the global scheduler.
 */
void Scheduler::init() {
    gShared = reinterpret_cast<Scheduler *>(&gSharedBuf);
    new(gShared) Scheduler();
}

/**
 * Sets up the scheduler.
 *
 * This sets up the slab allocators for scheduler data structures (namely, the task and thread
 * structs) and allocates the kernel task. It will also configure the timer used for preemption.
 */
Scheduler::Scheduler() {

}
