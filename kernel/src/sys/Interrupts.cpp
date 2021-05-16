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

/**
 * Sets up an interrupt handler that notifies a thread when fired.
 *
 * - Arg0: Interrupt number
 * - Arg1: Thread handle (0 = current thread)
 * - Arg2: Notification bits to send
 */
intptr_t sys::IrqHandlerInstall(const Syscall::Args *args, const uintptr_t number) {
    rt::SharedPtr<sched::Thread> thread = nullptr;

    // resolve thread handle
    if(args->args[1]) {
        thread = handle::Manager::getThread(static_cast<Handle>(args->args[1]));
    } else {
        thread = sched::Thread::current();
    }

    // get notification bits
    const auto bits = args->args[2];
    if(!bits) {
        return Errors::InvalidArgument;
    }

    // get irq number
    const auto irqNum = args->args[0];

    // register the IRQ handler
    RW_LOCK_WRITE_GUARD(thread->lock);

    auto handler = ipc::Interrupts::create(irqNum, thread, bits);
    if(!handler) {
        return Errors::GeneralError;
    }

    thread->irqHandlers.append(handler);
    return static_cast<intptr_t>(handler->getHandle());
}

/**
 * Removes an existing interrupt handler.
 *
 * The calling thread must be in the same task as the thread to which the interrupt is delivering
 * notifications to.
 */
intptr_t sys::IrqHandlerRemove(const Syscall::Args *args, const uintptr_t number) {
    // get the interrupt handler
    auto irq = handle::Manager::getIrq(static_cast<Handle>(args->args[0]));
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

