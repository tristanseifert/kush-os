#include "Scheduler.h"
#include "SchedulerBehavior.h"
#include "Task.h"
#include "Thread.h"
#include "IdleWorker.h"

#include "vm/Map.h"
#include "mem/SlabAllocator.h"

#include <arch/critical.h>

#include <platform.h>
#include <new>
#include <log.h>

using namespace sched;

Scheduler *Scheduler::gShared = nullptr;

Task *gKernelTask = nullptr;

/**
 * Initializes the global scheduler.
 */
void Scheduler::init() {
    gShared = new Scheduler();

    log("sizes: scheduler = %u, task = %u, thread = %u", sizeof(Scheduler), sizeof(Task), sizeof(Thread));
}

/**
 * Sets up the scheduler.
 *
 * We'll set up the kernel task as well as the idle thread here.
 */
Scheduler::Scheduler() {
    // create kernel task
    gKernelTask = Task::alloc(vm::Map::kern());
    gKernelTask->setName("kernel_task");
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
    // create the idle worker
    this->idle = new IdleWorker(this);

    // prepare for scheduler setup
    this->running = nullptr;

    // do it
    this->handleDeferredUpdates();
    this->switchToRunnable();

    // we should NEVER get here
    panic("Scheduler::switchToRunnable() returned (this should never happen)");
}

/**
 * Selects the next runnable thread.
 *
 * The thread provided as an argument will be ignored for purposes of getting scheduled; this can
 * be used so that if a thread yields, we'll try to schedule other, possibly lower-priority threads
 * again.
 *
 * An optional callback may be invoked immediately before context switching to perform task such as
 * acknowledging an interrupt.
 *
 * @param requeueRunning When set, the currently running thread is placed back on the run queue.
 */
void Scheduler::switchToRunnable(Thread *ignore, bool requeueRunning, void (*willSwitchCb)(void *), void *willSwitchCbCtx) {
    Thread *next = nullptr;

    /*
     * Fake entering a critical section by raising the irql to the critical section level; this
     * ensures we're not interrupted until we finish switching to the thread, where the arch code
     * will lower the irql again, after it has masked interrupts to prepare for switching context.
     *
     * This is a bit heavy handed but if the code below is reasonably fast (in that we find a
     * runnable thread quickly) it shouldn't be too bad, but definitely a target for later
     * optimizations.
     */
    platform_raise_irql(platform::Irql::CriticalSection);
    //SPIN_LOCK(this->runnableLock);

    // check priority bands from highest to lowest
    for(size_t i = 0; i < kPriorityGroupMax; i++) {
        auto &queue = this->runnable[i];
        if(queue.empty()) continue;

        next = queue.pop();
        if(next == ignore || next == this->running) {
            // pop the next item off the queue, if any
            if(!queue.empty()) {
                next = queue.pop();
                queue.push_back(ignore);
                goto beach;
            }

            // if none, place it back and check next priority band
            queue.push_back(next);
            continue;
        }

        // switch to this thread
        goto beach;
    }

beach:;
    // if still no thread found, schedule idle worker
    if(!next) {
        next = this->idle->thread;
    }

    // switch to it
    //SPIN_UNLOCK(this->runnableLock);
    REQUIRE(next, "failed to find thread to switch to");

    // do not perform a context switch into an already running thread
    if(next == this->running) {
        platform_lower_irql(platform::Irql::Passive);
        return;
    }

    // ensure the thread we're switching from will get CPU time again
    if(requeueRunning && this->running && this->running->state == Thread::State::Runnable) {
        const auto band = this->groupForThread(this->running);

        //SPIN_LOCK(this->runnableLock);
        log("preserving thread %s (pre-empted)", this->running->name);
        this->runnable[band].push_back(this->running);
        //SPIN_UNLOCK(this->runnableLock);
    }

    // otherwise, reset its boost, quantum and switch
    if(next->priorityBoost > 0) {
        // if it had a positive priority boost, delete it
        next->priorityBoost = 0;
    }

    next->quantum = next->quantumTicks;

    if(willSwitchCb) {
        willSwitchCb(willSwitchCbCtx);
    }

    next->switchTo();
}

/**
 * Schedules all runnable threads in the given task that aren't already scheduled.
 */
void Scheduler::scheduleRunnable(Task *task) {
    // TODO: check which threas are scheduled already

    RW_LOCK_READ_GUARD(task->lock);
    for(auto thread : task->threads) {
        this->markThreadAsRunnable(thread, false, true);
    }
}

/**
 * Adds the given thread to the end of the run queue.
 */
