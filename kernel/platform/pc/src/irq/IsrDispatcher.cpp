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

        // legacy ISA interrupts
        case ISR_ISA_0:
        case ISR_ISA_1:
        case ISR_ISA_2:
        case ISR_ISA_3:
        case ISR_ISA_4:
        case ISR_ISA_5:
        case ISR_ISA_6:
        case ISR_ISA_7:
        case ISR_ISA_8:
        case ISR_ISA_9:
        case ISR_ISA_10:
        case ISR_ISA_11:
        case ISR_ISA_12:
        case ISR_ISA_13:
        case ISR_ISA_14:
        case ISR_ISA_15:
            Manager::get()->handleIsr(type);
            break;

        // unhandled (other)
        default:
            panic("unhandled ISR type $%08lx", type);
    }
}
