#include "GlobalState.h"

#include <log.h>

using namespace sched;

GlobalState *GlobalState::gShared = nullptr;

/**
 * Initializes the scheduler's shared global state.
 *
 * This will be called during scheduler setup for the BSP.
 */
void GlobalState::Init() {
    REQUIRE(!gShared, "cannot re-initialize scheduler");
    gShared = new GlobalState;
}

/**
 * Registers new tasks.
 */
void GlobalState::registerTask(Task *task) {
    SPIN_LOCK_GUARD(this->tasksLock);

    this->tasks.append(task);
}

/**
 * Removes a previously registered task.
 */
void GlobalState::unregisterTask(Task *task) {
    SPIN_LOCK_GUARD(this->tasksLock);

    this->tasks.removeMatching([](void *ctx, Task *task) {
        return (ctx == task);
    }, task);
}

/**
 * Invokes the callback for each task.
 */
void GlobalState::iterateTasks(void (*callback)(Task *)) {
    SPIN_LOCK_GUARD(this->tasksLock);

    for(auto task : this->tasks) {
        callback(task);
    }
}

