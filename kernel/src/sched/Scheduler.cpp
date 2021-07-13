#include "Scheduler.h"
#include "SchedulerData.h"
#include "GlobalState.h"
#include "Deadline.h"
#include "Task.h"
#include "Thread.h"
#include "IdleWorker.h"

#include "vm/Map.h"

#include <arch/critical.h>
#include <arch/rwlock.h>
#include <arch/PerCpuInfo.h>

#include <platform.h>
#include <new>
#include <log.h>

using namespace sched;

rt::SharedPtr<Task> sched::gKernelTask = nullptr;

/**
 * Default configuration for the 32 levels the scheduler is currently configured with. The top
 * 4 levels are reserved for kernel threads, while the low 28 are avaialble for all user threads.
 *
 * The lengths of time quantums decrease steadily between levels.
 */
Scheduler::LevelInfo Scheduler::gLevelInfo[kNumLevels] = {
    // kernel queues
    { (1000000ULL *   3) }, { (1000000ULL *   6) }, { (1000000ULL *  12) }, { (1000000ULL *  20) },
    // user queues 
    { (1000000ULL *   5) }, { (1000000ULL *  10) }, { (1000000ULL *  15) }, { (1000000ULL *  20) },
    { (1000000ULL *  25) }, { (1000000ULL *  30) }, { (1000000ULL *  35) }, { (1000000ULL *  40) },
    { (1000000ULL *  45) }, { (1000000ULL *  50) }, { (1000000ULL *  55) }, { (1000000ULL *  60) },
    { (1000000ULL *  70) }, { (1000000ULL *  80) }, { (1000000ULL *  90) }, { (1000000ULL * 100) },
    { (1000000ULL * 110) }, { (1000000ULL * 120) }, { (1000000ULL * 130) }, { (1000000ULL * 140) },
    { (1000000ULL * 150) }, { (1000000ULL * 175) }, { (1000000ULL * 200) }, { (1000000ULL * 225) },
    { (1000000ULL * 250) }, { (1000000ULL * 300) }, { (1000000ULL * 400) }, { (1000000ULL * 500) },
};

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
    gKernelTask = Task::alloc(nullptr, false);
    gKernelTask->vm = rt::SharedPtr<vm::Map>(vm::Map::kern());

    gKernelTask->setName("kernel_task");

    GlobalState::the()->registerTask(gKernelTask);

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
    // return arch::PerCpuInfo::scheduler();
}

/**
 * Sets up the scheduler.
 */
Scheduler::Scheduler() : coreId(arch::GetProcLocal()->getCoreId()), peers(PeerList(this)) {
    this->idle = new IdleWorker(this);
}

/**
 * Tears down the idle worker.
 */
Scheduler::~Scheduler() {
    // clean up any worker threads we've spawned
    delete this->idle;
}

/**
 * Initializes the scheduler data structures in a newly created thread.\
 */
void Scheduler::threadWasCreated(Thread &t) {
    auto &sched = t.sched;

    // set the base priority levels
    if(t.kernelMode) {
        sched.maxLevel = 0;
    } else {
        sched.maxLevel = kUserPriorityLevel;
    }

    // ensure the thread is scheduled in its maximum level initially
    sched.level = sched.maxLevel;
}

/**
 * Scheduler entry point; this will jump to the switch function which will select the first
 * runnable thread and switch to it.
 */
void Scheduler::run() {
    rt::SharedPtr<Thread> thread;

    // configure the scheduler timer
    platform_raise_irql(platform::Irql::Scheduler);
    this->timerUpdate();

    // find runnable thread (or idle thread) and switch to it
    thread = this->findRunnableThread();
    if(!thread) {
        thread = this->idle->thread;
    }

    thread->switchTo();

    // we should NEVER get here
    panic("Scheduler::switchTo() returned (this should never happen)");
}

/**
 * Adds the given thread to the run queue of the current core. If the thread's priority is higher
 * than the currently executing one, an immediate context switch is performed.
 *
 * @param t Thread to switch to
 * @param shouldSwitch Whether the context switch should be performed by the scheduler invoked IPI
 *
 * @return Whether we successfully added the thread to the appropriate run queue (0) or not
 */
