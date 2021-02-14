#include "Handlers.h"

#include "sched/Scheduler.h"
#include "sched/Thread.h"

#include "handle/Manager.h"

#include <arch.h>
#include <log.h>

/**
 * Sets the thread's new name.
 */
int sys::ThreadSetName(const Syscall::Args *args, const uintptr_t number) {
    sched::Thread *thread = nullptr;

    // get the task
    if(!args->args[0]) {
        thread = sched::Thread::current();
    } else {
        thread = handle::Manager::getThread(static_cast<Handle>(args->args[0]));
        if(!thread) {
            return Errors::InvalidHandle;
        }
    }

    // validate the user pointer
    auto namePtr = reinterpret_cast<const char *>(args->args[1]);
    const auto nameLen = args->args[2];
    if(!Syscall::validateUserPtr(namePtr, nameLen)) {
        return Errors::InvalidPointer;
    }

    // set it
    thread->setName(namePtr, nameLen);

    return Errors::Success;
}
