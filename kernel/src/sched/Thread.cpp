#include "Thread.h"
#include "Blockable.h"
#include "SignalFlag.h"
#include "Scheduler.h"
#include "Task.h"
#include "IdleWorker.h"

#include "TimerBlocker.h"

#include "ipc/Interrupts.h"
#include "mem/StackPool.h"

#include <arch.h>
#include <arch/critical.h>

#include <platform.h>
#include <string.h>

using namespace sched;

/// Thread id for the next thread
uintptr_t Thread::nextTid = 1;

bool Thread::gLogLifecycle = false;

/**
 * Allocates a new thread.
 */
rt::SharedPtr<Thread> Thread::kernelThread(rt::SharedPtr<Task> &parent, void (*entry)(uintptr_t),
        const uintptr_t param) {
    auto ptr = rt::MakeShared<Thread>(parent, (uintptr_t) entry, param, true);
    ptr->handle = handle::Manager::makeThreadHandle(ptr);

    // add to the parent task
    parent->addThread(ptr);
    return ptr;
}

/**
 * Allocates a new thread.
 */
rt::SharedPtr<Thread> Thread::userThread(rt::SharedPtr<Task> &parent, void (*entry)(uintptr_t),
        const uintptr_t param) {
    auto ptr = rt::MakeShared<Thread>(parent, (uintptr_t) entry, param, false);
    ptr->handle = handle::Manager::makeThreadHandle(ptr);

    // add to the parent task
    parent->addThread(ptr);
    return ptr;
}


/**
 * Allocates a new thread.
 */
Thread::Thread(rt::SharedPtr<Task> &_parent, const uintptr_t pc, const uintptr_t param,
        const bool _kernel) : task(_parent), kernelMode(_kernel) {
    this->tid = nextTid++;

    // get a kernel stack
    this->stack = mem::StackPool::get(this->stackSize);
    REQUIRE(this->stack, "failed to get stack for thread %p", this);

    if(gLogLifecycle) {
        log("* alloc thread $%p'h (%lu) stack $%p parent $%p'h", this->handle, this->tid,
                this->stack, this->task->handle);
    }

    // then initialize thread state
    arch::InitThreadState(this, pc, param);

    Scheduler::threadWasCreated(*this);
}

/**
 * Destroys all resources associated with this thread.
 */
Thread::~Thread() {
    if(gLogLifecycle) {
        log("* dealloc thread $%p'h (%lu)", this->handle, this->tid);
    }

    // remove IRQ handlers
    this->irqHandlers.clear();

    // invoke termination handlers
    while(!this->terminateSignals.empty()) {
        auto signal = this->terminateSignals.pop();
        signal->signal();
    }

    // remove all objects we're blocking on
    if(this->blockingOn) {
        this->blockingOn->reset();
    }

    // invalidate the handle
    handle::Manager::releaseThreadHandle(this->handle);

    // release kernel stack
    mem::StackPool::release(this->stack);
}

/**
 * Returns the currently executing thread.
 */
rt::SharedPtr<Thread> Thread::current() {
    return Scheduler::get()->runningThread();
}

/**
 * Updates internal tracking structures when the thread is context switched out.
 */
void Thread::switchFrom() {
    if(this->lastSwitchedTo) {
        //const auto ran = platform_timer_now() - this->lastSwitchedTo;
        //__atomic_fetch_add(&this->cpuTime, ran, __ATOMIC_RELAXED);
    }

    // if we've DPCs, execute them
    bool haveDpcs;
    __atomic_load(&this->dpcsPending, &haveDpcs, __ATOMIC_CONSUME);

    if(haveDpcs) {
        this->runDpcs();
    }

    // terminate
    if(this->needsToDie) {
        this->deferredTerminate();
    }
}

/**
 * Performs a context switch to this thread.
 *
 * If we're currently executing on a thread, its state is saved, and the function will return when
 * that thread is switched back in. Otherwise, the function never returns.
 */
void Thread::switchTo() {
    auto current = Scheduler::get()->runningThread();

    if(current) {
        current->switchFrom();
    }

    this->lastSwitchedTo = platform_timer_now();

    auto to = handle::Manager::getThread(this->handle);
    //log("switching to %s (from %s)", this->name, current ? (current->name) : "<null>");
    Scheduler::get()->setRunningThread(to);
    arch::RestoreThreadState(current, to);
}

