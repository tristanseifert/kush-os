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
 * Return the current thread's handle.
 */
intptr_t sys::ThreadGetHandle() {
    auto thread = sched::Thread::current();
    if(!thread) {
        return Errors::GeneralError;
    }
    return static_cast<intptr_t>(thread->handle);
}

/**
 * Gives up the remainder of the thread's time quantum, allowing other threads a chance to run.
 */
intptr_t sys::ThreadYield() {
    sched::Thread::yield();
    return Errors::Success;
}

/**
 * Sleeps the calling thread for the given number of microseconds.
 */
intptr_t sys::ThreadUsleep(const uintptr_t usecs) {
    // validate args
    const uint64_t sleepNs = 1000ULL * usecs;
    if(!sleepNs) return Errors::InvalidArgument;

    // call sleep handler
    sched::Thread::sleep(sleepNs);
    return Errors::Success;
}

/**
 * Creates a new userspace thread.
 *
 * @param entryPtr Userspace entry point address
 * @param entryParam Parameter to pass as an argument to the entry point
 * @param stackPtr Userspace stack pointer
 * @param flags Thread creation flags
 *
 * @return Valid thread handle for newly created thread or negative error code
 */
intptr_t sys::ThreadCreate(const uintptr_t entryPtr, const uintptr_t entryParam,
        const uintptr_t stackPtr, const uintptr_t rawFlags) {
    // get running task
    auto running = sched::Thread::current();
    if(!running || !running->task) {
        return Errors::GeneralError;
    }

    // validate the stack and entry point addresses
    if(!Syscall::validateUserPtr(stackPtr)) {
        return Errors::InvalidPointer;
    }

    if(!Syscall::validateUserPtr(entryPtr)) {
        return Errors::InvalidPointer;
    }

    // convert the flags
    const auto flags = ConvertFlags(rawFlags);

    // create a new thread
    auto info = new InitThreadInfo(entryPtr, entryParam, stackPtr);

    auto thread = sched::Thread::userThread(running->task, &CreateThreadEntryStub,
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
 * @param threadHandle Handle to the thread to destroy
 *
 * @return 0 on success or a negative error code
 *
 * @note To guard against mis-use, you cannot use the shorthand of the thread handle 0 = current
 * thread.
 */
intptr_t sys::ThreadDestroy(const Handle threadHandle) {
    rt::SharedPtr<sched::Thread> thread = nullptr;

    if(!threadHandle) {
        return Errors::InvalidHandle;
    }

    // get the thread
    thread = handle::Manager::getThread(threadHandle);
    if(!thread) {
        return Errors::InvalidHandle;
    }

    // terminate it
    thread->terminate();

    return Errors::Success;
}

/**
 * Sets a thread's priority modifier, which is an integer in the range [-100, 100].
 *
 * @param threadHandle Handle to thread whose priority to change, or 0 for current thread
 * @param priority Priority modifier to apply
 *
 * @return 0 on success or negative error code
 */
intptr_t sys::ThreadSetPriority(const Handle threadHandle, const intptr_t priority) {
    rt::SharedPtr<sched::Thread> thread = nullptr;

    // get the thread
    if(!threadHandle) {
        thread = sched::Thread::current();
    } else {
        thread = handle::Manager::getThread(threadHandle);
        if(!thread) {
            return Errors::InvalidHandle;
        }
    }

    // validate priority and set it
    if(priority < -100 || priority > 100) {
        return Errors::InvalidArgument;
    }

    thread->setPriority(priority);
    return Errors::Success;
}

/**
 * Sets the notification mask of the specified thread.
 *
 * @param threadHandle Handle to thread whose notification mask is to be updated (or 0 for current)
 * @param newMask New value for the thread's notification mask
 *
 * @return 0 on success, or negative error code
 */
intptr_t sys::ThreadSetNoteMask(const Handle threadHandle, const uintptr_t newMask) {
    rt::SharedPtr<sched::Thread> thread = nullptr;

    // get the thread
    if(!threadHandle) {
        thread = sched::Thread::current();
    } else {
        thread = handle::Manager::getThread(threadHandle);
        if(!thread) {
            return Errors::InvalidHandle;
        }
    }

    // validate priority and set it
    thread->setNotificationMask(newMask);
    return Errors::Success;
}
/**
 * Sets the thread's new name.
 *
 * @param threadHandle Handle of thread to rename, or 0 for current thread
 * @param namePtr Pointer to name string
 * @param nameLen Number of characters in name string to copy
 *
 * @return 0 on success, or negative error code
 */
intptr_t sys::ThreadSetName(const Handle threadHandle, const char *namePtr, const size_t nameLen) {
    rt::SharedPtr<sched::Thread> thread = nullptr;

    // get the thread
    if(!threadHandle) {
        thread = sched::Thread::current();
    } else {
        thread = handle::Manager::getThread(threadHandle);
        if(!thread) {
            return Errors::InvalidHandle;
        }
    }

    // validate the user pointer
    if(!Syscall::validateUserPtr(namePtr, nameLen)) {
        return Errors::InvalidPointer;
    }

    // set it
    thread->setName(namePtr, nameLen);
    return Errors::Success;
}

/**
 * Resumes a currently paused thread.
 *
 * @param threadHandle Handle of thread to resume; it must be in the paused state
 *
 * @return 0 on success, or negative error code
 */
intptr_t sys::ThreadResume(const Handle threadHandle) {
    // look up the thread
    auto thread = handle::Manager::getThread(threadHandle);
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
 *
 * @param threadHandle Thread to wait on
 * @param timeout How long to block (in nsec); 0 to poll, or UINTPTR_MAX to wait forever
 *
 * @return 0 if thread terminated, or an appropriate error code.
 */
intptr_t sys::ThreadJoin(const Handle threadHandle, const uintptr_t timeout) {
    int err;

    // look up the thread; reject if it's the currently running thread
    auto thread = handle::Manager::getThread(threadHandle);
    if(!thread) {
        return Errors::InvalidHandle;
    }

    if(thread == sched::Thread::current()) {
        return Errors::DeadlockPrevented;
    }

    // has the thread already terminated?
    if(thread->needsToDie) {
        return Errors::Success;
    }

    // get the time to wait
    uint64_t nanos = 0;

    if(timeout != UINTPTR_MAX) {
        nanos = platform_timer_now() + (timeout * 1000ULL);
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

    //log("entry stub %p ($%p'h): pc %p sp %p arg %p", _ctx, thread->handle, pc, sp, arg);

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
