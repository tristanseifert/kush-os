#include "Handlers.h"

#include "sched/Scheduler.h"
#include "sched/Task.h"
#include "sched/Thread.h"
#include "vm/Map.h"
#include "handle/Manager.h"

#include <arch.h>
#include <log.h>

/// Info passed to a new userspace thread kernel stub
struct InitThreadInfo {
    uintptr_t entry, entryArg, stack;

    InitThreadInfo() = default;
    InitThreadInfo(const uintptr_t _entry, const uintptr_t _arg, const uintptr_t _stack) :
        entry(_entry), entryArg(_arg), stack(_stack) {}
};

static void CreateThreadEntryStub(uintptr_t arg);


/**
 * Creates a new userspace thread.
 */
int sys::ThreadCreate(const Syscall::Args *args, const uintptr_t number) {
    // get running task
    auto running = sched::Thread::current();
    if(!running || !running->task) {
        return Errors::GeneralError;
    }

    // validate the stack and entry point addresses
    auto stackPtr = reinterpret_cast<const char *>(args->args[2]);
    if(!Syscall::validateUserPtr(stackPtr)) {
        return Errors::InvalidPointer;
    }

    auto entryPtr = reinterpret_cast<const char *>(args->args[0]);
    if(!Syscall::validateUserPtr(entryPtr)) {
        return Errors::InvalidPointer;
    }

    // create a new thread
    auto info = new InitThreadInfo(args->args[0], args->args[1], args->args[2]);

    auto thread = sched::Thread::kernelThread(running->task, &CreateThreadEntryStub,
            (uintptr_t) info);
    thread->kernelMode = false;

    // ensure it gets scheduled
    thread->setState(sched::Thread::State::Runnable);
    sched::Scheduler::get()->markThreadAsRunnable(thread);

    // return handle of newly created thread
    return static_cast<int>(thread->handle);
}

/**
 * Destroys an userspace thread.
 */
int sys::ThreadDestroy(const Syscall::Args *args, const uintptr_t number) {
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

    // terminate it
    thread->terminate();

    return Errors::Success;
}

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



/**
 * Entry point stub for threads created from userspace.
 *
 * This will return to the specified userspace address.
 */
static void CreateThreadEntryStub(uintptr_t _ctx) {
    auto thread = sched::Thread::current();

    // extract info
    auto info = reinterpret_cast<const InitThreadInfo *>(_ctx);
    REQUIRE(info, "invalid thread info");

    const auto [pc, arg, sp] = *info;
    delete info;

    // perform return to userspace
    thread->returnToUser(pc, sp, arg);
}
