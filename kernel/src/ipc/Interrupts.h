#ifndef KERNEL_IPC_INTERRUPTS_H
#define KERNEL_IPC_INTERRUPTS_H

#include <stdint.h>

#include <handle/Manager.h>

namespace sched {
struct Thread;
}

namespace ipc {
/**
 * Wraps up an interrupt handler.
 *
 * You can simply `delete` it when you want to uninstall the handler.
 */
class IrqHandler {
    friend class Interrupts;

    public:
        /// Return the handle for this irq handler.
        const Handle getHandle() const {
            return this->handle;
        }
        /// Return the thread to which this irq handler delivers notifications
        sched::Thread *getThread() const {
            return this->thread;
        }

        ~IrqHandler();

    private:
        IrqHandler(sched::Thread *_thread, const uintptr_t _bits);

        /// the IRQ has fired
        void fired();

        /// handle for the irq handler object
        Handle handle;

        /// platform irq handler token
        uintptr_t platformToken = 0;
        /// platform irq number
        uintptr_t irqNum = 0;

        /// thread to notify when the irq fires
        sched::Thread *thread = nullptr;
        /// notification bits to yeet on the thread
        uintptr_t bits = 0;
};


/**
 * Provides a thin wrapper around platform interrupt handlers and threads.
 */
class Interrupts {
    public:
        static IrqHandler *create(const uintptr_t irqNum, sched::Thread *thread,
                const uintptr_t noteBits);
};
};

#endif
