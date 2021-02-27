/**
 * Thread-local implementation for statically linked executables
 */
#include "tls.h"
#include <threads.h>

#include <stdlib.h>
#include <stdio.h>

/**
 * Invoked when a thread is launched to set up its thread-local storage array.
 */
LIBC_INTERNAL void __libc_tls_init() {
    fprintf(stderr, "%s unimplemented!\n", __FUNCTION__);
}

/**
 * Tears down a thread's TLS structure.
 */
LIBC_INTERNAL void __libc_tls_fini() {
    fprintf(stderr, "%s unimplemented!\n", __FUNCTION__);
}
