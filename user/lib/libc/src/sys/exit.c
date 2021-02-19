#include <sys/syscalls.h>
#include <stdlib.h>

/**
 * Terminates the calling task, performing any clean-up work needed.
 */
void exit(int status) {
    // TODO: invoke exit handlers

    // now, terminate the process
    TaskExit(0, status);

    // we should never get here
    while(1) {}
}

/**
 * Terminates the calling task, without invoking any cleanup.
 */
void _Exit(int status) {
    TaskExit(0, status);

    // we should never get here
    while(1) {}
}
