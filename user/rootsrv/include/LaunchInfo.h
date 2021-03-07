/**
 * Format of the structure the program loader places into memory that defines some information
 * about where we're loaded, arguments, and so forth.
 */
#ifndef ROOTSRV_TASK_LAUNCHINFO_H
#define ROOTSRV_TASK_LAUNCHINFO_H

// 32-bit platforms
#if defined(__i386__)
#define LAUNCHINFO_OFF_NARGS    (8)
#define LAUNCHINFO_OFF_ARGPTR   (12)
// 64-bit platforms
#elif defined(__x86_64__)
#define LAUNCHINFO_OFF_NARGS    (16)
#define LAUNCHINFO_OFF_ARGPTR   (24)
#else
#error unsupported architecture
#endif


#ifndef ASM_FILE
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

#ifdef __cplusplus
static_assert(offsetof(kush_task_launchinfo_t, numArgs) == LAUNCHINFO_OFF_NARGS);
static_assert(offsetof(kush_task_launchinfo_t, args) == LAUNCHINFO_OFF_ARGPTR);
#endif

/// Magic value for the task launch info
#define TASK_LAUNCHINFO_MAGIC           'TASK'
#endif

#endif
