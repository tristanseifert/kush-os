#include "Scheduler.h"
#include "SchedulerData.h"
#include "GlobalState.h"
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
}


/**
 * Sets up the scheduler.
 */
Scheduler::Scheduler() {
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

    invalidateAllPeerList(this);
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
        this->markThreadAsRunnable(thread, false);
    }

    if(!task->registered) {
        bool yes = true;
        __atomic_store(&task->registered, &yes, __ATOMIC_RELEASE);

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
 * @param higherPriorityRunnable If non-null, location where we write a flag indicating if a higher
 *        priority thread became runnable. This can be used to manually invoke the scheduler at
 *        the end of an interrupt handler, for example.
 *
 * @return Whether we successfully added the thread to the appropriate run queue (0) or not
 */
int Scheduler::markThreadAsRunnable(const rt::SharedPtr<Thread> &t, const bool shouldSwitch,
        bool *higherPriorityRunnable) {
    if(int err = this->schedule(t)) {
        return err;
    }

    // TODO: perform context switch (if requested)
    if(shouldSwitch) {

    }

    // determine if this thread is at a higher priority than the current runnable
    // TODO: should this check the entire structure? other threads don't push work to us...
    if(higherPriorityRunnable) {
        *higherPriorityRunnable = (this->currentLevel > this->getLevelFor(t));
    }

    return 0;
}

/**
 * Gives up the remainder of the current thread's time quantum.
 */
void Scheduler::yield(const Thread::State state) {
    // find the next runnable thread
    auto runnable = this->findRunnableThread();

    // place this back on the run queue
    const auto levelNum = this->getLevelFor(this->running);
    auto &level = this->levels[levelNum];

    if(!level.push(this->running)) {
        panic("sched(%p) level %lu queue overflow (thread %p)", this, levelNum,
                static_cast<void *>(this->running));
    }

    // perform context switch to next runnable (if one was found)
    if(runnable) {
        this->switchTo(runnable);
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

        if(level.storage.pop(thread, rt::LockFreeQueueFlags::kPartialPop)) {
            // found a thread, update last schedule timestamp
            level.lastScheduledTsc = platform_local_timer_now();

            this->currentLevel = i;
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
    const auto &levelInfo = gLevelInfo[levelNum];

    // update its quantum (if level changed)
    if(levelNum != sched.lastLevel || sched.quantumTotal) {
        sched.lastLevel = levelNum;
        sched.quantumTotal = levelInfo.quantumLength;
        sched.quantumUsed = 0; // XXX: is this proper?
    }

    // try to insert it to that level's queue
    if(!level.push(thread)) {
        panic("sched(%p) level %lu queue overflow (thread %p)", this, levelNum,
                static_cast<void *>(this->running));
        return -1;
    }

    // update epoch
    __atomic_fetch_add(&this->levelEpoch, 1, __ATOMIC_RELAXED);

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
 */
void Scheduler::switchTo(const rt::SharedPtr<Thread> &thread) {
    auto &sched = thread->sched;

    log("switch: %p ($%p'h %s) quantum %lu/%lu", static_cast<void*>(thread), thread->handle,
            thread->name, sched.quantumUsed, sched.quantumTotal);

    // TODO: determine when quantum expiration timer fires / nearest deadline

    // finally, perform context switch
    thread->switchTo();
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