/**
 * Call into architecture code to return to user mode.
 */
void Thread::returnToUser(const uintptr_t pc, const uintptr_t stack, const uintptr_t arg) {
    arch::ReturnToUser(pc, stack, arg);
}


/**
 * Copies the given name string to the thread's name field.
 */
void Thread::setName(const char *newName, const size_t _inLength) {
    RW_LOCK_WRITE_GUARD(this->lock);

    memset(this->name, 0, kNameLength);

    if(!_inLength) {
        strncpy(this->name, newName, kNameLength);
    } else {
        const auto toCopy = (_inLength >= kNameLength) ? kNameLength : _inLength;
        memcpy(this->name, newName, toCopy);
    }
}

/**
 * Call into the scheduler to yield the rest of this thread's CPU time. We'll get put back at the
 * end of the runnable queue.
 */
void Thread::yield() {
    Scheduler::get()->yield();
}

/**
 * Terminates the calling thread.
 */
void Thread::die() {
    auto thread = current();
    REQUIRE(thread, "cannot terminate null thread!");

    thread->terminate();

    // we should not get here
    panic("failed to terminmate thread");
}

/**
 * Terminates the thread.
 *
 * If not active, we'll set it as a zombie and deal with it accordingly. Otherwise, we'll set some
 * flags, then context switch and the deferred work will occur then.
 */
void Thread::terminate(bool release) {
    DECLARE_CRITICAL();

    // update state
    CRITICAL_ENTER();

    bool isRunning;
    __atomic_load(&this->isActive, &isRunning, __ATOMIC_ACQUIRE);

    if(!isRunning) {
        this->setState(State::Zombie);
    } else {
        bool yes = true;
        __atomic_store(&this->needsToDie, &yes, __ATOMIC_RELEASE);
    }

    CRITICAL_EXIT();

    // TODO: ensure it's removed from run queues
    // Scheduler::get()->removeThreadFromRunQueue(this);

    // if running, context switch away
    if(isRunning) {
        yield();
    } 
    // delete it right away since the thread isn't running
    else {
        if(release) Scheduler::get()->idle->queueDestroyThread(this->sharedFromThis());
    }
}

/**
 * Performs a deferred thread termination.
 */
void Thread::deferredTerminate() {
    this->setState(State::Zombie);

    Scheduler::get()->idle->queueDestroyThread(this->sharedFromThis());
}



/**
 * Sleeps the calling thread for the given number of nanoseconds.
 *
 * @note The actual sleep time may be less or more than what is provided; it's merely taken as a
 * "best effort" hint to the actual sleep time.
 */
void Thread::sleep(const uint64_t nanos) {
    auto thread = current();
    int err;

    // create the timer blockable
    auto block = rt::MakeShared<TimerBlocker>(nanos);

    // wait on it
    err = thread->blockOn(block);

    if(err) {
        log("sleep failed: %d state %d", err, (int) thread->state);
    }
}

/**
 * Blocks the thread on the given object.
 *
 * @return 0 if the block completed, or an error code if the block was interrupted (because the
 * thread was woken for another reason, for example.)
 *
 * Note that we raise the IRQL, but do not ever lower it; this is because when we get switched back
 * in, the IRQL will be lowered again to the Passive level, i.e. what most threads that are running
 * will be using.
 *
 * TODO: Handle the timeouts
 */
