#include <platform.h>
#include <log.h>


/**
 * Puts the processor in a low-power state because there's nothing to do.
 *
 * This will sleep the processor until the next interrupt. It assumes interrupts are enabled
 * when called.
 */
void platform::Idle() {
    asm volatile("hlt" ::: "memory");
}

