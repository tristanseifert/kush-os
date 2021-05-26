#include <arch.h>

using namespace arch;

/**
 * Flush TLB entry for the given address.
 */
void arch::InvalidateTlb(const uintptr_t addr) {
    asm volatile("invlpg (%0)" : : "b"(addr) : "memory");
}
