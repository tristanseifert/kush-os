#include "Handlers.h"

#include "sched/Scheduler.h"
#include "sched/Task.h"
#include "sched/Thread.h"

#include "handle/Manager.h"

#include <arch.h>
#include <arch/critical.h>
#include <platform.h>
#include <log.h>

using namespace sys;

/**
 * Sends a notification to a thread.
 *
 * - Arg0: Thread handle
 * - Arg1: Notification bits to send
 */
int sys::NotifySend(const Syscall::Args *args, const uintptr_t number) {
    // resolve thread handle
    auto thread = handle::Manager::getThread(static_cast<Handle>(args->args[0]));
    if(!thread) {
        return Errors::InvalidHandle;
    }

    // send notification to it
    const auto bits = args->args[1];
    if(!bits) {
        return Errors::InvalidArgument;
    }

    thread->notify(bits);

    return Errors::Success;
}

/**
 * Blocks the calling thread waiting to receive notifications.
 *
 * - Arg0: Notification mask to use, a value of 0 is equivalent to setting all bits
 */
int sys::NotifyReceive(const Syscall::Args *args, const uintptr_t number) {
    auto thread = sched::Thread::current();

    // get mask
    auto mask = args->args[0];
    if(!mask) {
        mask = UINTPTR_MAX;
    }

    // return the notify wait result
    return thread->blockNotify(mask);
}
