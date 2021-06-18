#include "Interrupts.h"

#include "log.h"
#include "handle/Manager.h"
#include "sched/Thread.h"

#include <platform.h>
#include <arch/critical.h>

using namespace ipc;

/**
 * Creates a new IRQ handler object. This allocates a handle to represent it.
 */
IrqHandler::IrqHandler(const rt::SharedPtr<sched::Thread> &_thread, const uintptr_t _bits) : 
    thread(_thread), bits(_bits) {
}

/**
 * Removes the platform irq handler.
 */
IrqHandler::~IrqHandler() {
    if(this->platformToken) {
        platform::IrqUnregister(this->platformToken);
    }
}

/**
 * Notifies the thread that the interrupt has fired.
 */
void IrqHandler::fired() {
    REQUIRE(this->thread, "cannot deliver irq to nonexistent thread");
    this->thread->notify(this->bits);
}



/**
 * Creates a new IRQ handler for the given thread and notification bits.
 */
rt::SharedPtr<IrqHandler> Interrupts::create(const uintptr_t irq,
        const rt::SharedPtr<sched::Thread> &thread, const uintptr_t bits) {
    // validate args
    if(!thread || !bits) {
        return nullptr;
    }

    // create the object and assign a handle
    auto info = rt::SharedPtr<IrqHandler>(new IrqHandler(thread, bits));
    info->irqNum = irq;

    info->handle = handle::Manager::makeIrqHandle(info);
    REQUIRE(static_cast<uintptr_t>(info->handle), "failed to make handle for irq handler");

    // register interrupt
    info->platformToken = platform::IrqRegister(irq, [](void *ctx, const uintptr_t) -> bool {
        auto info = reinterpret_cast<IrqHandler *>(ctx);
        info->fired();

        /// XXX: handle more than one handler
        return true;
    }, info.get());

    if(!info->platformToken) {
        // failed to install the handler
        return nullptr;
    }

    return info;
}

/**
 * Updates the interrupt handler's target.
 */
void IrqHandler::setTarget(const rt::SharedPtr<sched::Thread> &newThread,
        const uintptr_t bits) {
    REQUIRE(newThread, "invalid thread ptr");
    REQUIRE(bits, "invalid notification bits");

    DECLARE_CRITICAL();

    // copy the thread ptr so we don't deallocate in critical section
    auto oldThread = this->thread;

    // replace notification bits and interrupts (TODO: need to mask irq?)
    CRITICAL_ENTER();
    this->thread = newThread;
    __atomic_store_n(&this->bits, bits, __ATOMIC_RELAXED);
    CRITICAL_EXIT();
}
