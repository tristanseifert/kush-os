#include "Task.h"
#include "Thread.h"
#include "Scheduler.h"
#include "GlobalState.h"
#include "IdleWorker.h"

#include "ipc/Port.h"
#include "vm/Map.h"
#include "vm/MapEntry.h"

#include <string.h>
#include <new>

using namespace sched;

/// pid for the next task
uint32_t Task::nextPid = 0;

/**
 * Allocates a new task.
 */
rt::SharedPtr<Task> Task::alloc(rt::SharedPtr<vm::Map> map, const bool writeVm) {
    auto task = new Task(map, writeVm);
    if(!task) return nullptr;

    // create ptr and allocate handle
    rt::SharedPtr<Task> ptr(task);
    task->handle = handle::Manager::makeTaskHandle(ptr);

    return ptr;
}



/**
 * Initializes the task structure.
 *
 * @param writeVm Whether the VM field is set, or it remains set to `nullptr` (for kernel task)
 */
Task::Task(rt::SharedPtr<vm::Map> &_map, const bool writeVm) {
    // set up the virtual memory mappings
    if(writeVm) {
        if(_map) {
            this->vm = _map;
        } else {
            this->vm = vm::Map::alloc();
        }
    }

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
    auto ptr =  handle::Manager::getTask(this->handle);
    GlobalState::the()->unregisterTask(ptr);

    // terminate all remaining threads
    for(auto thread : this->threads) {
        thread->terminate();
    }

    // release the ports
    this->ports.clear();

    // invalidate the handle
    handle::Manager::releaseTaskHandle(this->handle);
}

/**
 * Adds a reference to the given VM object to this task.
 *
 * This is used for created objects that aren't immediately mapped to a task. They'll have a ref
 * count of one, so when this task exits, the objects are destroyed.
 */
void Task::addVmRegion(const rt::SharedPtr<vm::MapEntry> &region) {
    RW_LOCK_WRITE_GUARD(this->lock);
    this->ownedRegions.append(region);
}

/**
 * Removes the given VM region from the task, if owned.
 *
 * @return Whether the VM region was successfully removed or not.
 */
bool Task::removeVmRegion(const rt::SharedPtr<vm::MapEntry> &region) {
    RW_LOCK_WRITE_GUARD(this->lock);
    bool removed = false;

    // set up info
    struct RemoveRegionInfo {
        rt::SharedPtr<vm::MapEntry> region = nullptr;
        bool *found = nullptr;
    };

    RemoveRegionInfo info{region, &removed};

    // iterate
    this->ownedRegions.removeMatching([](void *ctx, rt::SharedPtr<vm::MapEntry> &region) {
        auto info = reinterpret_cast<RemoveRegionInfo *>(ctx);

        if(region == info->region) {
            *info->found = true;
            return true;
        }
        return false;
    }, &info);

    // return whether we removed it
    return removed;
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
void Task::addThread(const rt::SharedPtr<Thread> &t) {
    RW_LOCK_WRITE_GUARD(this->lock);
    this->threads.push_back(t);

    bool yes = true;
    __atomic_store(&t->attachedToTask, &yes, __ATOMIC_RELEASE);

    // get a shared ptr
    auto ptr =  handle::Manager::getTask(this->handle);
    REQUIRE(ptr, "failed to get task");

    t->task = ptr;
}

/**
 * Detaches a thread from this task.
 *
 * This is only allowed if the thread is paused or in the zombie state. (or is it)
 */
void Task::detachThread(const rt::SharedPtr<Thread> &t) {
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
    log("thread %p does not belong to task %p (belongs to %p) attach %d!", static_cast<void *>(t), this, 
            static_cast<void *>(t->task), t->attachedToTask);
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
    Scheduler::get()->idle->queueDestroyTask(this->sharedFromThis());

    // finally, terminate calling thread, if it is also in this task
    if(current->task.get() == this) {
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
    log("Task $%p'h (%s) exited: %d", this->handle, this->name, status);
}

/**
 * Extract the currently executing task from the current thread.
 */
rt::SharedPtr<Task> Task::current() {
    auto thread = Thread::current();
    if(thread) {
        return thread->task;
    }
    return nullptr;
}

/**
 * Schedules all threads newly added to this task, and registers the task with the global scheduler
 * state info.
 */
void Task::launch() {
    const auto oldIrql = platform_raise_irql(platform::Irql::Scheduler);

    RW_LOCK_READ_GUARD(this->lock);
    for(auto &thread : this->threads) {
        // become runnable if needed
        if(thread->state == Thread::State::Paused) {
            thread->setState(Thread::State::Runnable);
        }

        // if thread is runnable, add it to the run queue
        if(thread->state != Thread::State::Runnable) continue;
        Scheduler::get()->markThreadAsRunnable(thread, false);
    }

    // register the task with the global state if needed
    if(!__atomic_test_and_set(&this->registered, __ATOMIC_RELAXED)) {
        GlobalState::the()->registerTask(this->sharedFromThis());
    }

    // restore irql
    platform_lower_irql(oldIrql);
}

/**
 * Inserts the given port to the task's port list. All ports that remain in this list when the
 * task is released will be released as well.
 */
void Task::addPort(const rt::SharedPtr<ipc::Port> &port) {
    RW_LOCK_WRITE_GUARD(this->lock);
    this->ports.append(port);
}

/**
 * Removes a port from the task's ports list.
 *
 * @return Whether the port existed in this task prior to the call.
 */
bool Task::removePort(const rt::SharedPtr<ipc::Port> &port) {
    RW_LOCK_WRITE_GUARD(this->lock);
    bool removed = false;

    // set up info
    struct RemovePortInfo {
        rt::SharedPtr<ipc::Port> port = nullptr;
        bool *found = nullptr;
    };

    RemovePortInfo info{port, &removed};

    // iterate
    this->ports.removeMatching([](void *ctx, rt::SharedPtr<ipc::Port> &port) {
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
bool Task::ownsPort(const rt::SharedPtr<ipc::Port> &port) {
    RW_LOCK_READ_GUARD(this->lock);
    for(auto i : this->ports) {
        if(i == port) {
            return true;
        }
    }
    return false;
}
