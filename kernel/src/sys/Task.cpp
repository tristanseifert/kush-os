#include "Handlers.h"

#include <sched/Scheduler.h>
#include <sched/Task.h>

#include <log.h>

using namespace sys;

/**
 * Info structure for the "init task" DPC.
 */
struct InitTaskDpcInfo {
    // initial program counter and stack for the userspace thread
    uintptr_t pc, sp;

    InitTaskDpcInfo() = default;
    InitTaskDpcInfo(const uintptr_t entry, const uintptr_t stack) : pc(entry), sp(stack) {}
};

/**
 * Implements the "initialize task" syscall.
 *
 * This will invoke all kernel handlers that are interested in new tasks being created, finish
 * setting up some kernel structures, then perform a return to userspace to the specified address
 * and stack.
 *
 * Note that at the point that this syscall returns, the task may not actually have started
 * executing; and even if it has, it may have failed at some initialization stage in userspace. If
 * you're interested in determining that the process properly started, wait on the task object and
 * see if an unexpected exit code is returned before looking up the task handle.
 */
int sys::TaskInitialize(const Syscall::Args *args, const uintptr_t number) {
    int err;

    // get the task handle
    sched::Task *task = nullptr;

    // queue a DPC to its main thread
    auto info = new InitTaskDpcInfo(args->args[1], args->args[2]);

    auto main = task->threads[0];
    err = main->addDpc([](sched::Thread *thread, void *ctx) {
        // get the info
        auto info = reinterpret_cast<InitTaskDpcInfo *>(ctx);
        const auto [pc, sp] = *info;
        delete info;

        // set up the thread to perform a return to userspace
        log("return to userspace for %p: pc %08x sp %08x", thread, pc, sp);
    }, info);

    // return the success status of whether we could add the DPC
    return (!err ? 0 : -1);
}
