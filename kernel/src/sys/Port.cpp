#include "Handlers.h"

#include "ipc/Port.h"
#include "sched/Scheduler.h"
#include "sched/Task.h"

#include "handle/Manager.h"

#include <arch.h>
#include <arch/critical.h>
#include <platform.h>
#include <log.h>

using namespace sys;

/**
 * Receive message buffer; these must always be 16-byte aligned. This is a header for a message
 * buffer, which is allocated in 16-byte chunks.
 */
struct RecvInfo {
    /// thread handle of the thread that sent this message
    Handle thread;
    /// task handle of the task that contains the thread
    Handle task;
    /// flags; currently unused
    uint16_t flags;
    /// length of the message (bytes)
    uint16_t messageLength;

    /// reserved fields (for padding)
    uint32_t reserved[1];

    /// data buffer; must be allocated in 16-byte chunks
    uint8_t data[];
} __attribute__((aligned(16)));

static_assert(offsetof(RecvInfo, data) % 16 == 0, "RecvInfo data must be 16 byte aligned");


/**
 * Sends message data to a port.
 *
 * Arg0: Port handle
 * Arg1: Message buffer pointer (16-byte aligned)
 * Arg2: Message length
 */
int sys::PortSend(const Syscall::Args *args, const uintptr_t number) {
    int err;

    // validate the message buffer
    const auto msgLen = args->args[2];
    auto msgPtr = reinterpret_cast<RecvInfo *>(args->args[1]);
    if(!Syscall::validateUserPtr(msgPtr, msgLen)) {
        return Errors::InvalidPointer;
    }

    // look up the port 
    auto port = handle::Manager::getPort(static_cast<Handle>(args->args[0]));
    if(!port) {
        return Errors::InvalidHandle;
    }

    // perform the send
    err = port->send(msgPtr, msgLen);

    if(!err) {
        return Errors::Success;
    } else if(err == -1) {
        return Errors::TryAgain;
    } else {
        return Errors::GeneralError;
    }
}

/**
 * Receives a message on the given port.
 */
int sys::PortReceive(const Syscall::Args *args, const uintptr_t number) {
    int err;
    auto task = sched::Task::current();
    if(!task) {
        return Errors::GeneralError;
    }

    // basic validation of lengths
    const auto recvLen = args->args[2];
    if(recvLen < sizeof(RecvInfo)) {
        return Errors::InvalidArgument;
    } else if(recvLen % 16) {
        return Errors::InvalidArgument;
    }

    const auto msgBufLen = recvLen - offsetof(RecvInfo, data);
    if(msgBufLen % 16) {
        return Errors::InvalidArgument;
    }

    // validate the destination buffer
    auto recvPtr = reinterpret_cast<RecvInfo *>(args->args[1]);
    if(!Syscall::validateUserPtr(recvPtr, recvLen)) {
        return Errors::InvalidPointer;
    }

    // get port handle and ensure we own it
    auto port = handle::Manager::getPort(static_cast<Handle>(args->args[0]));
    if(!port) {
        return Errors::InvalidHandle;
    }
    if(!task->ownsPort(port)) {
        return Errors::PermissionDenied;
    }

    // calculate timeout
    uint64_t timeout = 0;

    if(args->args[3]) {
        // if it's the max value of the type, block forever
        if(args->args[3] == UINTPTR_MAX) {
            timeout = UINT64_MAX;
        } 
        // otherwise, it's a timeout in usec
        else {
            timeout = (args->args[3] * 1000); // usec -> ns
            timeout += platform_timer_now();
        }
    }

    // receive from port
    Handle senderThreadHandle;
    err = port->receive(senderThreadHandle, recvPtr->data, msgBufLen, timeout);

    if(err < 0) {
        // receive timed out
        if(err == -1) {
            // memset(recvPtr, 0, sizeof(RecvInfo));
            return Errors::Timeout;
        }
        // other receive error
        else {
            log("failed to receive from port %p ($%08x'h): %d", port, port->getHandle(), err);
            return Errors::GeneralError;
        }
    }

    // resolve the thread
    auto senderThread = handle::Manager::getThread(senderThreadHandle);
    if(senderThread && senderThread->task) {
        recvPtr->task = senderThread->task->handle;
    } else {
        recvPtr->task = static_cast<Handle>(0);
    }

    // write info on the received message to it
    recvPtr->thread = senderThreadHandle;
    recvPtr->flags = 0;
    recvPtr->messageLength = err;

    return err;
}

/**
 * Updates a port's parameters.
 *
 * The caller must be the owner of the port.
 *
 * Arg0: New port queue size
 */
int sys::PortSetParams(const Syscall::Args *args, const uintptr_t number) {
    auto task = sched::Task::current();
    if(!task) {
        return Errors::GeneralError;
    }

    // convert the port handle
    auto port = handle::Manager::getPort(static_cast<Handle>(args->args[0]));
    if(!port) {
        return Errors::InvalidHandle;
    }

    // ensure the handle belongs to this task
    if(!task->ownsPort(port)) {
        return Errors::PermissionDenied;
    }

    // update params
    port->setQueueDepth(args->args[1]);

    return Errors::Success;
}

/**
 * Allocates a new port.
 */
int sys::PortAlloc(const Syscall::Args *args, const uintptr_t number) {
    // allocate the port
    auto port = ipc::Port::alloc();
    if(!port) {
        return Errors::GeneralError;
    }

    // register it with the currently executing task
    auto task = sched::Task::current();
    task->addPort(port);

    // return its handle
    return static_cast<int>(port->getHandle());
}

/**
 * Deallocates a port.
 *
 * Arg0 is the port handle. The calling task must have been the one that created the port.
 */
int sys::PortDealloc(const Syscall::Args *args, const uintptr_t number) {
    auto task = sched::Task::current();
    if(!task) {
        return Errors::GeneralError;
    }

    // convert the port handle
    auto port = handle::Manager::getPort(static_cast<Handle>(args->args[0]));
    if(!port) {
        return Errors::InvalidHandle;
    }

    // ensure the handle belongs to this task
    if(!task->ownsPort(port)) {
        return Errors::PermissionDenied;
    }

    // actually perform the removal and release the port
    task->removePort(port);
    ipc::Port::free(port);

    return Errors::Success;
}
