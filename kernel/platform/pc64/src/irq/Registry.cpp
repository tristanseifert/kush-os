#include <platform.h>

#include <log.h>

/**
 * Registers a new interrupt handler for the given irq number. When the interrupt is triggered,
 * the specified callback is invoked with the specified context, and an opaque irq token that can
 * be used to later acknowledge the interrupt.
 *
 * Multiple handlers for the same interrupt are allowed; they are called in an implementation-
 * defined order.
 */
int platform_irq_register(const uintptr_t irq, bool(*callback)(void *, const uintptr_t), void *ctx) {
    log("%s unimplemented", __PRETTY_FUNCTION__);
    return 0;
}

/**
 * Removes a previously installed interrupt handler.
 */
void platform_irq_unregister(const uintptr_t token) {
    log("%s unimplemented", __PRETTY_FUNCTION__);
}

/**
 * Acknowledge an interrupt.
 *
 * We wait to receive however many irq acks for an interrupt as there are registered handlers
 * before we acknowledge the interrupt in hardware.
 *
 * @param token Opaque "irq token" value passed to an irq callback
 */
int platform_irq_ack(const uintptr_t token) {
    log("%s unimplemented", __PRETTY_FUNCTION__);

    // XXX: wtf does this return mean
    return 0;
}

