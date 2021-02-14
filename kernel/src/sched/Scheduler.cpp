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

        this->markThreadAsRunnable(thread, false, true);
    }
}

/**
 * Adds the given thread to the end of the run queue.
 */
void Scheduler::markThreadAsRunnable(Thread *t, const bool atHead, const bool immediate) {
    REQUIRE(t->state == Thread::State::Runnable, "cannot add thread %p to run queue: invalid state %d",
            t, (int) t->state);
    DECLARE_CRITICAL();

    // prepare info
    RunnableInfo info(t);
    info.atFront = atHead;

    // insert it
    CRITICAL_ENTER();

    if(atHead) {
        this->newlyRunnable.push_front(info);
    } else {
        this->newlyRunnable.push_back(info);
    }

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
 *
 * @return Whether a context switch was performed
 */
bool Scheduler::switchToRunnable(Thread *ignore, bool requeueRunning, void (*willSwitchCb)(void *), void *willSwitchCbCtx) {
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
    const auto prevIrql = platform_raise_irql(platform::Irql::CriticalSection, false);

    // check priority bands from highest to lowest
    for(size_t i = 0; i < kPriorityGroupMax; i++) {
        // skip empty priority bands
        auto &queue = this->runnable[i];

again:;
        if(queue.empty()) continue;

        // check the head of this priority queue
        next = queue.pop();

        // is it the currently running thread?
        if(next == this->running) {
            // if so, ignore it.
            goto again;
        }
        // is it the thread we're ignoring; or is it not runnable?
        else if(next == ignore || next->state != Thread::State::Runnable) {
            // pop the next item off the queue, if any
            if(!queue.empty()) {
                auto oldNext = next;
                next = queue.pop();
                queue.push_back(oldNext);
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
    REQUIRE(next, "failed to find thread to switch to");
    REQUIRE(next->state == Thread::State::Runnable, "Thread %p '%s' in ready queue but has state %d",
            next, next->name, (int) next->state);

    // do not perform a context switch into an already running thread
    if(next == this->running) {
        platform_lower_irql(prevIrql);
        return false;
    }

    // ensure the thread we're switching from will get CPU time again
    if(requeueRunning && this->running && this->running->state == Thread::State::Runnable) {
        const auto band = this->groupForThread(this->running);

        // log("preserving thread %s (pre-empted)", this->running->name);
        this->runnable[band].push_back(this->running);
    }

    // otherwise, reset its boost, quantum and switch
    next->quantum = next->quantumTicks;

    if(willSwitchCb) {
        willSwitchCb(willSwitchCbCtx);
    }

    next->switchTo();
    return true;
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
    REQUIRE_IRQL(platform::Irql::Scheduler);

    bool needsSwitch = false, requeueThread = true;
    Thread *toIgnore = nullptr;

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
            // does the running thread need to be pre-empted?
            if(--this->running->quantum == 0) {
                // somewhat boost down the priority of this task
                if(this->running->priorityBoost < -10) {
                    this->running->priorityBoost--;
                }

                // switch away from it to the next runnable task
                toIgnore = this->running;
                needsSwitch = true;
                goto beach;
            }
        }

beach:;
        this->ticksToHandle = 0;
    }

    /*
     * Perform a context switch; this can happen if the currently running thread is being preempted
     * because it's finished its CPU quantum, or because we added new threads to the run queue.
     *
     * If neither of those happened, we can simply return out of the dispatch IPI and let whatever
     * thread is currently running resume.
     */
    if(needsSwitch) {
        this->switchToRunnable(toIgnore, requeueThread, ackFxn,
                reinterpret_cast<void *>(irqToken));
    }
}

/**
 * Handles deferred updates, i.e. threads that became runnable since the last scheduler
 * invocation.
 */
bool Scheduler::handleDeferredUpdates() {
    if(this->running) {
        REQUIRE_IRQL(platform::Irql::Scheduler);
    }

    DECLARE_CRITICAL();
    size_t numUpdates = 0;

    CRITICAL_ENTER();

    while(!this->newlyRunnable.empty()) {
        const RunnableInfo info = this->newlyRunnable.pop();
        // ignore threads that have since becoming runnable, been deleted
        if(info.thread->state == Thread::State::Zombie) continue;

        // otherwise, schedule it
        const auto band = this->groupForThread(info.thread);
        REQUIRE(info.thread->state == Thread::State::Runnable, "invalid runnable thread: %p (state %d)", info.thread, (int) info.thread->state);

        // insert at front or back of run queue
        if(info.atFront) {
            this->runnable[band].push_front(info.thread);
        } else {
            this->runnable[band].push_back(info.thread);
        }

        numUpdates++;
    }

    CRITICAL_EXIT();

    return (numUpdates != 0);
}

/**
 * Yields the remainder of the processor time allocated to the current thread. This will run the
 * next available thread. The current thread is pushed to the back of its run queue.
 */
void Scheduler::yield(void (*willSwitch)(void*), void *willSwitchCtx) {
    auto thread = this->running;
    REQUIRE(thread, "cannot yield without running thread");
    REQUIRE_IRQL_LEQ(platform::Irql::Scheduler);

    // raise to the scheduler level
    const auto prevIrql = platform_raise_irql(platform::Irql::Scheduler);

    if(!thread->needsToDie) {
        // queue it to run again during the next scheduler invocation
        if(thread->state == Thread::State::Runnable) {
            const auto band = groupForThread(thread);
            this->runnable[band].push_back(thread);
        }
        // if the thread is blocking, prepare it
        else if(thread->state == Thread::State::Blocked) {
            thread->prepareBlocks();
        }
    }

    // since we're at scheduler IPL, we can context switch now
    const bool switched = this->switchToRunnable(thread, false, willSwitch, willSwitchCtx);

    // if we return, there was no thread to switch to. exit to previous IRQL
    if(!switched) {
        platform_lower_irql(prevIrql);
    }
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
