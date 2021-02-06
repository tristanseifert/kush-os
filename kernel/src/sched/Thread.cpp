#include "Thread.h"
#include "Scheduler.h"
#include "Task.h"
#include "IdleWorker.h"

#include "mem/StackPool.h"
#include "mem/SlabAllocator.h"

#include <string.h>

using namespace sched;

static char gAllocBuf[sizeof(mem::SlabAllocator<Thread>)];
static mem::SlabAllocator<Thread> *gThreadAllocator = nullptr;

/// Thread id for the next thread
uint32_t Thread::nextTid = 1;

/**
 * Initializes the task struct allocator.
 */
void Thread::initAllocator() {
    gThreadAllocator = reinterpret_cast<mem::SlabAllocator<Thread> *>(&gAllocBuf);
    new(gThreadAllocator) mem::SlabAllocator<Thread>();
}

/**
 * Allocates a new thread.
 */
Thread *Thread::kernelThread(Task *parent, void (*entry)(uintptr_t), const uintptr_t param) {
    if(!gThreadAllocator) initAllocator();
    auto thread = gThreadAllocator->alloc(parent, (uintptr_t) entry, param, true);

    return thread;
}

/**
 * Frees a previously allocated thread.
 *
 * @note The thread MUST NOT be scheduled or running.
 */
void Thread::free(Thread *ptr) {
    gThreadAllocator->free(ptr);
}



/**
 * Allocates a new thread.
 */
Thread::Thread(Task *_parent, const uintptr_t pc, const uintptr_t param, const bool _kernel) : 
    task(_parent), kernelMode(_kernel) {
    this->tid = nextTid++;

    // get a kernel stack
    this->stack = mem::StackPool::get();
    REQUIRE(this->stack, "failed to get stack for thread %p", this);

    // then initialize thread state
    arch::InitThreadState(this, pc, param);

    // and add to task
    if(_parent) {
        _parent->addThread(this);
    }
}

/**
 * Destroys all resources associated with this thread.
 */
Thread::~Thread() {
    // release kernel stack
    mem::StackPool::release(this->stack);
}

/**
 * Performs a context switch to this thread.
 *
 * If we're currently executing on a thread, its state is saved, and the function will return when
 * that thread is switched back in. Otherwise, the function never returns.
 */
void Thread::switchTo() {
    auto current = Scheduler::get()->runningThread();

    Scheduler::get()->setRunningThread(this);
    arch::RestoreThreadState(current, this);
}

/**
 * Copies the given name string to the thread's name field.
 */
void Thread::setName(const char *newName) {
    RW_LOCK_WRITE_GUARD(this->lock);
    strncpy(this->name, newName, kNameLength);
}

/**
 * Call into the scheduler to yield the rest of this thread's CPU time. We'll get put back at the
 * end of the runnable queue.
 */
void Thread::yield() {
    Scheduler::get()->yield();
}

/**
 * Terminates the calling thread.
 *
 * The thread will be marked as a zombie (so it won't be scheduled anymore) and submitted to the
 * scheduler to actually be deallocated at a later time.
 */
void Thread::terminate() {
    auto current = Scheduler::get()->runningThread();
    REQUIRE(current, "cannot terminate null thread!");

    current->setState(State::Zombie);

    Scheduler::get()->idle->queueDestroyThread(current);
    Scheduler::get()->switchToRunnable();

    // we should not get here
    panic("failed to terminmate thread");
}
