/**
 * Thread-local implementation for dynamically linked executables
 */
#include "tls.h"
#include "tss_private.h"
#include <errno.h>
#include <threads.h>
#include <stdlib.h>
#include <stdio.h>

extern void *__dyldo_setup_tls();
extern void __dyldo_teardown_tls();


/**
 * Invoked to initialize TLS for the main thread.
 */
LIBC_INTERNAL void __libc_tls_main_init() {
    __libc_tss_thread_init();

    errno = 0;
}

/**
 * Invoked when a thread is launched to set up its thread-local storage array.
 */
LIBC_INTERNAL void __libc_tls_init() {
    __dyldo_setup_tls();
    __libc_tss_thread_init();

    errno = 0;
}

/**
 * Tears down a thread's TLS structure.
 */
LIBC_INTERNAL void __libc_tls_fini() {
    __libc_tss_thread_fini();
    __dyldo_teardown_tls();
}

/*
 * Define weakly the dyldo functions; they're replaced at load time by the linker, but since no
 * object actually exports them, we'll get problems at compile time otherwise.
 */
__attribute__((weak)) void *__dyldo_setup_tls() {
    fprintf(stderr, "%s unimplemented!\n", __FUNCTION__);
    abort();
}

__attribute__((weak)) void __dyldo_teardown_tls() {
    fprintf(stderr, "%s unimplemented!\n", __FUNCTION__);
    abort();
}
__attribute__((weak)) void *___tls_get_addr(void *ctx) {
    fprintf(stderr, "%s unimplemented!\n", __FUNCTION__);
    abort();
}