int Thread::blockOn(const rt::SharedPtr<Blockable> &b, const uint64_t until) {
    DECLARE_CRITICAL();

    REQUIRE(b, "invalid blockable %p", static_cast<void *>(b));
    REQUIRE_IRQL_LEQ(platform::Irql::Scheduler);

    // raise IRQL to scheduler level (to prevent being preempted)
    // XXX: change this?
    platform_raise_irql(platform::Irql::Scheduler);
    CRITICAL_ENTER();

    // update thread state
    RW_LOCK_WRITE(&this->lock);

    REQUIRE(this->state == State::Runnable, "Cannot %s thread %p with state: %d (blockable %p)",
            "block", this, (int) this->state, static_cast<void *>(b));
    REQUIRE(!this->blockingOn, "cannot block thread %p (object %p) while already blocking (%p)",
            this, static_cast<void *>(b), static_cast<void *>(this->blockingOn));

    this->blockingOn = b;
    this->setState(State::Blocked);

    // prepare the blockables
    b->willBlockOn(this->sharedFromThis());

    RW_UNLOCK_WRITE(&this->lock);
    CRITICAL_EXIT();

    // yield the rest of the CPU time
    Scheduler::get()->yield();

    // get state from the blockable
    bool signaled = b->isSignalled();
    b->reset();

    // return whether the thread woke correctly or nah
    return (signaled ? 0 : -1);
}

/**
 * Unblocks the thread.
 */
void Thread::unblock(const rt::SharedPtr<Blockable> &b) {
    REQUIRE_IRQL_LEQ(platform::Irql::Scheduler);
    REQUIRE(this->state == State::Blocked, "Cannot %s thread %p with state: %d (blockable %p)",
            "unblock", this, (int) this->state, static_cast<void *>(b));

    DECLARE_CRITICAL();
    CRITICAL_ENTER();

    // clear the blockable list
    RW_LOCK_WRITE(&this->lock);
    REQUIRE(b == this->blockingOn, "thread not blocking on %p! (is %p)", static_cast<void *>(b), 
            static_cast<void *>(this->blockingOn));

    // finish setting state
    this->blockingOn->didUnblock();
    this->blockingOn = nullptr;

    this->setState(State::Runnable);

    RW_UNLOCK_WRITE(&this->lock);
    CRITICAL_EXIT();

    // enqueue in scheduler
    Scheduler::get()->markThreadAsRunnable(this->sharedFromThis(), true);
}



/**
 * Inserts a new deferred procedure call (DPC) into this thread's call. The next time the thread is
 * context switched in, it will execute all DPCs queued on it.
 *
 * To accomplish this, when we add a DPC to a thread that does not have any DPCs queued, we add a
 * "fake" context switch stack frame to the top of the thread's stack, which will return to the
 * DPC handler. This will then invoke all DPCs, and then perform another context switch with the
 * actual saved state of the thread.
 */
int Thread::addDpc(void (*handler)(Thread *, void *), void *context) {
    int err = 0;

    DECLARE_CRITICAL();
    bool needsDpcReturnFrame = false;

    // build info struct
    DpcInfo info;
    info.handler = handler;
    info.context = context;

    // insert it
    CRITICAL_ENTER();
    RW_LOCK_WRITE(&this->lock);

    this->dpcs.push_back(info);
    needsDpcReturnFrame = (this->dpcs.size() == 1);

    RW_UNLOCK_WRITE(&this->lock);

    // push the fake return frame if needed
    if(!this->isActive && needsDpcReturnFrame) {
        err = arch::PushDpcHandlerFrame(this);
    }

    // set the "have DPCs" flag
    bool yee = true;
    __atomic_store(&this->dpcsPending, &yee, __ATOMIC_RELEASE);

    CRITICAL_EXIT();

    // assume success
    return err;
}

/**
 * Runs all pending DPCs.
 */
void Thread::runDpcs() {
    DECLARE_CRITICAL();

    // set up queue access
    CRITICAL_ENTER();
    RW_LOCK_WRITE(&this->lock);

    // queue loop
    while(!this->dpcs.empty()) {
        auto dpc = this->dpcs.pop();
        dpc.handler(this, dpc.context);
    }

    // clear the "have DPCs" flag
    bool no = false;
    __atomic_store(&this->dpcsPending, &no, __ATOMIC_RELEASE);

    // release queue access
    RW_UNLOCK_WRITE(&this->lock);
    CRITICAL_EXIT();
}


/**
 * Blocks the caller waiting for this thread to terminate, or until the timeout elapses.
 *
 * @param waitUntil Absolute timepoint until which to wait, 0 to poll, or the maximum value of
 * the type to wait forever.
 *
 * @return 0 if the thread terminated, 1 if the wait timed out, or a negativ error code.
 */
