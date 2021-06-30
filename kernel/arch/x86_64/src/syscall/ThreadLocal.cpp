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

/// are thread local updates logged?
static bool gLogTls = false;

/**
 * Updates the thread-local base (the base of the %fs/%gs segment) for the current thread.
 *
 * @param threadHandle Thread handle (or 0 for current thread) to update the TLS base for
 * @param gs Whether we want to set the %gs base (true) or %fs (false)
 * @param base Base address for the new thread local storage
 *
 * @return 0 on success, negative error code otherwise
 *
 * XXX: What validation, if any, do we need to do on the base address? Seems like paging should
 * protect us from everything.
 */
intptr_t syscall::UpdateThreadTlsBase(const uintptr_t threadHandle, const bool gs,
        const uintptr_t base) {
    auto thread = sched::Thread::current();

    if(gs) {
        return Errors::InvalidArgument;
    }

    // get the thread
    if(threadHandle) {
        thread = handle::Manager::getThread(static_cast<Handle>(threadHandle));
        if(!thread) {
            return Errors::InvalidHandle;
        }
    }

    // validate base address (TODO: implement?)
    if(gLogTls) {
        log("Setting thread $%p'h %s base to %p", thread->handle, gs ? "gs" : "fs", base);
    }

    // take the thread's lock and update the gs value
    RW_LOCK_WRITE_GUARD(thread->lock);
    auto &ai = thread->regs;

    if(gs) {
        ai.gsBase = base;
    } else {
        ai.fsBase = base;
    }

    /**
     * Reload the %fs and %gs base addresses for the thread if this is the current thread.
     */
    if(thread == sched::Thread::current()) {
        // we can reload %fs immediately as the kernel doesn't use it
        if(!gs) {
            x86_msr_write(X86_MSR_FSBASE, (base), (base) >> 32ULL);
        }
        // but %gs we just haven't implemented yet
        else {
            panic("cannot auto-reload %%gs yet!");
        }
    }

    return Errors::Success;
}
