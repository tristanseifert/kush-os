#include "Handlers.h"

#include "sched/Task.h"
#include "sched/Thread.h"
#include "ipc/Interrupts.h"
#include "handle/Manager.h"

#include <arch.h>
#include <arch/critical.h>
#include <platform.h>
#include <log.h>

using namespace sys;

/// Possible values for the irq handler get info call
enum InfoKey: uintptr_t {
    /// Return the interrupt number
    InterruptNumber                     = 0x01,
    /// Return the vector number of the interrupt handler.
    VectorNumber                        = 0x02,
};



/**
 * Validate whether the calling task may access the provided interrupt handler object.
 */
static bool CallerHasRightsTo(const rt::SharedPtr<ipc::IrqHandler> &irq) {
    // verify caller is in the same task
    auto handlerThread = irq->getThread();
    if(!handlerThread) return false;

    auto callerTask = sched::Task::current();
    if(callerTask != handlerThread->task) return false;

    return true;
}



/**
 * Sets up an interrupt handler that notifies a thread when fired.
 *
 * @param irqNum Platform specific IRQ number to register a handler for
 * @param threadHandle Thread to notify when interrupt triggers (0 for current thread)
 * @param bits Notification bits to send when interrupt is triggered
 *
 * @return Negative error code, or a valid handle for the IRQ handler object
 */
intptr_t sys::IrqHandlerInstall(const uintptr_t irqNum, const Handle threadHandle,
        const uintptr_t bits) {
    rt::SharedPtr<sched::Thread> thread;

    // validate some arguments
    if(!bits) {
        return Errors::InvalidArgument;
    }

    // resolve thread handle
    if(!threadHandle) {
        thread = sched::Thread::current();
    } else {
        thread = handle::Manager::getThread(threadHandle);
    }

    if(!thread) {
        return Errors::InvalidHandle;
    } 

    // register the IRQ handler
    RW_LOCK_WRITE_GUARD(thread->lock);

    auto handler = ipc::Interrupts::create(irqNum, thread, bits);
    if(!handler) {
        return Errors::NoMemory;
    }

    thread->irqHandlers.append(handler);
    return static_cast<intptr_t>(handler->getHandle());
}

/**
 * Removes an existing interrupt handler.
 *
 * The calling thread must be in the same task as the thread to which the interrupt is delivering
 * notifications to.
 *
 * @param irqHandle Handle of the interrupt handler object
 *
 * @return 0 on success, or a negative reror code
 */
intptr_t sys::IrqHandlerRemove(const Handle irqHandle) {
    // get the interrupt handler
    auto irq = handle::Manager::getIrq(irqHandle);
    if(!irq) {
        return Errors::InvalidHandle;
    }

    // verify caller is in the same task
    if(!CallerHasRightsTo(irq)) {
        return Errors::PermissionDenied;
    }
    auto handlerThread = irq->getThread();

    // remove the handler and delete it
    RW_LOCK_WRITE_GUARD(handlerThread->lock);
    handlerThread->irqHandlers.removeMatching([](void *ctx, rt::SharedPtr<ipc::IrqHandler> &handler) {
        auto key = reinterpret_cast<ipc::IrqHandler *>(ctx);

        if(key == handler.get()) {
            return true;
        }
        return false;
    }, irq.get());

    return Errors::Success;
}

/**
 * Updates the thread and the notification bits sent to the thread, when the interrupt handler
 * fires.
 *
 * @note This call _replaces_ notification bits; so a value of 0 would result in a nonfunctional
 * handler, so it is prohibited.
 *
 * @param irqHandle Handle of the interrupt handler object. The calling task must own it.
 * @param threadHandle Thread to notify when the interrupt fires
 * @param bits Notification bits to assign to that thread
 *
 * @return 0 on success, or a negative error code.
 */