int Thread::waitOn(const uint64_t waitUntil) {
    int err;

    DECLARE_CRITICAL();

    // create the notification flag
    auto flag = rt::MakeShared<SignalFlag>();
    if(!flag) {
        return -1;
    }

    // add it to the termination list
    CRITICAL_ENTER();
    RW_LOCK_WRITE(&this->lock);

    this->terminateSignals.push_back(flag);

    RW_UNLOCK_WRITE(&this->lock);
    CRITICAL_EXIT();

    // now, block the caller on this object
    err = sched::Thread::current()->blockOn(flag, waitUntil);

    if(!err) { // block completed
        return 0;
    } else { // some sort of error
        return err;
    }
}


/**
 * Handles faults.
 *
 * If the fault can be handeled, we rewrite the exception frame to return to a different address in
 * the userspace thread; this would be an assembler runtime stub that records processor state and
 * invokes its own handlers.
 *
 * This will only be invoked for faults from userspace. All kernel faults will immediately cause
 * a panic.
 */
void Thread::handleFault(const FaultType type, const uintptr_t pc, void *context, const void *arch) {
    // special handling for the general fault type
    if(type == FaultType::General) {
        goto unhandled;
    }

    // other faults may be caught
    switch(type) {
        // invoke the userspace handler for invalid instruction, if registered
        case FaultType::InvalidInstruction:
            goto unhandled;
            break;

        default:
            break;
    }

    // if we get here, the fault wasn't handled

unhandled:;
    // fault was not handled; kill the thread
    REQUIRE(this->task, "no task for thread %p with fault %d", this, (int) type);

    // print the register info
    constexpr static const size_t kBufSize = 1024;
    char buf[kBufSize];
    arch::PrintState(arch, buf, kBufSize);

    log("Unhandled fault %d in thread $%p'h (%s) info %p pc %p\n%s", (int) type, this->handle, 
            this->name, context, pc, buf);
    this->task->terminate(-1);
}



/**
 * Sends a notification to the thread.
 *
 * We'll OR the provided bit mask against the existing notification mask. If the result of ANDing
 * this and the notification mask is nonzero, the thread is unblocked (if it is blocked.)
 */
void Thread::notify(const uintptr_t bits) {
    uintptr_t mask;

    bool no = false, yes = true;

    // set the bits
    const auto set = __atomic_or_fetch(&this->notifications, bits, __ATOMIC_RELEASE);
    __atomic_load(&this->notificationMask, &mask, __ATOMIC_RELAXED);

    if(set & mask) {
        // unblock me boi
        if(this->notificationsFlag &&
                __atomic_compare_exchange(&this->notified, &no, &yes, false, __ATOMIC_RELEASE,
                    __ATOMIC_RELAXED)) {
            this->notificationsFlag->signal();
        }
    }
}

/**
 * Blocks the thread waiting for notifications to arrive.
 *
 * @param mask If nonzero, a new value to set for the thread's notification mask.
 * @return The bitwise AND of the notification bits and the notification mask.
 */
uintptr_t Thread::blockNotify(const uintptr_t _mask) {
    uintptr_t mask;

    DECLARE_CRITICAL();
    CRITICAL_ENTER();

    // update mask
    if(_mask) {
        mask = _mask;
        __atomic_store(&this->notificationMask, &mask, __ATOMIC_RELAXED);
    } else {
        __atomic_load(&this->notificationMask, &mask, __ATOMIC_RELAXED);
    }

    // clear all bits covered by the mask, return old value
    const auto oldBits = __atomic_fetch_and(&this->notifications, ~mask, __ATOMIC_RELAXED);
    // if any bits coincided with the mask were set, return them
    if(oldBits & mask) {
        CRITICAL_EXIT();
        return (oldBits & mask);
    }

    // prepare for blocking
    auto flag = rt::MakeShared<SignalFlag>();
    this->notificationsFlag = flag;

    CRITICAL_EXIT();

    // block on it
    this->blockOn(this->notificationsFlag);

    // woke up from blocking. return the set bits
    this->notificationsFlag = nullptr;
    __atomic_clear(&this->notified, __ATOMIC_RELEASE);

    const auto newBits = __atomic_fetch_and(&this->notifications, ~mask, __ATOMIC_RELAXED);
    return (newBits & mask);
} 
