#include "Syscalls.h"
#include "Handler.h"

#include "gdt.h"
#include "sched/ThreadState.h"

#include <handle/Manager.h>
#include <sched/Thread.h>
#include <sys/Syscall.h>

#include <arch/rwlock.h>

#include <log.h>

using namespace sys;
using namespace arch;

/**
 * Updates the thread-local base (the base of the %gs segment) for the current thread.
 *
 * - Arg0: Thread handle (or 0 for current thread)
 * - Arg1: Value for thread local storage base
 *
 * XXX: What validation, if any, do we need to do on the base address? Seems like paging should
 * protect us from everything.
 */
int syscall::UpdateThreadTlsBase(const ::sys::Syscall::Args *args, const uintptr_t) {
    sched::Thread *task = nullptr;
    auto thread = sched::Thread::current();

    // get the task
    if(args->args[0]) {
        task = handle::Manager::getThread(static_cast<Handle>(args->args[0]));
        if(!task) {
            return Errors::InvalidHandle;
        }
    }

    // validate base address (TODO: implement?)
    const uintptr_t base = args->args[1];

    // take the thread's lock and update the gs value
    RW_LOCK_WRITE_GUARD(thread->lock);
    auto &ai = thread->regs;

    ai.gsBase = base;

    // log("Updated %%gs for '%s': %08x", thread->name, base);

    /**
     * Reload the %gs register (by rewriting the segment AND forcing the register to be loaded
     * again) for the calling thread.
     * 
     * We then immediately reload the %gs register with the desired value so the descriptor is
     * updated. This is required since on x86, neither the SYSEXIT instruction nor our syscall
     * entry stubs ever save (and thus, restore) the %gs register.
     */
    if(thread == sched::Thread::current()) {
        // update the segment base
        gdt_update_tls_user(ai.gsBase);

        // reload register
        asm volatile("mov %0, %%gs" :: "c"(GDT_USER_TLS_SEG | 3));
    }

    return Errors::Success;
}
