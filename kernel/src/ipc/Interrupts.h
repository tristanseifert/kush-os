#ifndef KERNEL_IPC_INTERRUPTS_H
#define KERNEL_IPC_INTERRUPTS_H

#include <stdint.h>

#include <handle/Manager.h>
#include <runtime/SmartPointers.h>

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
    friend class rt::SharedPtr<IrqHandler>;

    public:
        /// Return the handle for this irq handler.
        constexpr const Handle getHandle() const {
            return this->handle;
        }
        /// Return the thread to which this irq handler delivers notifications
        auto getThread() const {
            return this->thread;
        }
        /// Returns the vector number for this handler.
        constexpr auto getIrqNum() const {
            return this->irqNum;
        }

        /// Update the thread that is notified when the interrupt fires
        void setTarget(const rt::SharedPtr<sched::Thread> &thread, const uintptr_t bits);

        ~IrqHandler();

    private:
        IrqHandler(const rt::SharedPtr<sched::Thread> &_thread, const uintptr_t _bits);

        /// the IRQ has fired
        void fired();

        /// handle for the irq handler object
        Handle handle;

        /// platform irq handler token
        uintptr_t platformToken = 0;
        /// platform irq number
        uintptr_t irqNum = 0;

        /// thread to notify when the irq fires
        rt::SharedPtr<sched::Thread> thread;
        /// notification bits to yeet on the thread
        uintptr_t bits = 0;
};


/**
 * Provides a thin wrapper around platform interrupt handlers and threads.
 */
class Interrupts {
    public:
        static rt::SharedPtr<IrqHandler> create(const uintptr_t irqNum,
                const rt::SharedPtr<sched::Thread> &thread, const uintptr_t noteBits);
};
};

#endif