int Scheduler::markThreadAsRunnable(const rt::SharedPtr<Thread> &t, const bool shouldSwitch) {
    int err = 0;
    const auto oldIrql = platform_raise_irql(platform::Irql::Scheduler);

    if((err = this->schedule(t))) {
        panic("failed to schedule thread $%p'h: %d", t->getHandle(), err);
        goto done;
    }

    // if this thread is higher priority, take an IPI (so we can context switch)
    if(shouldSwitch && this->currentLevel >= getLevelFor(t)) {
        this->sendIpi();
    }

done:;
    platform_lower_irql(oldIrql);
    return err;
}

/**
 * Gives up the remainder of the current thread's time quantum.
 */
void Scheduler::yield() {
    rt::SharedPtr<Thread> to;

    // raise irql to prevent preemption and update time accounting
    const auto oldIrql = platform_raise_irql(platform::Irql::Scheduler);
    const bool expired = this->updateQuantumUsed(this->running);

    // if thread is still runnable...
    if(this->running->getState() == Thread::State::Runnable && !this->running->needsToDie) {
        // adjust level if its quantum expired
        auto maxLevel = this->currentLevel;
        if(expired) maxLevel = getLevelFor(this->running);

        // and ensure it stays in bounds
        if(maxLevel >= kNumLevels) {
            maxLevel = kNumLevels - 1;
        }

        // are there any higher priority threads runnable?
        for(size_t i = 0; i <= maxLevel; i++) {
            const auto &level = this->levels[i];

            // if so, switch to it
            if(!level.storage.empty()) {
                // but first, ensure we'll be scheduled again later by placing us on the run queue
                if(int err = this->schedule(this->running)) {
                    panic("failed to schedule $%p'h: %d", this->running->getHandle(), err);
                }

                goto beach;
            }
        }

        // if we get here, they aren't. so, we can just return after lowering irql
        this->timer.start(Oclock::Type::ThreadKernel);
        return platform_lower_irql(oldIrql);
    }
    // if not, simply pick the next thread (or idle)
beach:;
    to = this->findRunnableThread();
    if(!to) to = this->idle->thread;

    // switch to the destination thread
    if(to != this->running) {
        to->switchTo();
    }

    // when a thread returns, IRQL is back at Passive level. so no need to lower manually
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
 * @return The highest priority runnable thread on this core, or `nullptr` if none.
 */
rt::SharedPtr<Thread> Scheduler::findRunnableThread() {
    size_t tries = 0;
    uint64_t epoch, newEpoch;
    bool epochChanged;
    rt::SharedPtr<Thread> thread;

beach:;
    // read the current epoch and check each run queue
    epoch = __atomic_load_n(&this->levelEpoch, __ATOMIC_RELAXED);
    newEpoch = epoch + 1;

    for(size_t i = 0; i < kNumLevels; i++) {
        auto &level = this->levels[i];

again:;
        if(level.storage.pop(thread, rt::LockFreeQueueFlags::kPartialPop)) {
            REQUIRE(thread, "invalid thread in level %lu run queue", i);
            thread->sched.queuePopped++;

            /**
             * If the thread was supposed to previously terminate, but it's still in the run queue,
             * make sure that the deferred handlers are invoked and we pick another thread to
             * execute instead.
             */
            if(thread->needsToDie) {
                thread->deferredTerminate();
                goto again;
            }
            /*
             * Ignore the thread if it's not runnable and simply go again.
             *
             * A thread may be added back to the run queue, then immediately block, which would
             * otherwise result in Bad Things happening. If we just ignore that case here, we avoid
             * having to go through the entire run queue to remove this thread.
             */
            else if(thread->state != Thread::State::Runnable) {
                log("sched pull %p (%d) from %lu (ignored)", static_cast<void *>(thread),
                        static_cast<int>(thread->state), i);
                goto again;
            }

            REQUIRE(thread->state == Thread::State::Runnable,
                    "invalid thread state in run queue %lu: %d", i, static_cast<int>(thread->state));

            // found a thread, update last schedule timestamp
            level.lastScheduledTsc = platform_local_timer_now();

            this->currentLevel = i;

            if(kLogQueueOps) {
                log("sched pull %p (%d) from %lu", static_cast<void *>(thread),
                        static_cast<int>(thread->state), i);
            }
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
    this->currentLevel = kNumLevels;
    return nullptr;
}

/**
 * Pushes the given thread into the appropriate level's run queue.
 *
 * Additionally, if the thread's quantum has fully expired or it moved levels since it was last
 * executed, it will be updated with the level's quantum value, plus or minus any adjustments due
 * to the thread's priority.
 *
 * @param updateQuantum Whether we bother updating the thread's time quantum
 */
int Scheduler::schedule(const rt::SharedPtr<Thread> &thread) {
    auto &sched = thread->sched;
    const auto levelNum = this->getLevelFor(thread);
    auto &level = this->levels[levelNum];

    // the level had to have changed
    if(levelNum != sched.lastLevel || !sched.quantumTotal) {
        this->updateQuantumLength(thread);
        sched.lastLevel = levelNum;
    }

    // do not schedule the idle thread
    if(TestFlags(sched.flags & SchedulerThreadDataFlags::Idle)) {
        return 1;
    }
    // do not actually push it on the run queue if not schedulable
    else if(TestFlags(sched.flags & SchedulerThreadDataFlags::DoNotSchedule)) {
        return 1;
    }

    // try to insert it to that level's queue
    if(!level.push(thread)) {
        panic("sched(%p) level %lu queue overflow (thread %p)", this, levelNum,
                static_cast<void *>(this->running));
        return -1;
    }
    __atomic_fetch_add(&this->levelEpoch, 1, __ATOMIC_RELAXED);

    if(levelNum < this->maxScheduledLevel) {
        this->maxScheduledLevel = levelNum;
    }

    return 0;
}

/**
 * Returns the run queue level to which the given thread belongs.
 *
 * If it is not currently executing, this is the run queue into which it should be inserted when it
 * becomes runnable again. If it is running, this indicates not necessarily the queue it was pulled
 * from when it started execution, but the queue that it will be placed back on if it gave up the
 * processor at that particular instant.
 */
size_t Scheduler::getLevelFor(const rt::SharedPtr<Thread> &thread) {
    // TODO: take into account priority offset
    const auto &data = thread->sched;

    // ensure we don't go out of bounds (past end or higher than max priority)
    const auto level = (data.level >= kNumLevels) ? (kNumLevels - 1) : data.level;
    return (level < data.maxLevel) ? data.maxLevel : level;
}

/**
 * Scheduler specific timer has expired; handle deadlines or figure out if the current thread can
 * be preempted.
 *
 * @note This is invoked from an interrupt context; it is not allowed to block.
 */
void Scheduler::timerFired() {
    bool preempted = false;
    this->maxScheduledLevel = kNumLevels;

    platform::StopLocalTimer();

    /*
     * Update the currently running thread's time quantum, and set the preemption flag if it has
     * consumed it all.
     */
    if(this->updateQuantumUsed(this->running)) {
        this->running->sched.preempted = true;
        preempted = true;
    }

    // process any deadlines that expired already / expire soon
    this->processDeadlines();

    /*
     * Send an IPI if the currently running thread needs to be preempted, or whether a deadline
     * scheduled a thread with a higher priority than the current one.
     *
     * If no IPI is needed, update the timer and return. Otherwise, once we take the IPI, it will
     * update the timer.
     */
    if(preempted || this->currentLevel >= this->maxScheduledLevel) {
        this->sendIpi();
    } else {
        this->timerUpdate();
    }
}

/**
 * Sets the platform timer interval to the minimum of the time to the next deadline or the current
 * thread being preempted.
 */
void Scheduler::timerUpdate() {
    uint64_t quantumRemaining = kIdleWakeupInterval;

    // calculate remaining quantum
    if(this->running) [[likely]] {
        auto &sched = this->running->sched;

        if(TestFlags(sched.flags & SchedulerThreadDataFlags::Idle)) [[unlikely]] {
            quantumRemaining = kIdleWakeupInterval;
        } else {
            quantumRemaining = sched.quantumTotal - sched.quantumUsed;
        }
    }

    // find the nearest deadline
    auto deadline = UINTPTR_MAX;
    const auto now = platform_timer_now();

    {
        RW_LOCK_READ_GUARD(this->deadlinesLock);

        if(!this->deadlines.empty()) {
            // is the front deadline expired/close to expiring?
            const auto &next = this->deadlines.min();
            const auto expires = next.expires();

            if(expires <= now) { // already expired
                deadline = 0;
            } else { // expires in the future
                deadline = expires - now;
            }
        }
    }

    // whichever is sooner, set a timer for it
    uint64_t interval = (deadline < quantumRemaining) ? deadline : quantumRemaining;

    // make sure it's at least the min interval (TODO: this is a bit janky)
    if(interval >= this->timerMinInterval) {
        platform::SetLocalTimer(interval);
    } else {
        platform::SetLocalTimer(this->timerMinInterval);
        //this->sendIpi();
    }
}

/**
 * Enqueues a scheduler IPI, updating the IPI status flags as needed.
 *
 * @note Repeated calls to this function may be coalesced into a single IPI.
 *
 * @param flags Work flag bits to set
 */
void Scheduler::sendIpi() {
    // perform self IPI
    if(this->coreId == arch::GetProcLocal()->getCoreId()) { [[ likely ]]
        platform::RequestSchedulerIpi();
    } 
    // to other processor
    else {
        platform::RequestSchedulerIpi(this->coreId);
    }
}

/**
 * Scheduler IPI has been fired. This can be in response to either a thread needing to be
 * preempted, to wake it from idle, or because a higher priority thread was made runnable.
 *
 * On amd64, we expect this to be called with the current thread's kernel stack already active; we
 * accmplish this by not using an interrupt stack, but rather, the legacy interrupt stack switch
 * mechanism. The kernel stack of the thread is set in the TSS, and will be loaded only if we take
 * the IPI while it's executing in user mode. Because scheduler IPI is lower priority than any
 * interrupts, it will never interrupt kernel code using any other stack so this _in theory_ works.
 *
 * @param ackIrq Function to invoke to acknowledge the interrupt; always executed even if we return
 *               immediately to the caller without context switching.
 * @param ackCtx Arbitrary pointer passed to acknowledgement function
 */
void Scheduler::handleIpi(void (*ackIrq)(void *), void *ackCtx) {
    this->maxScheduledLevel = kNumLevels;

    // process unblocked threads (from deadlines, etc.)
    this->processUnblockedThreads();

    // if current thread is still runnable, insert it to run queue
    if(this->running && this->running->getState() == Thread::State::Runnable) {
        this->schedule(this->running);
    }

    // pick the next highest priority thread and switch to it
    auto next = this->findRunnableThread();

    if(next == this->running) {
        // current thread is still highest priority; update scheduler timer and accounting
        this->timerUpdate();
        this->timer.start(Oclock::Type::ThreadKernel);

        return ackIrq(ackCtx);
    } 
    // no runnable threads, so run idle thread
    else if(!next) {
        // we're already idle, so update its accounting
        if(this->running == this->idle->thread) {
            this->timerUpdate();
            this->timer.start(Oclock::Type::ThreadKernel);

            return ackIrq(ackCtx);
        }

        // otherwise, perform a switch to it
        next = this->idle->thread;
    }

    // we found a thread that's suitable to switch to so go that
    this->timerUpdate();

    ackIrq(ackCtx);
    next->switchTo();
}

/**
 * A thread has been unblocked, so add it to the list of newly runnable threads.
 *
 * This list is consulted during each scheduler invocation, and any threads in it are tested to see
 * if they should run again; if so, they're inserted in to the appropriate run queues.
 *
 * If the unblocked thread is higher (or equal) in priority to the currently executing thread, an
 * IPI is taken immediately and the thread will likely be switched to.
 */
void Scheduler::threadUnblocked(const rt::SharedPtr<Thread> &thread) {
    // ensure it's in a valid state
    switch(thread->state) {
        case Thread::State::Sleeping:
        case Thread::State::Blocked:
        case Thread::State::NotifyWait:
            break;

        // about to be destroyed, so do NOT put it on the unblocked list
        case Thread::State::Zombie:
            return;

        case Thread::State::Paused:
        case Thread::State::Runnable:
            panic("thread $%p'h (%lu) unblocked, but has invalid state %d",
                    thread->getHandle(), thread->tid, static_cast<int>(thread->state));
    }

    {
        if(!this->unblocked.insert(thread)) {
            panic("unblock list overflow");
        }
    }

    // request IPI (if the unblocked thread is at the same or higher priority level)
    if(this->currentLevel >= getLevelFor(thread)) {
        this->sendIpi();
    }
}

/**
 * Tests all unblocked threads (called from scheduler IPI) and places those that became runnable on
 * the appropriate run queue.
 */
void Scheduler::processUnblockedThreads() {
    // pop the unblocked threads
    rt::SharedPtr<Thread> thread;
    while(this->unblocked.pop(thread, rt::LockFreeQueueFlags::kPartialPop)) {
        // ensure it's in a valid state
        switch(thread->state) {
            case Thread::State::Sleeping:
            case Thread::State::Blocked:
            case Thread::State::NotifyWait:
                break;

            // if thread died while waiting to be unblocked, ignore it
            case Thread::State::Zombie:
                continue;

            case Thread::State::Paused:
            case Thread::State::Runnable:
                panic("thread $%p'h (%lu) in unblocked list, but has invalid state %d",
                        thread->getHandle(), thread->tid, static_cast<int>(thread->state));
        }

        // test thread state and place back on run queue if desired
        thread->schedTestUnblock();

        if(thread->getState() == Thread::State::Runnable) {
            this->schedule(thread);
        }
    }
}

/**
 * A thread is being context switched out. We'll stop the appropriate time counters and figure out
 * how much of the time quantum the thread spent executing.
 *
 * @note Called from the context switch routine. No blocking or critical sections allowed!
 */
void Scheduler::willSwitchFrom(const rt::SharedPtr<Thread> &from) {
    auto &sched = from->sched;

    if(sched.preempted) {
        sched.preempted = false;
    }
    this->updateQuantumUsed(from);
}

/**
 * The thread is about to start executing on the processor. Start the thread execution time.
 *
 * @note Called from the context switch routine. No blocking or critical sections allowed!
 */
void Scheduler::willSwitchTo(const rt::SharedPtr<Thread> &to) {
    // start time accounting
    this->timer.start(Oclock::Type::ThreadKernel);
}

/**
 * Updates the total length of a task's time quantum.
 */
void Scheduler::updateQuantumLength(const rt::SharedPtr<Thread> &thread) {
    auto &sched = thread->sched;

    // look up the level's base quantum
    const auto levelNum = this->getLevelFor(thread);
    const auto &levelInfo = gLevelInfo[levelNum];

    auto total = levelInfo.quantumLength;

    // apply adjustments
    sched.quantumTotal = total;

}

/**
 * Calculates how many nanoseconds the given thread has received, adds it to its consumed quantum
 * allocation, and if necessary, moves it down to a lower priority queue if it used up its entire
 * time quantum.
 *
 * @note It's assumed that the timer was started when the given thread was switched in; there is
 *       nothing to actually ensure this though.
 *
 * @param thread Thread to account against
 *
 * @return Whether the thread's quantum has expired
 */
bool Scheduler::updateQuantumUsed(const rt::SharedPtr<Thread> &thread) {
    bool expired = false;

    // stop timer and read out
    this->timer.stop();
    const auto nsec = this->timer.reset(Oclock::Type::ThreadKernel);

    // add it to the total quantum used by the thread
    auto &sched = thread->sched;

    sched.cpuTime += nsec;

    auto used = sched.quantumUsed + nsec;
    if(used > sched.quantumTotal) {
        // it used the entire quantum, decrement its priority
        sched.lastLevel = sched.level;
        if(++sched.level >= kNumLevels) {
            sched.level = (kNumLevels - 1);
        }

        // set up time quantum for the next lowest level
        // XXX: this can still be greater than the quantum length?
        sched.quantumUsed = used - sched.quantumTotal;
        this->updateQuantumLength(thread);

        expired = true;
    }
    // only part of the quantum was used
    else {
        sched.quantumUsed = used;
    }

    return expired;
}

/**
 * Processes all expired deadlines, by invoking their expiration methods. These may add new threads
 * to the run queue.
 *
 * @return Whether any deadline expired during this invocation
 */
bool Scheduler::processDeadlines() {
    bool expired = false;

    RW_LOCK_WRITE_GUARD(this->deadlinesLock);
    // XXX: do we need to resample this every loop? in case we have tons of deadlines
    const auto now = platform_timer_now();

    bool doNext = true;

    while(!this->deadlines.empty() && doNext) {
        auto &next = this->deadlines.min();
        const auto expires = next.expires();

        // does the deadline lie in the past?
        if(expires <= now ||
        // does it expire within the fudge time?
          (expires - now) <= this->deadlineSlack) {
            next();
            this->deadlines.extract();
            expired = true;
        }
        // the deadline doesn't expire until later. chill
        else {
            doNext = false;
        }
    }

    return expired;
}

/**
 * Adds a new deadline for the scheduler to consider.
 */
void Scheduler::addDeadline(const rt::SharedPtr<Deadline> &deadline) {
    const auto oldIrql = platform_raise_irql(platform::Irql::Scheduler);

    DeadlineWrapper wrap(deadline);
    bool needTimerUpdate = true;

    if(kLogDeadlines) {
        log("adding deadline: %p (%lu %lu) %p", static_cast<void *>(deadline),
                deadline->expires, wrap.expires(), &deadline->expires);
    }

    // modify deadlines and determine if timer needs update
    {
        RW_LOCK_WRITE_GUARD(this->deadlinesLock);

        if(!this->deadlines.empty()) {
            needTimerUpdate = this->deadlines.min() > wrap;
        }

        this->deadlines.insert(wrap);
    }

    // update timer (if needed) and restore irql
    if(needTimerUpdate) {
        this->timerUpdate();
    }
    platform_lower_irql(oldIrql);
}

/**
 * Removes an existing deadline, if it has not yet expired.
 *
 * @return Whether the given deadline was found and removed
 */
bool Scheduler::removeDeadline(const rt::SharedPtr<Deadline> &deadline) {
    bool needTimerUpdate;
    const auto oldIrql = platform_raise_irql(platform::Irql::Scheduler);

    if(kLogDeadlines) {
        log("removing deadline: %p", static_cast<void *>(deadline));
    }

    // prepare info for removal
    struct RemoveInfo {
        rt::SharedPtr<Deadline> deadline;
        size_t numRemoved = 0;

        RemoveInfo(const rt::SharedPtr<Deadline> &_deadline) : deadline(_deadline) {}
    };

    RemoveInfo info(deadline);

    // enumerate over all the objects
    {
        RW_LOCK_WRITE_GUARD(this->deadlinesLock);

        const auto oldMinExpires = this->deadlines.min().expires();

        this->deadlines.enumerateObjects([](DeadlineWrapper &wrap, bool *remove, void *ctx) -> bool {
            auto info = reinterpret_cast<RemoveInfo *>(ctx);

            // we've found the deadline object to remove
            if(wrap == info->deadline) {
                info->numRemoved++;
                *remove = true;
                return false;
            }
            // continue iterating
            return true;
        }, &info);

        needTimerUpdate = this->deadlines.min().expires() != oldMinExpires;
    }

    // lower irql again
    if(needTimerUpdate) {
        this->timerUpdate();
    }
    platform_lower_irql(oldIrql);

    return (info.numRemoved != 0);
}

