#include "Task.h"

#include "vm/Map.h"
#include "mem/SlabAllocator.h"

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

    // release memory maps
    vm::Map::free(this->vm);
}
