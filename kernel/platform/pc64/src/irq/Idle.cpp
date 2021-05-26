#include <platform.h>
#include <log.h>


/**
 * Puts the processor in a low-power state because there's nothing to do.
 *
 * This will sleep the processor until the next interrupt. It assumes interrupts are enabled
 * when called.
 */
void platform::Idle() {
    // ensure IRQs are on
    uintptr_t flags;
    asm volatile("pushfq\n\t"
            "popq %0" : "=r"(flags));

    REQUIRE(flags & 0x200, "Calling %s with irqs masked is prohibited", __PRETTY_FUNCTION__);

    // wait for next interrupt
    asm volatile("hlt" ::: "memory");
}

