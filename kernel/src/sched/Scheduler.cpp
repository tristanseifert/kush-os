#include "Scheduler.h"
#include "SchedulerBehavior.h"
#include "Task.h"
#include "Thread.h"
#include "IdleWorker.h"

#include "vm/Map.h"
#include "mem/SlabAllocator.h"

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
 */
void Scheduler::switchToRunnable(Thread *ignore) {
    Thread *next = nullptr;

    // get the thread
    {
        SPIN_LOCK_GUARD(this->runnableLock);

        // check priority bands from highest to lowest
        for(size_t i = 0; i < kPriorityGroupMax; i++) {
            //log("checking queue %lu", i);

            auto &queue = this->runnable[i];
            if(queue.empty()) continue;

            next = queue.pop();
            if(next == ignore) {
                // pop the next item off the queue, if any
                if(!queue.empty()) {
                    next = queue.pop();
                    queue.push(ignore);
                    goto beach;
                }

                // if none, place it back and check next priority band
                queue.push(next);
                continue;
            }
            goto beach;
        }
    }

beach:;
    // switch to it
    REQUIRE(next, "failed to find thread to switch to");

    if(next->priorityBoost > 0) {
        // if it had a positive priority boost, delete it
        next->priorityBoost = 0;
    }

    next->quantum = next->quantumTicks;
    next->switchTo();
}

/**
 * Schedules all runnable threads in the given task that aren't already scheduled.
 */
void Scheduler::scheduleRunnable(Task *task) {
    // TODO: check which threas are scheduled already

    RW_LOCK_READ_GUARD(task->lock);
    for(auto thread : task->threads) {
        this->markThreadAsRunnable(thread);
    }
}

/**
 * Adds the given thread to the end of the run queue.
 */
void Scheduler::markThreadAsRunnable(Thread *t) {
    // mark thread as scheduled

    // insert it to the queue
    const auto band = this->groupForThread(t);
    t->setState(Thread::State::Runnable);
    t->priorityBoost = 0;
    //log("thread %p band %d/%d", t, band, kPriorityGroupMax);

    SPIN_LOCK_GUARD(this->runnableLock);
    this->runnable[band].push(t);
}

/**
 * Called in response to a system timer tick. This will see if the current thread's time quantum
 * has expired, and if so, schedule the next runnable thread.
 *
 * We need to acknowledge the timer IRQ before we perform the context switch if a task has been
 * pre-empted. (We need to acknowledge the IRQ in all cases, but this is especially critical since
 * otherwise we will miss out on timer interrupts until the current task gets resumed.)
 */
void Scheduler::tickCallback(const uintptr_t irqToken) {
    if(!this->running) return;

    // periodic priority adjustments
    if(++this->priorityAdjTimer == 20) {
        this->adjustPriorities();
        this->priorityAdjTimer = 0;
    }

    // pre-empt it
    if(--this->running->quantum == 0) {
        // log("preempting running %p", this->running);
        // since we'll context switch, we won't return here, so acknowledge the interrupt
        platform_irq_ack(irqToken);

        // somewhat boost down the priority of this task
        if(this->running->priorityBoost < -10) {
            this->running->priorityBoost--;
        }

        this->yield();
    } else {
        platform_irq_ack(irqToken);
    }
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
    SPIN_LOCK_GUARD(this->runnableLock);

    const auto band = this->groupForThread(this->running);
    if(band == PriorityGroup::Idle) return;

    for(size_t i = band; i < kPriorityGroupMax; i++) {
        // increment priority boost and remove threads that gained enough priority to jump up
        auto &list = this->runnable[i].getStorage();
        list.removeMatching(UpdatePriorities, this);
    }
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
            this->runnable[bandBoosted].push(thread);
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
void Scheduler::yield() {
    auto thread = this->running;
    REQUIRE(thread, "cannot yield without running thread");

    // insert it to the back of the runnable queue for its group
    const auto band = this->groupForThread(thread);

    SPIN_LOCK(this->runnableLock);
    this->runnable[band].push(thread);
    SPIN_UNLOCK(this->runnableLock);

    thread->setState(Thread::State::Runnable);

    // pick next thread
    this->switchToRunnable(thread);
}

/**
 * Returns the priority group that a particular thread belongs to.
 *
 * Currently, we linearly interpolate the range [-100, 100] to [4, 0] in our priority band groups.
 */
const Scheduler::PriorityGroup Scheduler::groupForThread(Thread *t, const bool withBoost) const {
    const auto prio = t->priority + (withBoost ? t->priorityBoost : 0);

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
    }
}



/**
 * Handle kernel time ticks.
 *
 * XXX: This should go somewhere better than in scheduler code.
 */
void platform_kern_tick(const uintptr_t irqToken) {
    Scheduler::gShared->tickCallback(irqToken);
}
