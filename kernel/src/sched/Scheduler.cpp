#include "Scheduler.h"
#include "Task.h"
#include "Thread.h"
#include "IdleWorker.h"

#include "mem/SlabAllocator.h"

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

    log("sizes: scheduler = %lu, task = %lu, thread = %lu", sizeof(Scheduler), sizeof(Task), sizeof(Thread));
}

/**
 * Sets up the scheduler.
 *
 * We'll set up the kernel task as well as the idle thread here.
 */
Scheduler::Scheduler() {
    // create kernel task
    gKernelTask = Task::alloc();
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
                queue.push(next);
                continue;
            }
            goto beach;
        }
    }

beach:;
    // switch to it
    REQUIRE(next, "failed to find thread to switch to");

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
    //log("thread %p band %d/%d", t, band, kPriorityGroupMax);

    SPIN_LOCK_GUARD(this->runnableLock);
    this->runnable[band].push(t);
}

/**
 * Called in response to a system timer tick. This will see if the current thread's time quantum
 * has expired, and if so, schedule the next runnable thread.
 */
void Scheduler::tickCallback() {
    if(!this->running) return;

    // pre-empt it
    if(--this->running->quantum == 0) {
        log("pre-empting %p", this->running);
        this->yield();
    }
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
const Scheduler::PriorityGroup Scheduler::groupForThread(Thread *t) const {
    const auto prio = t->priority;

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
