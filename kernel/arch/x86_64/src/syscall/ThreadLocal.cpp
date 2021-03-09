#include "Syscalls.h"
#include "Handler.h"

#include "sched/ThreadState.h"

#include <handle/Manager.h>
#include <sched/Thread.h>
#include <sys/Syscall.h>

#include <arch/rwlock.h>

#include <log.h>

using namespace sys;
using namespace arch;

/**
 * Updates the thread-local base (the base of the %fs/%gs segment) for the current thread.
 *
 * - Arg0: Thread handle (or 0 for current thread)
 * - Arg1: Value for thread local storage base (%fs or %gs)
 * - Arg2: Whether we want to set %fs (0) or %gs (1)
 *
 * XXX: What validation, if any, do we need to do on the base address? Seems like paging should
 * protect us from everything.
 */
intptr_t syscall::UpdateThreadTlsBase(const ::sys::Syscall::Args *args, const uintptr_t) {
    auto thread = sched::Thread::current();

    // get the thread
    if(args->args[0]) {
        thread = handle::Manager::getThread(static_cast<Handle>(args->args[0]));
        if(!thread) {
            return Errors::InvalidHandle;
        }
    }

    // validate base address (TODO: implement?)
    const uintptr_t base = args->args[1];

    // take the thread's lock and update the gs value
    RW_LOCK_WRITE_GUARD(thread->lock);
    auto &ai = thread->regs;

    switch(args->args[2]) {
        case 0:
            ai.fsBase = base;
            break;
        case 1:
            ai.gsBase = base;
            break;
        default:
            return Errors::InvalidArgument;
    }

    /**
     * Reload the %fs and %gs base addresses for the thread if this is the current thread.
     */
    if(thread == sched::Thread::current()) {
        // TODO: implement lmfao
    }

    return Errors::Success;
}
