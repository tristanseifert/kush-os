#include "IdleWorker.h"
#include "Scheduler.h"
#include "Task.h"
#include "Thread.h"

#include <platform.h>
#include <log.h>

using namespace sched;

// kernel task ptr
extern Task *gKernelTask;

/**
 * Initializes the worker thread.
 */
IdleWorker::IdleWorker(Scheduler *_sched) : sched(_sched) {
    // create the idle worker thread
    this->thread = Thread::kernelThread(gKernelTask, &IdleEntry, (uintptr_t) this);
    REQUIRE(this->thread, "failed to create idle worker");

    this->thread->priority = kThreadPriority;
    this->thread->setName("Idle worker");

    this->thread->setState(Thread::State::Runnable);

    // mark it such that it doesn't automatically get scheduled
    this->thread->sched.flags |= SchedulerThreadDataFlags::DoNotSchedule | 
                                 SchedulerThreadDataFlags::Idle;

    // add it to the scheduler
    //sched->markThreadAsRunnable(this->thread);
}

/**
 * Trampoline into the idle worker class. The argument is a pointer to the object.
 */
void sched::IdleEntry(uintptr_t arg) {
    reinterpret_cast<IdleWorker *>(arg)->main();
}

/**
 * Main work loop of the idle worker.
 *
 * On each iteration, we'll process work requests and invoke any installed callbacks before giving
 * control of the CPU back to the scheduler. If there's any runnable threads, they'll run; if not,
 * we'll just get the CPU back.
 */
void IdleWorker::main() {
    log("idle worker :)");

    while(1) {
        this->checkWork();
        platform::Idle();
    }
}

/**
 * Checks pending work requests and executes them.
 */
void IdleWorker::checkWork() {
    WorkItem *item = nullptr;

    // pop, execute and delete work items until queue is no longer full
    while(this->work.pop(item, rt::LockFreeQueueFlags::kSingleConsumer)) {
        REQUIRE(item, "got invalid NULL work item");

        (*item)();
        delete item;
    }
}

/**
 * Handles a request to destroy a thread.
 *
 * This will usually only come from requests to terminate threads; if an entire task is terminating
 * it will directly deallocate its threads.
 */
void IdleWorker::DeleteThreadItem::operator()() {
    // detach it from its task if needed
    if(this->thread->task) {
        this->thread->task->detachThread(this->thread);
    }

    // it will automatically be freed as the last ref is dropped
    if(gLog) {
        log("deleting thread %p", static_cast<void *>(this->thread));
    }
}

/**
 * Deallocates a task.
 */
void IdleWorker::DeleteTaskItem::operator()() {
    if(gLog) {
        log("deleting task %p", static_cast<void *>(this->task));
    }
}
