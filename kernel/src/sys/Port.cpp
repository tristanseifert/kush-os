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

static const bool gLogMsg = true;

/**
 * Receive message buffer; these must always be 16-byte aligned. This is a header for a message
 * buffer, which is allocated in 16-byte chunks.
 */
struct sys::RecvInfo {
    /// thread handle of the thread that sent this message
    Handle thread;
    /// task handle of the task that contains the thread
    Handle task;
    /// flags; currently unused
    uint16_t flags;
    /// length of the message (bytes)
    uint16_t messageLength;

    /// data buffer; must be allocated in 16-byte chunks
    uint8_t data[] __attribute__((aligned(16)));
} __attribute__((aligned(16)));

static_assert(offsetof(RecvInfo, data) % 16 == 0, "RecvInfo data must be 16 byte aligned");


/**
 * Sends message data to a port.
 *
 * @param portHandle Handle of the port to send a message to
 * @param msgPtr Pointer to the message data structure
 * @param msgLen Number of bytes of message data to copy
 *
 * @return 0 on success or a negative error code
 */
intptr_t sys::PortSend(const Handle portHandle, const void *msgPtr, const size_t msgLen) {
    int err;

    if(gLogMsg) log("%4u %4u) PortSend($%p'h, %p, %lu)", sched::Task::current()->pid, sched::Thread::current()->tid, portHandle, msgPtr, msgLen);

    // validate the message buffer
    if(!Syscall::validateUserPtr(msgPtr, msgLen)) {
        return Errors::InvalidPointer;
    }

    // look up the port 
    auto port = handle::Manager::getPort(portHandle);
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
 *
 * @param portHandle Port to receive from
 * @param recvPtr Receive buffer structure
 * @param recvLen Total bytes of receive struct space allocated
 * @param timeout How long to wait for a message, in nanoseconds.
 *
 * @return Negative error code or the number of bytes of message data returned
 */
intptr_t sys::PortReceive(const Handle portHandle, RecvInfo *recvPtr, const size_t recvLen,
        const size_t timeout) {
    int err;
    auto task = sched::Task::current();
    if(!task) {
        return Errors::GeneralError;
    }

    if(gLogMsg) log("%4u %4u) PortReceive($%p'h, %p, %lu, %lu)", task->pid, sched::Thread::current()->tid, portHandle, recvPtr, recvLen, timeout);

    // basic validation of lengths
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
    if(!Syscall::validateUserPtr(recvPtr, recvLen)) {
        return Errors::InvalidPointer;
    }

    // get port handle and ensure we own it
    auto port = handle::Manager::getPort(portHandle);
    if(!port) {
        return Errors::InvalidHandle;
    }
    if(!task->ownsPort(port)) {
        return Errors::PermissionDenied;
    }

    // calculate timeout
    uint64_t timeoutNs = 0;

    if(timeout) {
        // if it's the max value of the type, block forever
        if(timeout == UINTPTR_MAX) {
            timeoutNs = UINT64_MAX;
        } 
        // otherwise, it's a timeout in usec
        else {
            timeoutNs = platform_timer_now() + (timeout * 1000ULL); // usec -> ns
        }
    }

    // receive from port
    Handle senderThreadHandle = Handle::Invalid;

    err = port->receive(senderThreadHandle, recvPtr->data, msgBufLen, timeout);

    if(err < 0) {
        // receive timed out
        if(err == -1) {
            // memset(recvPtr, 0, sizeof(RecvInfo));
            return Errors::Timeout;
        }
        // other receive error
        else {
            log("failed to receive from port %p ($%llx'h): %d", static_cast<void *>(port),
                    port->getHandle(), err);
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
 * Updates a port's parameters. The caller must be the owner of the port.
 *
 * @param portHandle Handle of the port to modify
 * @param queueDepth Maximum number of messages that may be pending on the port
 *
 * @return 0 on success or a negative error code
 */
intptr_t sys::PortSetParams(const Handle portHandle, const uintptr_t queueDepth) {
    auto task = sched::Task::current();
    if(!task) {
        return Errors::GeneralError;
    }

    // convert the port handle
    auto port = handle::Manager::getPort(portHandle);
    if(!port) {
        return Errors::InvalidHandle;
    }

    // ensure the handle belongs to this task
    if(!task->ownsPort(port)) {
        return Errors::PermissionDenied;
    }

    // update params
    port->setQueueDepth(queueDepth);

    return Errors::Success;
}

/**
 * Allocates a new port.
 *
 * @return Valid handle to the newly created port or a negative error code
 */
intptr_t sys::PortAlloc() {
    // allocate the port
    auto port = ipc::Port::alloc();
    if(!port) {
        return Errors::GeneralError;
    }

    // register it with the currently executing task
    auto task = sched::Task::current();
    task->addPort(port);

    // return its handle
    return static_cast<intptr_t>(port->getHandle());
}

/**
 * Deallocates a port. The caller must be the owner of the port.
 *
 * @param portHandle Port to deallocate
 *
 * @return 0 on success, or a negative error code
 */
intptr_t sys::PortDealloc(const Handle portHandle) {
    auto task = sched::Task::current();
    if(!task) {
        return Errors::GeneralError;
    }

    // convert the port handle
    auto port = handle::Manager::getPort(portHandle);
    if(!port) {
        return Errors::InvalidHandle;
    }

    // ensure the handle belongs to this task
    if(!task->ownsPort(port)) {
        return Errors::PermissionDenied;
    }

    // actually perform the removal and release the port
    task->removePort(port);

    return Errors::Success;
}
