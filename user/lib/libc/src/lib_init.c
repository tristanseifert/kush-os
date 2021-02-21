#include "threads/thread_info.h"

extern void __stdstream_init();
extern void __libc_tss_init();

/**
 * General C library initialization
 */
void __libc_init() {
    __libc_thread_init();
    __libc_tss_init();

    // set up input/output streams
    __stdstream_init();
}

