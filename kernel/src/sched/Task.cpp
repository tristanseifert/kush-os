#include "Task.h"
#include "Thread.h"

#include "vm/Map.h"
#include "mem/SlabAllocator.h"

#include <string.h>
#include <new>

using namespace sched;

static char gAllocBuf[sizeof(mem::SlabAllocator<Task>)];
static mem::SlabAllocator<Task> *gTaskAllocator = nullptr;

/**
 * Initializes the task struct allocator.
 */
void Task::initAllocator() {
    gTaskAllocator = reinterpret_cast<mem::SlabAllocator<Task> *>(&gAllocBuf);
    new(gTaskAllocator) mem::SlabAllocator<Task>();
}

/**
 * Allocates a new task.
 */
Task *Task::alloc() {
    if(!gTaskAllocator) initAllocator();
    return gTaskAllocator->alloc();
}

/**
 * Frees a previously allocated task.
 */
void Task::free(Task *ptr) {
    gTaskAllocator->free(ptr);
}



/**
 * Initializes the task structure.
 */
Task::Task() {
    // set up the virtual memory mappings
    this->vm = vm::Map::alloc();
}

/**
 * Destroys the task structure.
 *
 * @note You must not be executing any thread in this task at the time of invoking the
 * destructor. We remove the virtual memory translation tables, which will cause any remaining
 * running tasks (or even kernel threads operating on this set of pagetables) to die pretty
 * spectacularly
 */
Task::~Task() {
    RW_LOCK_WRITE_GUARD(this->lock);

    // release all threads
    for(auto thread : this->threads) {
        Thread::free(thread);
    }

    // release memory maps
    vm::Map::free(this->vm);
}

/**
 * Copies the given name string to the task's name field.
 */
void Task::setName(const char *newName) {
    RW_LOCK_WRITE_GUARD(this->lock);
    strncpy(this->name, newName, kNameLength);
}

/**
 * Adds a thread to this task.
 *
 * We'll take ownership of the thread, meaning that when the task is destroyed, we'll destroy the
 * threads as well.
 */
void Task::addThread(Thread *t) {
    RW_LOCK_WRITE_GUARD(this->lock);
    this->threads.push_back(t);

    t->task = this;
}

/**
 * Detaches a thread from this task.
 *
 * This is only allowed if the thread is paused or in the zombie state.
 */
void Task::detachThread(Thread *t) {
    REQUIRE(t->state == Thread::State::Paused || t->state == Thread::State::Zombie,
            "invalid thread state");

    RW_LOCK_WRITE_GUARD(this->lock);
    for(size_t i = 0; i < this->threads.size(); i++) {
        // if we've found the thread, remove it
        if(this->threads[i] == t) {
            this->threads.remove(i);
            return;
        }
    }

    // failed to find the thread
    panic("thread %p does not belong to task %p!", t, this);
}
