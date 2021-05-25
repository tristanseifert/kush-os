#include "Scheduler.h"
#include "SchedulerData.h"
#include "GlobalState.h"
#include "Deadline.h"
#include "Task.h"
#include "Thread.h"
#include "IdleWorker.h"

#include "vm/Map.h"
#include "mem/SlabAllocator.h"

#include <arch/critical.h>
#include <arch/rwlock.h>
#include <arch/PerCpuInfo.h>

#include <platform.h>
#include <new>
#include <log.h>

using namespace sched;

rt::SharedPtr<Task> sched::gKernelTask = nullptr;

/// rwlock for the global list of scheduler instances (across all cores)
DECLARE_RWLOCK_S(gSchedulersLock);
/// list of all scheduler instances
rt::Vector<Scheduler::InstanceInfo> *Scheduler::gSchedulers = nullptr;

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
Scheduler::Scheduler() {
    this->ipi = new IpiInfo;
    this->coreId = arch::GetProcLocal()->getCoreId();

    this->idle = new IdleWorker(this);

    // insert into the global list of schedulers
    InstanceInfo info(this);
    info.coreId = arch::GetProcLocal()->getCoreId();

    RW_LOCK_WRITE(&gSchedulersLock);
    if(!gSchedulers) {
        gSchedulers = new rt::Vector<InstanceInfo>;
    }

    gSchedulers->push_back(info);
    RW_UNLOCK_WRITE(&gSchedulersLock);

    // initialization is done. ensure other schedulers update their peer lists
    invalidateAllPeerList(this);
}

/**
 * Tears down the idle worker.
 */
Scheduler::~Scheduler() {
    // remove the scheduler from the global list
    RW_LOCK_WRITE(&gSchedulersLock);

    for(size_t i = 0; i < gSchedulers->size(); i++) {
        const auto &info = (*gSchedulers)[i];
        if(info.instance != this) continue;

        gSchedulers->remove(i);
        break;
    }

    RW_UNLOCK_WRITE(&gSchedulersLock);

    // clean up any worker threads we've spawned
    delete this->idle;

    // clean up some other resources
    invalidateAllPeerList(this);

    delete this->ipi;
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
    // pick the first thread...
    auto thread = this->findRunnableThread();
    REQUIRE(thread, "no runnable threads");

    // switch to it
    this->switchTo(thread);

    // we should NEVER get here
    panic("Scheduler::switchToRunnable() returned (this should never happen)");
}



/**
 * Schedules all threads in the given task that aren't already scheduled. Any threads that are in
 * the paused state will become runnable. (Other states aren't changed.)
 */
void Scheduler::scheduleRunnable(rt::SharedPtr<Task> &task) {
    // TODO: check which threas are scheduled already

    RW_LOCK_READ_GUARD(task->lock);
    for(auto &thread : task->threads) {
        // become runnable if needed
        if(thread->state == Thread::State::Paused) {
            thread->setState(Thread::State::Runnable);
        }

        // if thread is runnable, add it to the run queue
        if(thread->state != Thread::State::Runnable) continue;
        this->schedule(thread);
    }

    // register the task with the global state if needed
    if(!__atomic_test_and_set(&task->registered, __ATOMIC_RELAXED)) {
        GlobalState::the()->registerTask(task);
    }
}

/**
 * Adds the given thread to the run queue of the current core. If the thread's priority is higher
 * than the currently executing one, an immediate context switch is performed.
 *
 * @param t Thread to switch to
 * @param shouldSwitch Whether the context switch should be performed by the scheduler. An ISR may
 * want to perform some cleanup before a context switch is initiated.
 *
 * @return Whether we successfully added the thread to the appropriate run queue (0) or not
 */
int Scheduler::markThreadAsRunnable(const rt::SharedPtr<Thread> &t, const bool shouldSwitch) {
    if(int err = this->schedule(t)) {
        return err;
    }

    // TODO: perform context switch (if requested)
    if(shouldSwitch) {
        auto next = this->findRunnableThread();
        log("switch to %p", static_cast<void *>(next));

        // switch to that thread
        if(next == this->running) {
            this->switchTo(this->running, true);
            this->timer.start(Oclock::Type::ThreadKernel);
        } else if(next) {
            this->switchTo(next);
        }
    }

    return 0;
}

/**
 * Gives up the remainder of the current thread's time quantum.
 *
 * TODO: Is this right? Currently if there's no runnable threads we'll become idle immediately, or
 * go down the next priority level, rather than returning directly to the current thread. This is
 * because the thread isn't actually rescheduled until it's actually context switched out.
 */
