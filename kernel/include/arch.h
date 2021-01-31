#ifndef KERNEL_ARCH_H
#define KERNEL_ARCH_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initializes the architecture. Called early during boot.
 */
void arch_init();

/**
 * Returns the size of a page.
 */
size_t arch_page_size();

#ifdef __cplusplus
}
#endif
#endif
