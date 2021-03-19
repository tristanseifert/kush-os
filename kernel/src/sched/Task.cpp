#include "Task.h"
#include "Thread.h"
#include "Scheduler.h"
#include "GlobalState.h"
#include "IdleWorker.h"

#include "ipc/Port.h"
#include "vm/Map.h"
#include "vm/MapEntry.h"
#include "mem/SlabAllocator.h"

#include <string.h>
#include <new>

using namespace sched;

static char gAllocBuf[sizeof(mem::SlabAllocator<Task>)] __attribute__((aligned(64)));
static mem::SlabAllocator<Task> *gTaskAllocator = nullptr;

/// pid for the next task
uint32_t Task::nextPid = 0;

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
Task *Task::alloc(vm::Map *map) {
    if(!gTaskAllocator) initAllocator();
    return gTaskAllocator->alloc(map);
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
Task::Task(vm::Map *_map) {
    // set up the virtual memory mappings
    if(_map) {
        this->vm = _map;
        this->ownsVm = false;
    } else {
        this->vm = vm::Map::alloc();
        this->ownsVm = true;
    }

    // allocate a handle
    this->handle = handle::Manager::makeTaskHandle(this);

    // misc initialization
    this->pid = __atomic_fetch_add(&nextPid, 1, __ATOMIC_RELEASE);
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

    // remove from scheduler
    GlobalState::the()->unregisterTask(this);

    // terminate all remaining threads
    for(auto thread : this->threads) {
        thread->terminate();
    }

    // release the ports
    for(auto port : this->ports) {
        ipc::Port::free(port);
    }

    // release VM objects
    for(auto region : this->ownedRegions) {
        region->release();
    }

    // invalidate the handle
    handle::Manager::releaseTaskHandle(this->handle);

    if(this->ownsVm) {
        vm::Map::free(this->vm);
    }
}

/**
 * Adds a reference to the given VM object to this task.
 *
 * This is used for created objects that aren't immediately mapped to a task. They'll have a ref
 * count of one, so when this task exits, the objects are destroyed.
 */
void Task::addOwnedVmRegion(vm::MapEntry *region) {
    RW_LOCK_WRITE_GUARD(this->lock);
    this->ownedRegions.append(region);
}

/**
 * Copies the given name string to the task's name field.
 */
void Task::setName(const char *newName, const size_t _inLength) {
    RW_LOCK_WRITE_GUARD(this->lock);

    memset(this->name, 0, kNameLength);

    if(!_inLength) {
        strncpy(this->name, newName, kNameLength);
    } else {
        const auto toCopy = (_inLength >= kNameLength) ? kNameLength : _inLength;
        memcpy(this->name, newName, toCopy);
    }

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

    bool yes = true;
    __atomic_store(&t->attachedToTask, &yes, __ATOMIC_RELEASE);

    t->task = this;
}

/**
 * Detaches a thread from this task.
 *
 * This is only allowed if the thread is paused or in the zombie state. (or is it)
 */
void Task::detachThread(Thread *t) {
    bool attached;
    __atomic_load(&t->attachedToTask, &attached, __ATOMIC_ACQUIRE);

    // bail if not attached
    if(!attached) return;

    REQUIRE(t->state == Thread::State::Paused || t->state == Thread::State::Zombie,
            "invalid thread state for detach: %d", (int) t->state);

    RW_LOCK_WRITE_GUARD(this->lock);
    for(size_t i = 0; i < this->threads.size(); i++) {
        // if we've found the thread, remove it
        if(this->threads[i] == t) {
            bool no = false;
            __atomic_store(&t->attachedToTask, &no, __ATOMIC_RELEASE);

            this->threads.remove(i);
            return;
        }
    }

    // failed to find the thread; ignore
    log("thread %p does not belong to task %p (belongs to %p) attach %d!", t, this, t->task,
            t->attachedToTask);
    return;
}



/**
 * Terminates the task.
 *
 * This will terminate all threads. If anyone is waiting on the task, they're notified of the
 * status code; otherwise, it's discarded
 */
int Task::terminate(int status) {
    // notify anyone blocking on us
    this->notifyExit(status);

    // terminate all threads in this task
    auto current = Thread::current();

    RW_LOCK_WRITE(&this->lock);

    size_t i = 0;
    bool no = false;

    while(i < this->threads.size()) {
        auto thread = this->threads[i];
        if(thread == current) {
            this->threads.remove(i);
            continue;
        }

        __atomic_store(&thread->attachedToTask, &no, __ATOMIC_RELEASE);
        thread->terminate();
        i++;
    }

    this->threads.clear();
    RW_UNLOCK_WRITE(&this->lock);

    // request task deletion later
    Scheduler::get()->idle->queueDestroyTask(this);

    // finally, terminate calling thread, if it is also in this task
    if(current->task == this) {
        __atomic_store(&current->attachedToTask, &no, __ATOMIC_RELEASE);
        current->terminate();
    }

    // success! the task was terminated
    return 0;
}

/**
 * Notifies all interested parties that we've exited.
 */
void Task::notifyExit(int status) {
    log("Task $%08x'h (%s) exited: %d", this->handle, this->name, status);
}

/**
 * Extract the currently executing task from the current thread.
 */
Task *Task::current() {
    auto thread = Thread::current();
    if(thread) {
        return thread->task;
    }
    return nullptr;
}

/**
 * Inserts the given port to the task's port list. All ports that remain in this list when the
 * task is released will be released as well.
 */
void Task::addPort(ipc::Port *port) {
    RW_LOCK_WRITE_GUARD(this->lock);
    this->ports.append(port);
}

/**
 * Removes a port from the task's ports list.
 *
 * @return Whether the port existed in this task prior to the call.
 */
bool Task::removePort(ipc::Port *port) {
    RW_LOCK_WRITE_GUARD(this->lock);
    bool removed = false;

    // set up info
    struct RemovePortInfo {
        ipc::Port *port = nullptr;
        bool *found = nullptr;
    };

    RemovePortInfo info{port, &removed};

    // iterate
    this->ports.removeMatching([](void *ctx, ipc::Port *port) {
        auto info = reinterpret_cast<RemovePortInfo *>(ctx);

        if(port == info->port) {
            *info->found = true;
            return true;
        }
        return false;
    }, &info);

    // return whether we removed it
    return removed;
}

/**
 * Iterate all ports to see if we own one.
 */
bool Task::ownsPort(ipc::Port *port) {
    RW_LOCK_READ_GUARD(this->lock);
    for(auto i : this->ports) {
        if(i == port) {
            return true;
        }
    }
    return false;
}