void Scheduler::yield(const Thread::State state) {
    // find the next runnable thread
    auto runnable = this->findRunnableThread();

    // perform context switch to next runnable (if one was found)
    if(runnable) {
        this->switchTo(runnable);
    }
    // no runnable thread, so go start the idle worker
    else {
        this->switchTo(this->idle->thread);
    }
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
    rt::SharedPtr<Thread> thread = nullptr;

beach:;
    // read the current epoch and check each run queue
    epoch = __atomic_load_n(&this->levelEpoch, __ATOMIC_RELAXED);
    newEpoch = epoch + 1;

    for(size_t i = 0; i < kNumLevels; i++) {
        auto &level = this->levels[i];

again:;
        if(level.storage.pop(thread, rt::LockFreeQueueFlags::kPartialPop)) {
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
                if(kLogQueueOps) {
                    log("sched pull %p (%d) from %lu (ignored)", static_cast<void *>(thread),
                            static_cast<int>(thread->state), i);
                }
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
 */
int Scheduler::schedule(const rt::SharedPtr<Thread> &thread) {
    auto &sched = thread->sched;

    // get level
    const auto levelNum = this->getLevelFor(thread);
    auto &level = this->levels[levelNum];

    // update its quantum (if level changed)
    if(levelNum != sched.lastLevel || !sched.quantumTotal) {
        this->updateQuantumLength(thread);
        sched.lastLevel = levelNum;
    }

    // do not actually push it on the run queue if not schedulable
    if(TestFlags(sched.flags & SchedulerThreadDataFlags::DoNotSchedule)) {
        return 1;
    }

    // try to insert it to that level's queue
    if(!level.push(thread)) {
        panic("sched(%p) level %lu queue overflow (thread %p)", this, levelNum,
                static_cast<void *>(this->running));
        return -1;
    }
    __atomic_fetch_add(&this->levelEpoch, 1, __ATOMIC_RELAXED);

    if(kLogQueueOps) {
        log("sched push %p (%d)   to %lu", static_cast<void *>(thread), static_cast<int>(thread->state),
                levelNum);
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
 * Switches to the given thread.
 *
 * @param fake When set, we skip the actual context switch part. Useful if the same thread simply
 *             gets an extension on its quantum, for example.
 */
void Scheduler::switchTo(const rt::SharedPtr<Thread> &thread, const bool fake) {
    // auto &sched = thread->sched;
    //log("switch: %p ($%p'h %s) quantum %lu/%lu", static_cast<void*>(thread), thread->handle,
    //        thread->name, sched.quantumUsed, sched.quantumTotal);

    // set the timer
    this->updateTimer();

    // finally, perform context switch
    if(!fake) thread->switchTo();
}



/**
 * Updates the timer interval. It will select either the end of the current thread's time quantum,
 * or the nearest deadline.
 */
void Scheduler::updateTimer() {
    uint64_t quantumRemaining;

    // calculate remaining quantum
    if(this->running) [[likely]] {
        auto &sched = this->running->sched;

        if(TestFlags(sched.flags & SchedulerThreadDataFlags::Idle)) [[unlikely]] {
            quantumRemaining = kIdleWakeupInterval;
        } else {
            quantumRemaining = sched.quantumTotal - sched.quantumUsed;
        }
    } else {
        quantumRemaining = kIdleWakeupInterval;
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

    // take an IPI directly if below fudge factor
    if(interval >= this->timerMinInterval) {
        platform::SetLocalTimer(interval);
    } else {
        this->sendIpi();
    }
}

/**
 * Scheduler specific timer has expired; handle deadlines or figure out if the current thread can
 * be preempted.
 *
 * @note This is invoked from an interrupt context; it is not allowed to block.
 */
void Scheduler::timerFired() {
    // update running thread's quantum
    if(this->updateQuantumUsed(this->running)) {
        // entire quantum used so preempt it
        this->running->sched.preempted = true;
    }

    // stop timer and request scheduler invocation
    platform::StopLocalTimer();
    this->sendIpi();
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
 * preempted, or to wake it from idle.
 *
 * The precise actions to take are available in the IPI status struct, which is allocated
 * separately so it's cache line aligned.
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
    // process any deadlines that expired already / expire soon
    {
        RW_LOCK_WRITE_GUARD(this->deadlinesLock);
        const auto now = platform_timer_now();

        bool doNext = true;

        while(!this->deadlines.empty() && doNext) {
            auto &next = this->deadlines.min();
            const auto expires = next.expires();

            // does the deadline lie in the past?
            if(expires <= now) {
                next();
                this->deadlines.extract();
            }
            // does it expire within the fudge time?
            else if((expires - now) <= this->deadlineSlack) {
                next();
                this->deadlines.extract();
            }
            // the deadline doesn't expire until later. chill
            else {
                doNext = false;
            }
        }
    }

    // pick the next highest priority thread and switch to it
    auto next = this->findRunnableThread();

    if(next == this->running) {
        // this thread is still highest priority. restart the counter and update timer
        this->switchTo(this->running, true);
        this->timer.start(Oclock::Type::ThreadKernel);

        return ackIrq(ackCtx);
    } else if(!next) {
        // there are no runnable threads anymore, go and become idle (if not already)
        if(this->running == this->idle->thread) {
            this->switchTo(this->running, true);
            this->timer.start(Oclock::Type::ThreadKernel);

            return ackIrq(ackCtx);
        }

        // otherwise, perform a switch to it
        next = this->idle->thread;
    }

    // we found a thread that's suitable to switch to so go that
    ackIrq(ackCtx);
    this->switchTo(next);
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
 * A thread is being context switched out. We'll stop the appropriate time counters and figure out
 * how much of the time quantum the thread spent executing.
 */
void Scheduler::willSwitchFrom(const rt::SharedPtr<Thread> &from) {
    // skip time accounting if this thread was preempted
    auto &sched = from->sched;

    if(sched.preempted) {
        sched.preempted = false;
    } else {
        this->updateQuantumUsed(from);
    }

    if(kLogQuantum) {
        log("executed '%s' for %lu/%lu nsec", from->name, sched.quantumUsed, sched.quantumTotal);
    }

    // place this back on the run queue (if still runnable)
    if(from->state == Thread::State::Runnable && !from->needsToDie) {
        this->schedule(from);
    }
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
 * The thread is about to start executing on the processor. Start the thread execution time.
 */
void Scheduler::willSwitchTo(const rt::SharedPtr<Thread> &to) {
    this->timer.start(Oclock::Type::ThreadKernel);
}



/**
 * Adds a new deadline for the scheduler to consider.
 */
void Scheduler::addDeadline(const rt::SharedPtr<Deadline> &deadline) {
    DECLARE_CRITICAL();

    DeadlineWrapper wrap(deadline);

    if(kLogDeadlines) {
        log("adding deadline: %p (%lu %lu) %p", static_cast<void *>(deadline),
                deadline->expires, wrap.expires(), &deadline->expires);
    }

    CRITICAL_ENTER();
    {
        RW_LOCK_WRITE_GUARD(this->deadlinesLock);
        this->deadlines.insert(wrap);
    }
    CRITICAL_EXIT();
}

/**
 * Removes an existing deadline, if it has not yet expired.
 *
 * @return Whether the given deadline was found and removed
 */
bool Scheduler::removeDeadline(const rt::SharedPtr<Deadline> &deadline) {
    DECLARE_CRITICAL();
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
    CRITICAL_ENTER();
    {
        RW_LOCK_WRITE_GUARD(this->deadlinesLock);

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
    }
    CRITICAL_EXIT();

    return (info.numRemoved != 0);
}




/**
 * Iterates over the list of all schedulers and marks their peer lists as dirty. This will cause
 * that core to recompute its peers next time it becomes idle and needs to steal work.
 *
 * @param skip Scheduler instance to skip marking as having an invalid peer list
 */
void Scheduler::invalidateAllPeerList(Scheduler *skip) {
    RW_LOCK_READ(&gSchedulersLock);

    for(const auto &info : *gSchedulers) {
        auto sched = info.instance;
        if(sched == skip) continue;

        sched->invalidatePeerList();
    }

    RW_UNLOCK_READ(&gSchedulersLock);
}

/**
 * Iterates the list of all schedulers, and produces a version of this sorted in ascending order by
 * the cost of moving a thread from that core. This is used for work stealing.
 *
 * Sorting is by means of an insertion sort. This doesn't scale very well but even for pretty
 * ridiculous core counts this should not be too terribly slow; especially considering this code
 * runs very, very rarely, and even then, only when the core is otherwise idle.
 *
 * @note This may only be called from the core that owns this scheduler, as this is the only core
 * that may access the peer list.
 */
void Scheduler::buildPeerList() {
    // clear the old list of peers
    this->peers.clear();

    RW_LOCK_READ_GUARD(gSchedulersLock);
    const auto numSchedulers = gSchedulers->size();

    // if only one core, we have no peers
    if(numSchedulers == 1) return;
    this->peers.reserve(numSchedulers - 1);

    // iterate over list of peers to copy the infos and insert in sorted order
    const auto myId = arch::GetProcLocal()->getCoreId();

    for(const auto &info : *gSchedulers) {
        const auto cost = platform_core_distance(myId, info.coreId);

        // find index to insert at; it goes immediately before the first with a HIGHER cost
        size_t i = 0;
        for(i = 0; i < this->peers.size(); i++) {
            const auto &peer = this->peers[i];

            if(cost <= platform_core_distance(myId, peer.coreId)) goto beach;
        }

beach:;
        // actually insert it
        this->peers.insert(i, info);
    }

    // clear the dirty flag
    __atomic_clear(&this->peersDirty, __ATOMIC_RELAXED);
}



