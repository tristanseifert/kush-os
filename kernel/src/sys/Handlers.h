/*
 * Function definitions for all non-trivial syscalls
 */
#ifndef KERNEL_SYS_HANDLERS_H
#define KERNEL_SYS_HANDLERS_H

#include "Syscall.h"
#include "Errors.h"

namespace sys {

/// Sends a message to the given port.
intptr_t PortSend(const Syscall::Args *args, const uintptr_t number);
/// Waits to receive a message on a port.
intptr_t PortReceive(const Syscall::Args *args, const uintptr_t number);
/// Updates a port's queue size
intptr_t PortSetParams(const Syscall::Args *args, const uintptr_t number);
/// Allocates a new port.
intptr_t PortAlloc(const Syscall::Args *args, const uintptr_t number);
/// Releases a previously allocated port.
intptr_t PortDealloc(const Syscall::Args *args, const uintptr_t number);

/// Sets the given notification bits in the thread.
intptr_t NotifySend(const Syscall::Args *args, const uintptr_t number);
/// Blocks waiting to receive a notification.
intptr_t NotifyReceive(const Syscall::Args *args, const uintptr_t number);

/// Allocates a virtual memory region backed by physical memory
intptr_t VmAlloc(const Syscall::Args *args, const uintptr_t number);
/// Allocate a virtual memory region backed by anonymous memory
intptr_t VmAllocAnon(const Syscall::Args *args, const uintptr_t number);
/// Update permissions (R/W/X flags) of a VM region
intptr_t VmRegionUpdatePermissions(const Syscall::Args *args, const uintptr_t number);
/// Resizes a VM region
intptr_t VmRegionResize(const Syscall::Args *args, const uintptr_t number);
/// Maps a VM region into a task.
intptr_t VmRegionMap(const Syscall::Args *args, const uintptr_t number);
/// Unmaps a VM region.
intptr_t VmRegionUnmap(const Syscall::Args *args, const uintptr_t number);
/// Gets information about a VM region
intptr_t VmRegionGetInfo(const Syscall::Args *args, const uintptr_t number);
/// Gets information on a task's virtual memory environment
intptr_t VmTaskGetInfo(const Syscall::Args *args, const uintptr_t number);
/// Returns the handle for the region containing an address.
intptr_t VmAddrToRegion(const Syscall::Args *args, const uintptr_t number);

/**
 * Creates a new userspace thread.
 */
intptr_t ThreadCreate(const Syscall::Args *args, const uintptr_t number);
/**
 * Destroys an userspace thread.
 */
intptr_t ThreadDestroy(const Syscall::Args *args, const uintptr_t number);

/**
 * Sets the thread priority.
 */
intptr_t ThreadSetPriority(const Syscall::Args *args, const uintptr_t number);

/**
 * Sets the thread's notification mask.
 */
intptr_t ThreadSetNoteMask(const Syscall::Args *args, const uintptr_t number);

/**
 * Sets the descriptive name of the thread.
 *
 * The first argument is a thread handle (or zero for current thread,) followed by the userspace
 * address of the name and its length, in bytes.
 */
intptr_t ThreadSetName(const Syscall::Args *args, const uintptr_t number);
/// Resumes the given paused thread.
intptr_t ThreadResume(const Syscall::Args *args, const uintptr_t number);
/// Waits for the specified thread to terminate.
intptr_t ThreadJoin(const Syscall::Args *args, const uintptr_t number);


/**
 * Implements the create task syscall.
 */
intptr_t TaskCreate(const Syscall::Args *args, const uintptr_t number);

/**
 * Terminates a task, setting its exit code.
 *
 * The first argument is the task handle; if zero, the current task is terminated. The second
 * argument specifies the return code.
 */
intptr_t TaskTerminate(const Syscall::Args *args, const uintptr_t number);

/**
 * Implements the "initialize task" syscall.
 *
 * This will invoke all kernel handlers that are interested in new tasks being created, finish
 * setting up some kernel structures, then perform a return to userspace to the specified address
 * and stack.
 */
intptr_t TaskInitialize(const Syscall::Args *args, const uintptr_t number);

/**
 * Sets the descriptive name of the task.
 *
 * The first argument is a task handle (or zero for current task,) followed by the userspace
 * address of the name and its length, in bytes.
 */
intptr_t TaskSetName(const Syscall::Args *args, const uintptr_t number);

intptr_t TaskDbgOut(const Syscall::Args *args, const uintptr_t number);

/// Installs an IRQ handler
intptr_t IrqHandlerInstall(const Syscall::Args *args, const uintptr_t number);
/// Removes an IRQ handler
intptr_t IrqHandlerRemove(const Syscall::Args *args, const uintptr_t number);

}

#endif
