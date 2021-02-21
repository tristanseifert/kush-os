#include <platform.h>

/**
 * Places the processor in a low power state.
 *
 * This will sleep until the next interrupt.
 */
void platform_idle() {
    asm volatile("hlt" ::: "memory");
}
