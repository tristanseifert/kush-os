/*
 * Function definitions for all non-trivial syscalls
 */
#ifndef KERNEL_SYS_HANDLERS_H
#define KERNEL_SYS_HANDLERS_H

#include <cstddef>

#include "Syscall.h"
#include "Errors.h"
#include "handle/Manager.h"
#include "runtime/SmartPointers.h"

namespace sys {

struct RecvInfo;

/// Sends a message to the given port.
intptr_t PortSend(const Handle portHandle, const void *msgPtr, const size_t msgLen);
/// Waits to receive a message on a port.
intptr_t PortReceive(const Handle portHandle, RecvInfo *recvPtr, const size_t recvLen,
        const size_t timeout);
/// Updates a port's queue size
intptr_t PortSetParams(const Handle portHandle, const uintptr_t queueDepth);
/// Allocates a new port.
intptr_t PortAlloc();
/// Releases a previously allocated port.
intptr_t PortDealloc(const Handle portHandle);

/// Sets the given notification bits in the thread.
intptr_t NotifySend(const Handle threadHandle, const uintptr_t bits);
/// Blocks waiting to receive a notification.
intptr_t NotifyReceive(uintptr_t mask, const uintptr_t timeout);


struct VmInfo;
struct VmTaskInfo;
struct VmMapRequest;
enum VmFlags: uintptr_t;
enum VmQueryKey: uintptr_t;

/// Allocates a virtual memory region backed by physical memory
intptr_t VmAllocPhysRegion(const uintptr_t physAddr, const size_t length, const VmFlags flags);
/// Allocate a virtual memory region backed by anonymous memory
intptr_t VmAllocAnonRegion(const uintptr_t length, const VmFlags flags);
/// Releases a previously allocated VM region
intptr_t VmDealloc(const Handle vmHandle);
/// Update permissions (R/W/X flags) of a VM region
intptr_t VmRegionUpdatePermissions(const Handle vmHandle, const VmFlags newFlags);
/// Resizes a VM region
intptr_t VmRegionResize(const Handle vmHandle, const size_t newSize, const VmFlags flags);
/// Maps a VM region into a task
intptr_t VmRegionMap(const Handle vmHandle, const Handle taskHandle, const uintptr_t base,
        const size_t length, const VmFlags flags);
/// Maps a VM region into a task optionally searching for an address
intptr_t VmRegionMapEx(const Handle vmHandle, const Handle taskHandle, VmMapRequest *req,
        const size_t reqLen);
/// Unmaps a VM region
intptr_t VmRegionUnmap(const Handle vmHandle, const Handle taskHandle);
/// Gets information about a VM region
intptr_t VmRegionGetInfo(const Handle vmHandle, const Handle taskHandle, VmInfo *infoPtr,
        const size_t infoLen);
/// Gets information on a task's virtual memory environment
intptr_t VmTaskGetInfo(const Handle taskHandle, VmTaskInfo *infoPtr, const size_t infoLen);
/// Returns the handle for the region containing an address.
intptr_t VmAddrToRegion(const Handle taskHandle, const uintptr_t vmAddr);
/// Translates an array of one or more virtual addresses to physical.
intptr_t VmTranslateVirtToPhys(const Handle taskHandle, const uintptr_t *inVirtAddrs,
        uintptr_t *outPhysAddrs, const size_t numAddresses);
/// Gets information from the memory subsystem.
intptr_t VmQueryParams(const VmQueryKey what, void *outPtr, const size_t outPtrBytes);

/// Get thread handle of currently executing thread.
intptr_t ThreadGetHandle();
/// Yield the thread's time slice allowing higher priority threads to run
intptr_t ThreadYield();
/// Microsecond sleep
intptr_t ThreadUsleep(const uintptr_t usecs);
/// Creates a new userspace thread
intptr_t ThreadCreate(const uintptr_t entryPtr, const uintptr_t entryParam,
        const uintptr_t stackPtr, const uintptr_t rawFlags);
/// Destroys an userspace thread
intptr_t ThreadDestroy(const Handle threadHandle);
/// Sets the thread priority
intptr_t ThreadSetPriority(const Handle threadHandle, const intptr_t priority);
/// Sets the thread's notification mask
intptr_t ThreadSetNoteMask(const Handle threadHandle, const uintptr_t newMask);
/// Sets the descriptive name of the thread
intptr_t ThreadSetName(const Handle threadHandle, const char *namePtr, const size_t nameLen);
/// Resumes the given paused thread
intptr_t ThreadResume(const Handle threadHandle);
/// Waits for the specified thread to terminate
intptr_t ThreadJoin(const Handle threadHandle, const uintptr_t timeout);


/// Returns the currently executing task's handle
intptr_t TaskGetHandle();
/// Create a new task
intptr_t TaskCreate(const Handle parentTaskHandle);
/// Terminate an existing task
intptr_t TaskTerminate(const Handle taskHandle, const intptr_t code);
/// Completes task initialization and begins its main thread execution
intptr_t TaskInitialize(const Handle taskHandle, const uintptr_t userPc,
        const uintptr_t userStack);
/// Set the descriptive name of a task
intptr_t TaskSetName(const Handle taskHandle, const char *namePtr, const size_t nameLen);
/// Debug output to kernel log console
intptr_t TaskDbgOut(const char *msgPtr, const size_t msgLen);


/// Installs an IRQ handler
intptr_t IrqHandlerInstall(const uintptr_t irqNum, const Handle threadHandle,
        const uintptr_t bits);
/// Removes an IRQ handler
intptr_t IrqHandlerRemove(const Handle irqHandle);
/// Updates an IRQ handler's thread and notification bits
intptr_t IrqHandlerUpdate(const Handle irqHandle, const Handle threadHandle, const uintptr_t bits);
/// Gets info on an IRQ handler.
intptr_t IrqHandlerGetInfo(const Handle irqHandle, const uintptr_t what);
/// Allocate a processor local IRQ handler.
intptr_t IrqHandlerAllocCoreLocal(const Handle threadHandle, const uintptr_t bits);
}

#endif
