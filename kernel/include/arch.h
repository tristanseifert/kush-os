#ifndef KERNEL_ARCH_H
#define KERNEL_ARCH_H

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes the architecture. Called early during boot.
 */
void arch_init();

/**
 * Notifies the architecture code that paging and virtual memory has become available.
 */
void arch_vm_available();

/**
 * Returns the size of a page.
 */
size_t arch_page_size();

/**
 * Whether the processor supports marking regions as no-execute.
 */
bool arch_supports_nx();

#ifdef __cplusplus
}
#endif
#endif
