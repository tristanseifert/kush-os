#include "threads/thread_info.h"
#include "LaunchInfo.h"

extern void __stdstream_init();
extern void __libc_tss_init();

/// memory address of the task's info page
kush_task_launchinfo_t *__libc_task_info = NULL;

/**
 * General C library initialization
 */
void __libc_init() {
    __libc_thread_init();
    __libc_tss_init();

    // set up input/output streams
    __stdstream_init();
}

