#include "handlers.h"
#include "Manager.h"

#include <stdint.h>

#include <log.h>

using namespace platform::irq;

/**
 * High level ISR handler as invoked by all ISRs registered through our platform-specific interrupt
 * management system.
 *
 * This takes a single argument - a code defined in the handlers file. This corresponds to an
 * interrupt that we can route to the kernel.
 */
void platform_isr_handle(const uint32_t type) {
    switch(type) {
        // spurious PIC interrupt
        case ISR_SPURIOUS_PIC:
            log("spurious PIC interrupt");
            break;
        // spurious APIC interrupt
        case ISR_SPURIOUS_APIC:
            log("spurious APIC interrupt");
            break;

        // forward all other interrupts to the usual handler
        default:
            Manager::get()->handleIsr(type);
    }
}