intptr_t sys::IrqHandlerUpdate(const Handle irqHandle, const Handle threadHandle,
        const uintptr_t bits) {
    // validate arguments
    if(!bits) return Errors::InvalidArgument;

    // get the interrupt handler and destination thread
    auto irq = handle::Manager::getIrq(irqHandle);
    if(!irq) {
        return Errors::InvalidHandle;
    }

    auto newHandlerThread = handle::Manager::getThread(threadHandle);
    if(!newHandlerThread) {
        return Errors::InvalidHandle;
    }

    // verify caller is in the same task; and the destination thread is also in this task
    if(!CallerHasRightsTo(irq)) {
        return Errors::PermissionDenied;
    }

    auto oldHandlerThread = irq->getThread();
    if(newHandlerThread->task != oldHandlerThread->task) {
        return Errors::InvalidArgument;
    }

    // perform the exchange and add to new thread's list
    irq->setTarget(newHandlerThread, bits);

    RW_LOCK_WRITE(&newHandlerThread->lock);
    newHandlerThread->irqHandlers.append(irq);
    RW_UNLOCK_WRITE(&newHandlerThread->lock);

    // remove it from the old thread's IRQ handler list
    RW_LOCK_WRITE(&oldHandlerThread->lock);
    oldHandlerThread->irqHandlers.removeMatching([](void *ctx, rt::SharedPtr<ipc::IrqHandler> &handler) {
        auto key = reinterpret_cast<ipc::IrqHandler *>(ctx);

        if(key == handler.get()) {
            return true;
        }
        return false;
    }, irq.get());
    RW_UNLOCK_WRITE(&oldHandlerThread->lock);

    return Errors::Success;
}

/**
 * Gets info for an irq handler.
 *
 * @param irqHandle Handle of the interrupt handler object. The calling task must own it.
 * @param what Information key to retrieve; see the `InfoKey` enum.
 *
 * @return Information value, or a negative error code.
 */
intptr_t sys::IrqHandlerGetInfo(const Handle irqHandle, const uintptr_t what) {
    // get the interrupt handler object
    auto irq = handle::Manager::getIrq(irqHandle);
    if(!irq) {
        return Errors::InvalidHandle;
    }

    if(!CallerHasRightsTo(irq)) {
        return Errors::PermissionDenied;
    }

    // get the info value
    switch(what) {
        case InfoKey::InterruptNumber:
            return static_cast<intptr_t>(irq->getIrqNum());
        case InfoKey::VectorNumber:
            return static_cast<intptr_t>(irq->getVecNum());

        // unknown key
        default:
            return Errors::InvalidArgument;
    }
}

/**
 * Allocates an interrupt handler that's bound to the next available vector number on the current
 * processor. The calling thread will be locked to that core.
 *
 * This can be used to implement things like driver IPIs or message signaled interrupts.
 *
 * @param threadHandle Thread to notify when the interrupt fires
 * @param bits Notification bits to set on the thread when the interrupt fires
 *
 * @return Negative error code, or a valid handle for the IRQ handler object
 */
intptr_t sys::IrqHandlerAllocCoreLocal(const Handle threadHandle, const uintptr_t bits) {
    rt::SharedPtr<sched::Thread> thread;
    uintptr_t vector{0};

    // validate some arguments
    if(!bits) {
        return Errors::InvalidArgument;
    }

    // resolve thread handle
    if(!threadHandle) {
        thread = sched::Thread::current();
    } else {
        thread = handle::Manager::getThread(threadHandle);
    }

    if(!thread) {
        return Errors::InvalidHandle;
    }

    // allocate a vector number
    const auto irqNum = platform::IrqAllocCoreLocal(vector);
    if(!irqNum) {
        // XXX: is there a better way to signal specifically we're out of IRQ resources?
        return Errors::GeneralError;
    }

    // allocate irq handler
    RW_LOCK_WRITE_GUARD(thread->lock);

    auto handler = ipc::Interrupts::create(irqNum, thread, bits);
    if(!handler) {
        return Errors::NoMemory;
    }

    // TODO: lock thread to core
    handler->irqVector = vector;

    // finish up
    thread->irqHandlers.append(handler);
    return static_cast<intptr_t>(handler->getHandle());
}

