#include "IdleWorker.h"
#include "Scheduler.h"
#include "Task.h"
#include "Thread.h"

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

    // add it to the scheduler
    sched->markThreadAsRunnable(this->thread);
}

/**
 * We can just delete the threads.
 */
IdleWorker::~IdleWorker() {
    Thread::free(this->thread);
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
    while(1) {
        this->checkWork();

        Thread::yield();
    }
}

/**
 * Checks pending work requests and executes them.
 */
void IdleWorker::checkWork() {
    // bail if no work is available
    SPIN_LOCK_GUARD(this->workLock);
    if(this->work.empty()) return;

    // pop all work items off of it
    while(!this->work.empty()) {
        auto item = this->work.pop();

        switch(item.type) {
            // the payload pointer is a thread
            case Type::DestroyThread: {
                auto thread = reinterpret_cast<Thread *>(item.payload);
                this->destroyThread(thread);
                break;
            }
            // this is a no-op
            case Type::Unknown:
                break;
            // we should handle all types!
            default:
                panic("unknown work item type: %d", (int) item.type);
        }
    }
}

/**
 * Handles a request to destroy a thread.
 *
 * This will usually only come from requests to terminate threads; if an entire task is terminating
 * it will directly deallocate its threads.
 */
void IdleWorker::destroyThread(Thread *thread) {
    // detach it from its task if needed
    if(thread->task) {
        thread->task->detachThread(thread);
    }

    // lastly, release it
    Thread::free(thread);
}
