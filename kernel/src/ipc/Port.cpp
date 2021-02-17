#include "Port.h"

#include "mem/SlabAllocator.h"

#include <log.h>

using namespace ipc;

static char gAllocBuf[sizeof(mem::SlabAllocator<Port>)] __attribute__((aligned(64)));
static mem::SlabAllocator<Port> *gPortAllocator = nullptr;

/**
 * Initializes the port allocator.
 */
void Port::initAllocator() {
    gPortAllocator = reinterpret_cast<mem::SlabAllocator<Port> *>(&gAllocBuf);
    new(gPortAllocator) mem::SlabAllocator<Port>();
}

/**
 * Allocates a new port.
 */
Port *Port::alloc() {
    if(!gPortAllocator) initAllocator();
    auto port = gPortAllocator->alloc();

    return port;
}

/**
 * Frees a previously allocated port.
 */
void Port::free(Port *ptr) {
    gPortAllocator->free(ptr);
}



/**
 * Initializes a new message port.
 */
Port::Port() {
    this->handle = handle::Manager::makePortHandle(this);
    REQUIRE(((uintptr_t) this->handle), "failed to create port handle for %p", this);
}

/**
 * Releases the message port's resources.
 *
 * Any processes waiting to receive on us will be woken up at this point.
 */
Port::~Port() {
    // release the handle
    handle::Manager::releasePortHandle(this->handle);
}



/**
 * Receives a message on the port.
 *
 * @param sender The sender of the received message is written into this field.
 * @param msgBuf Buffer to store the message data
 * @param msgBufLen Maximum number of bytes that may be stored in `msgBuf`
 * @param blockUntil Time point until which to block. 0 indicates no blocking (polling) and a value
 *                   of UINT64_MAX indicates blocking forever.
 *
 * @return Number of bytes of message data actually written, or a negative error code.
 */
int Port::receive(Handle &sender, void *msgBuf, const size_t msgBufLen, const uint64_t blockUntil) {
    RW_LOCK_WRITE(&this->lock);

    int err = 0, ret = -1;

    // any threads waiting to send?

    // if not, and not blocking, bail
    if(!blockUntil) {
        ret = -1;
        goto failedUnlock;
    }
    // otherwise, block waiting for a thread to send

    RW_UNLOCK_WRITE(&this->lock);

    // if wake-up from block was spurious, return
    if(err) {
        return -1;
    }

    // copy out the message data

    return 0;

failedUnlock:;
    // failure return
    RW_UNLOCK_WRITE(&this->lock);
    return ret;
}

