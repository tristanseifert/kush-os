#include "Port.h"

#include "mem/SlabAllocator.h"

#include <arch/critical.h>
#include <log.h>

using namespace ipc;

static char gAllocBuf[sizeof(mem::SlabAllocator<Port>)] __attribute__((aligned(64)));
static mem::SlabAllocator<Port> *gPortAllocator = nullptr;

// set to log adding and removing items from queue
bool Port::gLogQueuing = false;

/**
 * Deleter that will release a port back to the appropriate allocation pool
 */
struct PortDeleter {
    void operator()(Port *obj) const {
        gPortAllocator->free(obj);
    }
};

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
rt::SharedPtr<Port> Port::alloc() {
    // allocate the port
    if(!gPortAllocator) initAllocator();
    auto port = gPortAllocator->alloc();

    // create owning ptr and allocate handle
    rt::SharedPtr<Port> ptr(port, PortDeleter());

    port->handle = handle::Manager::makePortHandle(ptr);
    REQUIRE(((uintptr_t) port->handle), "failed to create port handle for %p", port);

    port->receiverBlocker = Blocker::make(port);

    return ptr;
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
 * Sends a message to the port. This will add the message to the message queue (if space permits)
 * and then wake up any waiting thread.
 *
 * Note that this does not guarantee the destination task has actually received the message, only
 * that we've queued it.
 */
int Port::send(const void *msgBuf, const size_t msgLen) {
    DECLARE_CRITICAL();

    // ensure message isn't too long
    if(!msgBuf || msgLen > kMaxMsgLen) {
        return -2;
    }

    if(gLogQueuing) {
        log("sending %p (%d bytes) to $%p'h", msgBuf, msgLen, this->handle);
    }

    // validate we won't insert too many items
    CRITICAL_ENTER();
    RW_LOCK_WRITE(&this->lock);

    if(this->maxMessages && this->messages.size() >= this->maxMessages) {
        RW_UNLOCK_WRITE(&this->lock);
        CRITICAL_EXIT();
        return -1;
    }

    // insert the message
    this->messages.push_back(Message(msgBuf, msgLen));

    if(gLogQueuing) {
        log("P%p enqueued %d", this->handle, this->messages.size());
    }

    RW_UNLOCK_WRITE(&this->lock);
    CRITICAL_EXIT();

    // wake any pending task
    this->receiverBlocker->messageQueued();

    return 0;

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
    using BlockOnReturn = sched::Thread::BlockOnReturn;

    DECLARE_CRITICAL();

    CRITICAL_ENTER();
    RW_LOCK_WRITE(&this->lock);

    /*
     * Reset the block flag; this way if we receive a message at some point while we're in the
     * process of setting up for blocking we'll be able to detect it.
     */
    this->receiverBlocker->reset();
    auto thread = sched::Thread::current();

    BlockOnReturn unblockReason;
    int ret = -1;

    // pop messages off the message queue, if any
    if(!this->messages.empty()) {
        auto msg = this->messages.pop();
        const auto toCopy = (msg.contentLen > msgBufLen) ? msgBufLen : msg.contentLen;

        memcpy(msgBuf, msg.content, toCopy);
        sender = msg.sender;
        msg.done();

        if(gLogQueuing) {
            log("P%p dequeued (ts %llu pending %u)", this->handle, msg.timestamp, this->messages.size());
        }

        RW_UNLOCK_WRITE(&this->lock);
        CRITICAL_EXIT();
        return toCopy;
    }
    // no messages on the queue, but we don't want to block; so abort
    if(!blockUntil) {
        ret = -1;
        goto failedUnlock;
    }

    RW_UNLOCK_WRITE(&this->lock);
    CRITICAL_EXIT();

    /*
     * Block thread on the receiver. Return immediately if it timed out, otherwise we try to see if
     * we received a message, whether that is because we actually got one or the block was
     * aborted.
     */
    unblockReason = thread->blockOn(this->receiverBlocker);
    if(unblockReason == BlockOnReturn::Error) {
        return -1;
    }

    // after wake-up, see if we have a message to copy out
    CRITICAL_ENTER();
    RW_LOCK_WRITE(&this->lock);

    if(!this->messages.empty()) {
        // copy out the message at the head of the queue
        auto msg = this->messages.pop();
        const auto toCopy = (msg.contentLen > msgBufLen) ? msgBufLen : msg.contentLen;

        memcpy(msgBuf, msg.content, toCopy);
        sender = msg.sender;

        RW_UNLOCK_WRITE(&this->lock);
        CRITICAL_EXIT();
        return toCopy;
    } 
    // if we get here and the queue is empty, the wakeup was spurious

failedUnlock:;
    // failure return
    RW_UNLOCK_WRITE(&this->lock);
    CRITICAL_EXIT();
    return ret;
}