void Scheduler::markThreadAsRunnable(Thread *t, const bool atHead, const bool immediate) {
    DECLARE_CRITICAL();

    // prepare info
    RunnableInfo info(t);
    info.atFront = atHead;

    // insert it
    CRITICAL_ENTER();
    SPIN_LOCK(this->newlyRunnableLock);

    if(atHead) {
        this->newlyRunnable.push_front(info);
    } else {
        this->newlyRunnable.push_back(info);
    }

    SPIN_UNLOCK(this->newlyRunnableLock);
    CRITICAL_EXIT();
}

/**
 * Called in response to a system timer tick. This will see if the current thread's time quantum
 * has expired, and if so, schedule the next runnable thread.
 */
void Scheduler::tickCallback() {
    if(!this->running) return;

    // increment the number of timer ticks that have occurred
    __atomic_fetch_add(&this->ticksToHandle, 1, __ATOMIC_RELEASE);

    // request a dispatch IPI
    platform_request_dispatch();
}

/**
 * Handles the dispatch IPI, i.e. request to update internal state and schedule the next runnable
 * thread.
 *
 * This will first update all deferred updates (tasks that have become unblocked) then handle the
 * case of possibly pre-empting a task. Lastly, it will context switch if a higher priority task
 * has become available to run.
 */
void Scheduler::receivedDispatchIpi(const uintptr_t irqToken) {
    bool needsSwitch = false;

    // define the callback for irq acknowledgement
    auto ackFxn = [](void *ctx) {
        const auto irq = reinterpret_cast<uint32_t>(ctx);
        platform_irq_ack(irq);
    };

    // then, handle deferred updates (to unblock tasks)
    needsSwitch = this->handleDeferredUpdates();

    // perform all elapsed ticks
    if(this->ticksToHandle) {
        while(this->ticksToHandle--) {
            // periodic priority adjustments
            if(++this->priorityAdjTimer == 20) {
                this->adjustPriorities();
                this->priorityAdjTimer = 0;
            }

            // does the running thread need to be pre-empted?
            if(--this->running->quantum == 0) {
                // somewhat boost down the priority of this task
                if(this->running->priorityBoost < -10) {
                    this->running->priorityBoost--;
                }

                // ensure we acknowledge the interrupt if switching away
                this->yield(ackFxn, reinterpret_cast<void *>(irqToken));
            }
        }

        this->ticksToHandle = 0;
    }

    /*
     * Perform a context switch if the runnable thread has not been pre-empted.
     */
    if(needsSwitch) {
        this->switchToRunnable(nullptr, true, ackFxn, reinterpret_cast<void *>(irqToken));
    }

    // no context switching was performed, so acknowledge the irq now
    platform_irq_ack(irqToken);
}

/**
 * Handles deferred updates, i.e. threads that became runnable since the last scheduler
 * invocation.
 */
bool Scheduler::handleDeferredUpdates() {
    DECLARE_CRITICAL();
    bool hadUpdates = false;

    CRITICAL_ENTER();
    SPIN_LOCK(this->newlyRunnableLock);

    while(!this->newlyRunnable.empty()) {
        hadUpdates = true;
        const RunnableInfo info = this->newlyRunnable.pop();

        const auto band = this->groupForThread(info.thread);
        info.thread->setState(Thread::State::Runnable);
        info.thread->priorityBoost = 0;

        //SPIN_LOCK(this->runnableLock);

        if(info.atFront) {
            this->runnable[band].push_front(info.thread);
        } else {
            this->runnable[band].push_back(info.thread);
        }

        //SPIN_UNLOCK(this->runnableLock);
    }

    SPIN_UNLOCK(this->newlyRunnableLock);
    CRITICAL_EXIT();

    return hadUpdates;
}

/**
 * Finds all threads that are waiting for CPU time and increments their priority boost; this will
 * allow threads to jump a certain number of priority bands if they're starved for processor time,
 * depending on their scheduling class.
 *
 * Note that we only do this on threads in priority bands lower than the one the currently
 * executing thread is in. (This follows as there shouldn't be any higher priority runnable
 * threads, if we're only now pre-empting this one.)
 */
void Scheduler::adjustPriorities() {
    DECLARE_CRITICAL();
    CRITICAL_ENTER();

    //SPIN_LOCK(this->runnableLock);

    const auto band = this->groupForThread(this->running);
    if(band == PriorityGroup::Idle) goto done;

    for(size_t i = band; i < kPriorityGroupMax; i++) {
        // increment priority boost and remove threads that gained enough priority to jump up
        auto &list = this->runnable[i].getStorage();
        list.removeMatching(UpdatePriorities, this);
    }

done:;
    //SPIN_UNLOCK(this->runnableLock);
    CRITICAL_EXIT();
}

