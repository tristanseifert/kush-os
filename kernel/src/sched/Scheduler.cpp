#include "Scheduler.h"
#include "GlobalState.h"
#include "Task.h"
#include "Thread.h"
#include "IdleWorker.h"

#include "vm/Map.h"
#include "mem/SlabAllocator.h"

#include <arch/critical.h>
#include <arch/PerCpuInfo.h>

#include <platform.h>
#include <new>
#include <log.h>

using namespace sched;

Task *sched::gKernelTask = nullptr;

/**
 * Initializes the global (shared) scheduler structures, as well as the scheduler for the calling
 * processor.
 *
 * This should only be called once, on the BSP during kernel startup. Later APs should instead call
 * the `Scheduler::InitAp()` method.
 */
void Scheduler::Init() {
    // set up the shared scheduler database
    GlobalState::Init();

    // create kernel task
    gKernelTask = Task::alloc(vm::Map::kern());
    gKernelTask->setName("kernel_task");

    // then, initialize the per-core scheduler the same as for all APs
    InitAp();
}

/**
 * Initializes the scheduler for the current processor.
 */
void Scheduler::InitAp() {
    auto sched = new Scheduler;
    REQUIRE(sched, "failed to allocate %s", "scheduler");

    arch::GetProcLocal()->sched = sched;
}

/**
 * Return the current core's scheduler.
 */
Scheduler *Scheduler::get() {
    return arch::GetProcLocal()->sched;
}


/**
 * Sets up the scheduler.
 */
Scheduler::Scheduler() {
    // set up our idle worker
    this->idle = new IdleWorker(this);
}

/**
 * Tears down the idle worker.
 */
Scheduler::~Scheduler() {
    delete this->idle;
}



/**
 * Scheduler entry point; this will jump to the switch function which will select the first
 * runnable thread and switch to it.
 */
void Scheduler::run() {
    // we should NEVER get here
    panic("Scheduler::switchToRunnable() returned (this should never happen)");
}



/**
 * Schedules all threads in the given task that aren't already scheduled. Any threads that are in
 * the paused state will become runnable. (Other states aren't changed.)
 */
void Scheduler::scheduleRunnable(Task *task) {
    // TODO: check which threas are scheduled already

    RW_LOCK_READ_GUARD(task->lock);
    for(Thread *thread : task->threads) {
        // become runnable if needed
        if(thread->state == Thread::State::Paused) {
            thread->setState(Thread::State::Runnable);
        }

        // ignore if not runnable; otherwise, add it to the "possibly runnable" queue
        if(thread->state != Thread::State::Runnable) continue;

        // TODO: mark thread runnable
        // this->markThreadAsRunnable(thread, false, true);
    }

    if(!task->registered) {
        bool yes = true;
        __atomic_store(&task->registered, &yes, __ATOMIC_RELEASE);

        GlobalState::the()->registerTask(task);
    }
}

/**
 * Adds the given thread to the run queue. If the thread's priority is higher than the currently
 * executing one, an immediate context switch is performed.
 *
 * @param t Thread to switch to
 * @param shouldSwitch Whether the context switch should be performed by the scheduler. An ISR may
 * want to perform some cleanup before a context switch is initiated.
 *
 * @return Whether a higher priority thread became runnable: this can be used to perform a manual
 * scheduler invocation later, for example, in an interrupt handler.
 */
bool Scheduler::markThreadAsRunnable(Thread *t, const bool shouldSwitch) {
    panic("%s unimplemented", __PRETTY_FUNCTION__);

    return false;
}

/**
 * Gives up the remainder of the thread's time quantum.
 */
void Scheduler::yield(const Thread::State state) {
    panic("%s unimplemented", __PRETTY_FUNCTION__);
}



/**
 * Sends a dispatch IPI on the current core if the run queue has been dirtied.
 */
void Scheduler::lazyDispatch() {
    // exit if run queue is not dirty
    bool yes = true, no = false;

    if(!__atomic_compare_exchange(&this->isDirty, &yes, &no, false, __ATOMIC_RELEASE,
                __ATOMIC_RELAXED)) {
        return;
    }

    // perform IPI
}


/**
 * Handle kernel time ticks.
 *
 * XXX: This should go somewhere better than in scheduler code.
 */
void platform_kern_tick(const uintptr_t irqToken) {
    panic("%s unimplemented", __PRETTY_FUNCTION__);
}

/**
 * Performs deferred scheduler updates if needed.
 *
 * This is called in response to a scheduler IPI.
 */
void platform_kern_scheduler_update(const uintptr_t irqToken) {
    panic("%s unimplemented", __PRETTY_FUNCTION__);
}
