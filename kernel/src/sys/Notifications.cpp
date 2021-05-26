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
 * @param threadHandle Thread to notify
 * @param bits Notification bits to set
 *
 * @return 0 if notification was sent, negative error code otherwise
 */
intptr_t sys::NotifySend(const Handle threadHandle, const uintptr_t bits) {
    // validate arguments
    if(!bits) {
        return Errors::InvalidArgument;
    }

    // resolve thread handle
    auto thread = handle::Manager::getThread(threadHandle);
    if(!thread) {
        return Errors::InvalidHandle;
    }

    // send notification to it
    thread->notify(bits);
    return Errors::Success;
}

/**
 * Blocks the calling thread waiting to receive notifications.
 *
 * @param mask Notification mask to set; 0 is equivalent to all bits set
 *
 * @return Notify bits that were set when the thread was unblocked
 */
intptr_t sys::NotifyReceive(uintptr_t mask) {
    auto thread = sched::Thread::current();

    // get mask
    if(!mask) {
        mask = UINTPTR_MAX;
    }

    // return the notify wait result
    return thread->blockNotify(mask);
}
