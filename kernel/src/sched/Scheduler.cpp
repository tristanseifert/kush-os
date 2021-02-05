#include "Scheduler.h"
#include "Task.h"
#include "Thread.h"

#include "mem/SlabAllocator.h"

#include <new>
#include <log.h>

using namespace sched;

Scheduler *Scheduler::gShared = nullptr;

/**
 * Initializes the global scheduler.
 */
void Scheduler::init() {
    gShared = new Scheduler();
}

/**
 * Sets up the scheduler.
 *
 * This sets up the slab allocators for scheduler data structures (namely, the task and thread
 * structs) and allocates the kernel task. It will also configure the timer used for preemption.
 */
Scheduler::Scheduler() {

}

/**
 * Scheduler entry point; this will jump to the switch function which will select the first
 * runnable thread and switch to it.
 */
void Scheduler::run() {
    // prepare for scheduler setup
    this->running = nullptr;

    // do it
    this->switchToRunnable();

    // we should NEVER get here
    panic("Scheduler::switchToRunnable() returned (this should never happen)");
}

/**
 * Selects the next runnable thread.
 */
void Scheduler::switchToRunnable() {
    Thread *next = nullptr;

    // get the thread
    {
        SPIN_LOCK_GUARD(this->runnableLock);
        // if no threads to run, try to find some
        if(this->runnable.empty()) {
            panic("no runnable threads");
        }
        next = this->runnable.pop();
    }

    // switch to it
    REQUIRE(next, "failed to find thread to switch to");
    next->switchTo();
}

/**
 * Adds the given thread to the end of the run queue.
 */
void Scheduler::markThreadAsRunnable(Thread *t) {
    // mark thread as scheduled

    // insert it to the queue
    SPIN_LOCK_GUARD(this->runnableLock);
    this->runnable.push(t);
}