/**
 * Boosts the given thread's priority. Returns true if the thread jumped priority levels; in this
 * case, we must add it into its new priority class.
 */
bool Scheduler::handleBoostThread(Thread *thread) {
    RW_LOCK_READ_GUARD(thread->lock);

    // ignore non-runnable threads
    if(thread->state != Thread::State::Runnable) return false;

    // determine the maximum boost for this thread
    const auto band = this->groupForThread(thread);
    const auto &rules = kSchedulerBehaviors[band];

    // increment thread priority if allowed
    if(thread->priorityBoost < rules.maxBoost &&
       (thread->priority + thread->priorityBoost) < 100) {
        // increment it's boost; and get the previous priority band
        const auto oldBandBoosted = this->groupForThread(thread, true);
        thread->priorityBoost++;

        // did it jump priority classes?
        const auto bandBoosted = this->groupForThread(thread, true);

        // if so, add it to its new priority class
        if(band > bandBoosted && bandBoosted != oldBandBoosted) {
            //log("thread %p (prio %d boost %d) new level %d (old %d)", thread, thread->priority, thread->priorityBoost, (int) bandBoosted, (int) band);
            this->runnable[bandBoosted].push_back(thread);
            return true;
        }
    }

    // do not remove
    return false;
}

/**
 * Jumps into the scheduler handler for updating the given thread's priorities.
 */
bool sched::UpdatePriorities(void *ctx, Thread *thread) {
    return (reinterpret_cast<Scheduler *>(ctx)->handleBoostThread(thread));
}

/**
 * Yields the remainder of the processor time allocated to the current thread. This will run the
 * next available thread. The current thread is pushed to the back of its run queue.
 */
void Scheduler::yield(void (*willSwitch)(void*), void *willSwitchCtx) {
    auto thread = this->running;
    REQUIRE(thread, "cannot yield without running thread");

    // yeet
    platform_raise_irql(platform::Irql::CriticalSection);
    //REQUIRE(platform_get_irql() <= platform::Irql::Scheduler, "Scheduler: invalid irql");

    // insert it to the back of the runnable queue for its group
    if(thread->state == Thread::State::Runnable) {
        this->requeueRunnable(thread);
    }

    // pick next thread
    this->switchToRunnable(thread, false, willSwitch, willSwitchCtx);
}

/**
 * Adds a thread back to the runnable queue. Invoke this when the thread gives up CPU time (or is
 * pre-empted because another higher priority task awoke, but is still runnable) to place it back
 * on the run queue.
 */
void Scheduler::requeueRunnable(Thread *thread) {
    DECLARE_CRITICAL();

    // get the band to place it in
    const auto band = this->groupForThread(thread);

    // insert in a critical section
    CRITICAL_ENTER();
    // SPIN_LOCK(this->runnableLock);

    this->runnable[band].push_back(thread);

    // SPIN_UNLOCK(this->runnableLock);
    CRITICAL_EXIT();
}

/**
 * Returns the priority group that a particular thread belongs to.
 *
 * Currently, we linearly interpolate the range [-100, 100] to [4, 0] in our priority band groups.
 */
const Scheduler::PriorityGroup Scheduler::groupForThread(Thread *t, const bool withBoost) const {
    return PriorityGroup::Highest;

/*    const auto prio = t->priority + (withBoost ? t->priorityBoost : 0);

    // idle band is [-100, -60)
    if(prio >= -100 && prio < -60) {
        return PriorityGroup::Idle;
    }
    // below normal band is [-60, -20)
    else if(prio >= -60 && prio < -20) {
        return PriorityGroup::BelowNormal;
    }
    // normal band is [-20, 20)
    else if(prio >= -20 && prio < 20) {
        return PriorityGroup::Normal;
    }
    // above normal band is [20, 60)
    else if(prio >= 20 && prio < 60) {
        return PriorityGroup::AboveNormal;
    }
    // the highest/realtime band is [60, 100]
    else if(prio >= 60 && prio <= 100) {
        return PriorityGroup::Highest;
    } else {
        panic("invalid priority for thread %p: %d", t, prio);
    }*/
}



/**
 * Handle kernel time ticks.
 *
 * XXX: This should go somewhere better than in scheduler code.
 */
void platform_kern_tick(const uintptr_t irqToken) {
    Scheduler::gShared->tickCallback();
}

/**
 * Performs deferred scheduler updates if needed.
 *
 * This is called in response to a scheduler IPI.
 */
void platform_kern_scheduler_update(const uintptr_t irqToken) {
    Scheduler::gShared->receivedDispatchIpi(irqToken);
}
