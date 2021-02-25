/**
 * Format of the structure the program loader places into memory that defines some information
 * about where we're loaded, arguments, and so forth.
 */
#ifndef ROOTSRV_TASK_LAUNCHINFO_H
#define ROOTSRV_TASK_LAUNCHINFO_H

#include <stdint.h>

/**
 * Info structure that's passed to all tasks, a pointer to which is always the first value on the
 * stack of newly loaded processes.
 */
typedef struct {
    // magic value: should be 'TASK'
    uint32_t magic;

    /// path from which the program was loaded
    const char *loadPath;

    /// number of arguments
    uintptr_t numArgs;
    /// An array containing pointers to each of the arguments
    const char **args;
} kush_task_launchinfo_t;

/// Magic value for the task launch info
#define TASK_LAUNCHINFO_MAGIC           'TASK'

#endif
