/*
 * Function definitions for all non-trivial syscalls
 */
#ifndef KERNEL_SYS_HANDLERS_H
#define KERNEL_SYS_HANDLERS_H

#include "Syscall.h"
#include "Errors.h"

namespace sys {

/// Allocates a virtual memory region backed by physical memory
int VmAlloc(const Syscall::Args *args, const uintptr_t number);
/// Allocate a virtual memory region backed by anonymous memory
int VmAllocAnon(const Syscall::Args *args, const uintptr_t number);
/// Update permissions (R/W/X flags) of a VM region
int VmRegionUpdatePermissions(const Syscall::Args *args, const uintptr_t number);
/// Resizes a VM region
int VmRegionResize(const Syscall::Args *args, const uintptr_t number);
/// Maps a VM region into a task.
int VmRegionMap(const Syscall::Args *args, const uintptr_t number);
/// Unmaps a VM region.
int VmRegionUnmap(const Syscall::Args *args, const uintptr_t number);
/// Gets information about a VM region
int VmRegionGetInfo(const Syscall::Args *args, const uintptr_t number);


/**
 * Creates a new userspace thread.
 */
int ThreadCreate(const Syscall::Args *args, const uintptr_t number);
/**
 * Destroys an userspace thread.
 */
int ThreadDestroy(const Syscall::Args *args, const uintptr_t number);

/**
 * Sets the thread priority.
 */
int ThreadSetPriority(const Syscall::Args *args, const uintptr_t number);

/**
 * Sets the thread's notification mask.
 */
int ThreadSetNoteMask(const Syscall::Args *args, const uintptr_t number);

/**
 * Sets the descriptive name of the thread.
 *
 * The first argument is a thread handle (or zero for current thread,) followed by the userspace
 * address of the name and its length, in bytes.
 */
int ThreadSetName(const Syscall::Args *args, const uintptr_t number);


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

int TaskDbgOut(const Syscall::Args *args, const uintptr_t number);

}

#endif
