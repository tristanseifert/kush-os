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
 * Returns the task handle of the currently executing task.
 */
intptr_t sys::TaskGetHandle() {
    auto thread = sched::Thread::current();
    if(!thread || !thread->task) {
        return Errors::GeneralError;
    }
    return static_cast<intptr_t>(thread->task->handle);
}

/**
 * Allocates a new task.
 *
 * @param parentTaskHandle Task to register this task as a child to, or 0 for current task
 *
 * @return Negative error code, or a valid task handle to the newly created task
 */
intptr_t sys::TaskCreate(const Handle parentTaskHandle) {
    rt::SharedPtr<sched::Task> parent = nullptr;

    // set the parent
    if(!parentTaskHandle) {
        parent = sched::Task::current();
        REQUIRE(parent, "no current task wtf");
    } else {
        parent = handle::Manager::getTask(parentTaskHandle);
        if(!parent) {
            return Errors::InvalidHandle;
        }
    }
    if(!parent) {
        return Errors::GeneralError;
    }

    // allocate a task
    auto task = sched::Task::alloc();
    if(!task) {
        return Errors::NoMemory;
    }

    /*
     * We need to register the task with the scheduler so it gets stored into the global task
     * list, so a strong reference to the task remain after this call returns. Otherwise, it will
     * be deallocated since the handle manager only holds a weak ref.
     */
    sched::Scheduler::get()->scheduleRunnable(task);

    // return task handle
    return static_cast<intptr_t>(task->handle);
}

/**
 * Terminates a task, setting its exit code.
 *
 * The first argument is the task handle; if zero, the current task is terminated. The second
 * argument specifies the return code.
 *
 * @param taskHandle Handle of the task to terminate, or 0 for current task
 * @param code Return code to associate with termination
 *
 * @return A negative error code or 0 if success
 */
intptr_t sys::TaskTerminate(const Handle taskHandle, const intptr_t code) {
    int err;
    rt::SharedPtr<sched::Task> task = nullptr;

    // get the task
    if(!taskHandle) {
        task = sched::Task::current();
    } else {
        task = handle::Manager::getTask(taskHandle);
        if(!task) {
            return Errors::InvalidHandle;
        }
    }

    // terminate it aye
    log("Terminating task %p (code %d)", static_cast<void *>(task), code);
    err = task->terminate(code);

    return (err ? Errors::GeneralError : Errors::Success);
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
 *
 * @param taskHandle Task to initialize
 * @param userPc Address to begin execution at in the task's address space
 * @param userStack Address of the bottom of the stack in the task's address space
 *
 * @return 0 if the task was successfully started, or a negative error code.
 */
intptr_t sys::TaskInitialize(const Handle taskHandle, const uintptr_t userPc,
        const uintptr_t userStack) {
    int err;

    // get the task handle
    auto task = handle::Manager::getTask(taskHandle);
    if(!task) {
        return Errors::InvalidHandle;
    }

    auto info = new InitTaskDpcInfo(userPc, userStack);

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
 *
 * @param taskHandle Task handle for the task to rename, or 0 for current task
 * @param namePtr Pointer to a string (does NOT have to be zero terminated) to use as new name
 * @param nameLen Total number of bytes of name to copy
 *
 * @return A negative error code or 0 if success
 */
intptr_t sys::TaskSetName(const Handle taskHandle, const char *namePtr, const size_t nameLen) {
    rt::SharedPtr<sched::Task> task;

    // get the task
    if(!taskHandle) {
        task = sched::Task::current();
    } else {
        task = handle::Manager::getTask(taskHandle);
    }

    if(!task) {
        return Errors::InvalidHandle;
    }

    // validate the user pointer
    if(!Syscall::validateUserPtr(namePtr, nameLen)) {
        return Errors::InvalidPointer;
    }

    // set it
    char buffer[256]{0};
    Syscall::copyIn(namePtr, nameLen, &buffer, 255);
    task->setName(buffer, (nameLen > 255) ? 255 : nameLen);

    return Errors::Success;
}

/**
 * Writes a zero-terminated message to the kernel's debug console.
 *
 * @param msgPtr Pointer to message buffer
 * @param msgLen Number of characters of message data to print
 *
 * @return A negative error code or 0 if success
 */
intptr_t sys::TaskDbgOut(const char *msgPtr, const size_t msgLen) {
    // validate the user pointer
    if(!Syscall::validateUserPtr(msgPtr, msgLen)) {
        return Errors::InvalidPointer;
    }

    // copy the message
    char message[1024];
    memset(&message, 0, 1024);

    Syscall::copyIn(msgPtr, msgLen, &message, sizeof(message));
    // strncpy(message, msgPtr, 1024);

    // print it
    DECLARE_CRITICAL();
    CRITICAL_ENTER();
    log("%4lu %4lu) %s", sched::Task::current()->pid, sched::Thread::current()->tid, message);
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
    log("return to userspace for %p: pc %08x sp %08x", static_cast<void *>(thread), pc, sp);
    thread->returnToUser(pc, sp);
}
