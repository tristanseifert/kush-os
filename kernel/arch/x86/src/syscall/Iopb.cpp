#include "Syscalls.h"
#include "Handler.h"

#include "gdt.h"
#include "sched/ThreadState.h"

#include <handle/Manager.h>
#include <sched/Task.h>
#include <sys/Syscall.h>

#include <arch/rwlock.h>

#include <log.h>

using namespace sys;
using namespace arch;

/**
 * Updates the IO bitmap of the given task.
 *
 * - Arg0: Task handle (or 0 for current task)
 * - Arg1: IO permission buffer (in userspace address)
 * - Arg2: IO permission buffer length/destination offset
 */
int syscall::UpdateTaskIopb(const ::sys::Syscall::Args *args, const uintptr_t) {
    int err;
    bool needsTssSwitch = false;

    sched::Task *task = nullptr;
    auto thread = sched::Thread::current();

    // get the task
    if(!args->args[0]) {
        task = thread->task;
    } else {
        task = handle::Manager::getTask(static_cast<Handle>(args->args[0]));
        if(!task) {
            return Errors::InvalidHandle;
        }
    }

    // ensure the entire IOPB array is accessible
    auto iopbBuf = reinterpret_cast<const uint8_t *>(args->args[1]);
    const auto iopbBits = (args->args[2] & 0xFFFF0000) >> 16;
    const auto iopbBytes = (iopbBits + 7) / 8;
    const auto offset = (args->args[2] & 0x0000FFFF);

    if(!Syscall::validateUserPtr(iopbBuf, iopbBytes)) {
        return Errors::InvalidPointer;
    }

    // take the task's lock
    RW_LOCK_WRITE_GUARD(task->lock);
    auto &ai = task->archState;

    // allocate a TSS for this task if needed
    if(!ai.hasTss) {
        err = tss_allocate(ai.tssIdx);
        if(err) {
            log("failed to allocate tss: %d", err);
            return Errors::GeneralError;
        }

        ai.hasTss = true;
        needsTssSwitch = true;
    }

    // update its IOPB
    log("writing to TSS %d at offset %u ptr %p bits %d", ai.tssIdx, offset, iopbBuf, iopbBits);
    tss_write_iopb(ai.tssIdx, offset, iopbBuf, iopbBits);

    const auto maxPort = iopbBits + offset;
    if(maxPort > ai.iopbBits) {
        ai.iopbBits = maxPort;
    }

    // switch to the task's TSS if it was freshly allocated (and it's currently executing)
    if(needsTssSwitch && thread->task == task) {
        tss_activate(ai.tssIdx, reinterpret_cast<uintptr_t>(thread->stack));
    }

    // if we get here, the IOPB was successfully updated
    return Errors::Success;
}
