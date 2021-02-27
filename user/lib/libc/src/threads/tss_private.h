#ifndef LIBC_THREAD_TSS_PRIVATE_H
#define LIBC_THREAD_TSS_PRIVATE_H

#include <_libc.h>
#include <stdint.h>
#include <threads.h>

/**
 * Entry in a thread's TSS hashmap
 */
typedef struct {
    tss_t key;
    void *value;
} tss_entry_t;

/// Allocate a thread's TSS info.
LIBC_INTERNAL void __libc_tss_thread_init();
/// Releases the thread's TSS info.
LIBC_INTERNAL void __libc_tss_thread_fini();

#endif
