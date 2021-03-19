#include "Handlers.h"

#include "sched/Scheduler.h"
#include "sched/Task.h"
#include "sched/Thread.h"
#include "vm/Map.h"
#include "handle/Manager.h"

#include <arch.h>
#include <log.h>

/// Thread creation flags
ENUM_FLAGS(ThreadCreateFlags)
enum class ThreadCreateFlags {
    /// No set flags
    None                                = 0,
    /// Default thread flags
    Default                             = None,

    /// The thread should be created as suspended
    StartSuspended                      = (1 << 15),
};

static ThreadCreateFlags ConvertFlags(const uintptr_t inFlags);

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
intptr_t sys::ThreadCreate(const Syscall::Args *args, const uintptr_t number) {
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

    // convert the flags
    const auto rawFlags = (number & 0xFFFF0000) >> 16;
    const auto flags = ConvertFlags(rawFlags);

    // create a new thread
    auto info = new InitThreadInfo(args->args[0], args->args[1], args->args[2]);

    auto thread = sched::Thread::kernelThread(running->task, &CreateThreadEntryStub,
            (uintptr_t) info);
    thread->kernelMode = false;

    // ensure it gets scheduled
    if(TestFlags(flags & ThreadCreateFlags::StartSuspended)) {
        thread->setState(sched::Thread::State::Paused);
    } else {
        thread->setState(sched::Thread::State::Runnable);
        sched::Scheduler::get()->markThreadAsRunnable(thread);
    }

    // return handle of newly created thread
    return static_cast<intptr_t>(thread->handle);
}

/**
 * Destroys an userspace thread.
 *
 * @note To guard against mis-use, you cannot use the shorthand of the thread handle 0 = current
 * thread.
 */
intptr_t sys::ThreadDestroy(const Syscall::Args *args, const uintptr_t number) {
    sched::Thread *thread = nullptr;

    // get the thread
    if(!args->args[0]) {
        return Errors::InvalidHandle;
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
 * Sets the priority of the thread in the first argument to the value in the second. It must be a
 * value on [-100, 100].
 */
intptr_t sys::ThreadSetPriority(const Syscall::Args *args, const uintptr_t number) {
    sched::Thread *thread = nullptr;

    // get the thread
    if(!args->args[0]) {
        thread = sched::Thread::current();
    } else {
        thread = handle::Manager::getThread(static_cast<Handle>(args->args[0]));
        if(!thread) {
            return Errors::InvalidHandle;
        }
    }

    // validate priority and set it
    int priority = static_cast<int>(args->args[1]);

    if(priority < -100 || priority > 100) {
        return Errors::InvalidArgument;
    }

    thread->setPriority(priority);
    return Errors::Success;
}

/**
 * Sets the notification mask of the specified thread.
 */
intptr_t sys::ThreadSetNoteMask(const Syscall::Args *args, const uintptr_t number) {
    sched::Thread *thread = nullptr;

    // get the thread
    if(!args->args[0]) {
        thread = sched::Thread::current();
    } else {
        thread = handle::Manager::getThread(static_cast<Handle>(args->args[0]));
        if(!thread) {
            return Errors::InvalidHandle;
        }
    }

    // validate priority and set it
    thread->setNotificationMask(args->args[1]);
    return Errors::Success;
}
/**
 * Sets the thread's new name.
 */
intptr_t sys::ThreadSetName(const Syscall::Args *args, const uintptr_t number) {
    sched::Thread *thread = nullptr;

    // get the thread
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
 * Resumes a currently paused thread.
 */
intptr_t sys::ThreadResume(const Syscall::Args *args, const uintptr_t number) {
    // look up the thread
    auto thread = handle::Manager::getThread(static_cast<Handle>(args->args[0]));
    if(!thread) {
        return Errors::InvalidHandle;
    }

    // ensure it's paused
    if(thread->getState() != sched::Thread::State::Paused) {
        return Errors::InvalidState;
    }

    // go ahead and resume it
    thread->setState(sched::Thread::State::Runnable);
    sched::Scheduler::get()->markThreadAsRunnable(thread);

    return Errors::Success;
}

/**
 * Waits for the given thread to terminate.
 */
intptr_t sys::ThreadJoin(const Syscall::Args *args, const uintptr_t number) {
    int err;

    // look up the thread; reject if it's the currently running thread
    auto thread = handle::Manager::getThread(static_cast<Handle>(args->args[0]));
    if(!thread) {
        return Errors::InvalidHandle;
    }

    if(thread == sched::Thread::current()) {
        return Errors::DeadlockPrevented;
    }

    // get the time to wait
    uint64_t nanos = 0;

    if(args->args[1] != UINTPTR_MAX) {
        nanos = platform_timer_now() + (args->args[1] * 1000ULL);
    }

    // wait for thread termination
    err = thread->waitOn(nanos);

    if(!err) { // 0 = thread terminated
        return Errors::Success;
    }
    else if(err == 1) { // 1 == timeout expired
        return Errors::Timeout;
    }
    else { // other error
        return Errors::GeneralError;
    }
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

    log("entry stub %p: pc %p sp %p arg %p", _ctx, pc, sp, arg);

    // perform return to userspace
    thread->returnToUser(pc, sp, arg);
}

/**
 * Converts the flag argument of the "create thread" syscall.
 */
static ThreadCreateFlags ConvertFlags(const uintptr_t inFlags) {
    ThreadCreateFlags flags = ThreadCreateFlags::None;

    if(inFlags & (1 << 15)) {
        flags |= ThreadCreateFlags::StartSuspended;
    }

    return flags;
}
