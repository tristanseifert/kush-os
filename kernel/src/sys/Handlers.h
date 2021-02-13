/*
 * Function definitions for all non-trivial syscalls
 */
#ifndef KERNEL_SYS_HANDLERS_H
#define KERNEL_SYS_HANDLERS_H

#include "Syscall.h"
#include "Errors.h"

namespace sys {

/**
 * Implements the create task syscall.
 */
int TaskCreate(const Syscall::Args *args, const uintptr_t number);

/**
 * Terminates a task, setting its exit code.
 *
 * The first argument is the task handle; if zero, the current task is terminated. The second
 * argument specifies the return code.
 */
int TaskTerminate(const Syscall::Args *args, const uintptr_t number);

/**
 * Implements the "initialize task" syscall.
 *
 * This will invoke all kernel handlers that are interested in new tasks being created, finish
 * setting up some kernel structures, then perform a return to userspace to the specified address
 * and stack.
 */
int TaskInitialize(const Syscall::Args *args, const uintptr_t number);

/**
 * Sets the descriptive name of the task.
 *
 * The first argument is a task handle (or zero for current task,) followed by the userspace
 * address of the name and its length, in bytes.
 */
int TaskSetName(const Syscall::Args *args, const uintptr_t number);

}

#endif
