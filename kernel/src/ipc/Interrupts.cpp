#include "Interrupts.h"

#include "log.h"
#include "handle/Manager.h"
#include "sched/Thread.h"

#include <platform.h>


using namespace ipc;

/**
 * Creates a new IRQ handler object. This allocates a handle to represent it.
 */
IrqHandler::IrqHandler(sched::Thread *_thread, const uintptr_t _bits) : thread(_thread),
    bits(_bits) {
    this->handle = handle::Manager::makeIrqHandle(this);
    REQUIRE(static_cast<uintptr_t>(this->handle), "failed to make handle for irq handler");
}

/**
 * Removes the platform irq handler.
 */
IrqHandler::~IrqHandler() {
    if(this->platformToken) {
        platform_irq_unregister(this->platformToken);
    }
}

/**
 * Notifies the thread that the interrupt has fired.
 */
void IrqHandler::fired() {
    this->thread->notify(this->bits);
}



/**
 * Creates a new IRQ handler for the given thread and notification bits.
 */
IrqHandler *Interrupts::create(const uintptr_t irq, sched::Thread *thread, const uintptr_t bits) {
    // validate args
    if(!thread || !bits) {
        return nullptr;
    }

    // create the info
    auto info = new IrqHandler(thread, bits);
    info->irqNum = irq;

    // register interrupt
    info->platformToken = platform_irq_register(irq, [](void *ctx, const uintptr_t) -> bool {
        auto info = reinterpret_cast<IrqHandler *>(ctx);
        info->fired();

        /// XXX: handle more than one handler
        return true;
    }, info);

    if(!info->platformToken) {
        // failed to install the handler
        delete info;
        return nullptr;
    }

    return info;
}

