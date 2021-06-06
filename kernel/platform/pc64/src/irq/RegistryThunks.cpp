#include "CoreLocalRegistry.h"

#include <arch.h>
#include <arch/PerCpuInfo.h>
#include <platform.h>
#include <log.h>

/**
 * Registers a new interrupt handler for the given irq number. When the interrupt is triggered,
 * the specified callback is invoked with the specified context, and an opaque irq token that can
 * be used to later acknowledge the interrupt.
 *
 * Multiple handlers for the same interrupt are allowed; they are called in an implementation-
 * defined order.
 *
 * The irq number is simply the physical irq vector number minus 48; vectors 0 through 31 are
 * reserved for CPU exceptions, and vectors 32 through 47 are reserved for low priority IPIs.
 *
 * @return IRQ token, or 0 if we failed to register the interrupt
 */
uintptr_t platform::IrqRegister(const uintptr_t irq, bool(*callback)(void *, const uintptr_t),
        void *ctx) {
    // calculate real vector number
    const uintptr_t vector = irq + 48;
    if(vector > 0xFF) {
        return -1;
    }

    // set up irq registry for this core if needed
    auto &p = arch::GetProcLocal()->p;
    {
        const auto oldIrql = platform_raise_irql(platform::Irql::Scheduler);

        if(!p.irqRegistrar) {
            p.irqRegistrar = new CoreLocalIrqRegistry;
            REQUIRE(p.irqRegistrar, "failed to allocate irq registrar");
        }

        platform_lower_irql(oldIrql);
    }

    // invoke its register method with the real vector number
    return p.irqRegistrar->add(irq, callback, ctx);
}

/**
 * Removes a previously installed interrupt handler.
 *
 * @return 0 on success, error code otherwise.
 */
int platform::IrqUnregister(const uintptr_t token) {
    // we _should_ have an IRQ registrar by now...
    auto &p = arch::GetProcLocal()->p;
    REQUIRE(p.irqRegistrar, "no irq registrar in %s", __PRETTY_FUNCTION__);

    // so call through to its remove routine
    return p.irqRegistrar->remove(token);
}

/**
 * Acknowledge an interrupt.
 *
 * We wait to receive however many irq acks for an interrupt as there are registered handlers
 * before we acknowledge the interrupt in hardware.
 *
 * @param token Opaque "irq token" value passed to an irq callback
 */
int platform::IrqAck(const uintptr_t token) {
    log("%s unimplemented", __PRETTY_FUNCTION__);

    // XXX: wtf does this return mean
    return 0;
}

