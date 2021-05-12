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
 * Selects the next runnable thread.
 *
 * This will go through every level's run queue, from highest to lowest priority, and attempt to
 * pop a thread from its run queue. If successful, a thread is returned immediately. Otherwise,
 * if no other thread has modified any of the run queues since, we'll exit to allow scheduling of
 * the idle thread.
 *
 * We detect whether another thread modified the queues by testing whether the `levelEpoch` field
 * changed between the start and end of our loop. If so, we restart the search up to a specified
 * number of times before giving up.
 *
 * @return The highest priority runnable thread, or `nullptr` if none.
 */
Thread *Scheduler::findRunnableThread() {
    size_t tries = 0;
    uint64_t epoch, newEpoch;
    bool epochChanged;
    Thread *thread = nullptr;

beach:;
    // read the current epoch and check each run queue
    epoch = __atomic_load_n(&this->levelEpoch, __ATOMIC_RELAXED);
    newEpoch = epoch + 1;

    for(size_t i = 0; i < kNumLevels; i++) {
        auto &level = this->levels[i];

        if(level.storage.pop(thread, rt::LockFreeQueueFlags::kPartialPop)) {
            // found a thread, update last schedule timestamp
            level.lastScheduledTsc = platform_local_timer_now();

            return thread;
        }
    }

    // if we get here, no thread was found. check if epoch changed
    epochChanged = !__atomic_compare_exchange_n(&this->levelEpoch, &epoch, newEpoch, false,
            __ATOMIC_RELAXED, __ATOMIC_RELAXED);

    if(epochChanged && ++tries <= this->levelChangeMaxLoops) {
        goto beach;
    }

    // exceeded max retries or epoch hasn't changed, so do idle behavior
    return nullptr;
}

/**
 * Checks if there's a thread ready to run with a higher priority than the currently executing one,
 * and if so, sends a scheduler IPI request that will be serviced on return from the current ISR.
 *
 * @return Whether a higher priority thread became runnable
 */
bool Scheduler::lazyDispatch() {
    // exit if run queue is not dirty
    bool yes = true, no = false;

    if(!__atomic_compare_exchange(&this->isDirty, &yes, &no, false, __ATOMIC_RELEASE,
                __ATOMIC_RELAXED)) {
        return false;
    }

    // perform IPI
    platform_request_dispatch();
    return true;
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
