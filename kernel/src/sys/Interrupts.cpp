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

// TODO: all functions need to be tested again

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
    rt::SharedPtr<sched::Thread> thread = nullptr;

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
    auto handlerThread = irq->getThread();
    if(!handlerThread) {
        return Errors::GeneralError;
    }

    auto callerTask = sched::Task::current();
    if(callerTask != handlerThread->task) {
        return Errors::PermissionDenied;
    }

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

