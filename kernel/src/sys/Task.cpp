#include "Handlers.h"

#include "sched/Scheduler.h"
#include "sched/Task.h"

#include "handle/Manager.h"

#include <arch.h>
#include <arch/critical.h>
#include <log.h>

using namespace sys;

static void UserspaceThreadStub(uintptr_t arg);

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
 * Allocates a new task.
 */
int sys::TaskCreate(const Syscall::Args *args, const uintptr_t number) {
    auto task = sched::Task::alloc();
    log("alloc task syscall: %p", task);

    // insert task into handle table

    // return task handle
    return -1;
}

/**
 * Terminates a task, setting its exit code.
 *
 * The first argument is the task handle; if zero, the current task is terminated. The second
 * argument specifies the return code.
 */
int sys::TaskTerminate(const Syscall::Args *args, const uintptr_t number) {
    sched::Task *task = nullptr;

    // get the task
    if(!args->args[0]) {
        task = sched::Thread::current()->task;
    } else {
        task = handle::Manager::getTask(static_cast<Handle>(args->args[0]));
        if(!task) {
            return Errors::InvalidHandle;
        }
    }

    // terminate it aye
    log("Terminating task %p (code %d)", task, args->args[1]);
    return Errors::Success;
}

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
    auto task = handle::Manager::getTask(static_cast<Handle>(args->args[0]));
    if(!task) {
        return Errors::InvalidHandle;
    }

    auto info = new InitTaskDpcInfo(args->args[1], args->args[2]);

    // set up the main thread
    auto main = sched::Thread::kernelThread(task, &UserspaceThreadStub, (uintptr_t) info);
    main->setName("Main Thread");
    main->kernelMode = false;

    // queue a DPC to perform last minute setup
    err = main->addDpc([](sched::Thread *thread, void *ctx) {
        // map the syscall tables
        arch::TaskWillStart(thread->task);
    });

    // schedule the task
    sched::Scheduler::get()->scheduleRunnable(task);

    // return the success status of whether we could add the DPC
    return (!err ? Errors::Success : Errors::GeneralError);
}

/**
 * Sets the task's new name.
 */
int sys::TaskSetName(const Syscall::Args *args, const uintptr_t number) {
    sched::Task *task = nullptr;

    // get the task
    if(!args->args[0]) {
        task = sched::Thread::current()->task;
    } else {
        task = handle::Manager::getTask(static_cast<Handle>(args->args[0]));
        if(!task) {
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
    task->setName(namePtr, nameLen);

    return Errors::Success;
}

int sys::TaskDbgOut(const Syscall::Args *args, const uintptr_t number) {
    // validate the user pointer
    auto namePtr = reinterpret_cast<const char *>(args->args[0]);
    const auto nameLen = args->args[1];
    if(!Syscall::validateUserPtr(namePtr, nameLen)) {
        return Errors::InvalidPointer;
    }

    // copy the message
    char message[1024];
    memset(&message, 0, 1024);
    strncpy(message, namePtr, 1024);

    // set it
    DECLARE_CRITICAL();
    CRITICAL_ENTER();
    log("%15s) %s", sched::Thread::current()->name, message);
    CRITICAL_EXIT();

    return Errors::Success;
}



/**
 * Entry point for new userspace threads
 */
static void UserspaceThreadStub(uintptr_t arg) {
    auto thread = sched::Thread::current();

    // get the info
    auto info = reinterpret_cast<InitTaskDpcInfo *>(arg);
    const auto [pc, sp] = *info;
    delete info;

    // execute return to userspace
    log("return to userspace for %p: pc %08x sp %08x", thread, pc, sp);
    thread->returnToUser(pc, sp);
}
