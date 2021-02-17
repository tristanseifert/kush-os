#ifndef KERNEL_IPC_PORT_H
#define KERNEL_IPC_PORT_H

#include <stdint.h>

#include <handle/Manager.h>
#include <runtime/List.h>

namespace sched {
struct Thread;
}

namespace ipc {
/**
 * Ports are unidirectional communications endpoints that tasks may use to receive messages.
 *
 * Threads may block on a port; only one thread may block for receiving, while multiple threads
 * may be blocked waiting to send on a port.
 */
class Port {
    public:
        /// Allocates a new port
        static Port *alloc();
        /// Releases a previously allocated port
        static void free(Port *);

        // you ought to use the above functions instead
        Port();
        ~Port();

        /// Receives a message from the port, optionally blocking the caller.
        int receive(Handle &sender, void *msgBuf, const size_t msgBufLen, const uint64_t blockUntil);

    public:
        /// Returns the port's kernel handle.
        const inline Handle getHandle() const {
            // TODO: does this need locking?
            return this->handle;
        }

    private:
        static void initAllocator();

    private:
        /// lock to protect access to all data inside the port
        DECLARE_RWLOCK(lock);

        /// kernel object handle for this port
        Handle handle;

        /// thread that's waiting to receive from this port, if any
        sched::Thread *receiver = nullptr;
        /// threads waiting to send to this port
        rt::List<sched::Thread *> waitingToSend;
};
}

#endif
