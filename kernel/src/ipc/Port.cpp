#include "Port.h"

#include "mem/SlabAllocator.h"

#include <arch/critical.h>
#include <log.h>

using namespace ipc;

static char gAllocBuf[sizeof(mem::SlabAllocator<Port>)] __attribute__((aligned(64)));
static mem::SlabAllocator<Port> *gPortAllocator = nullptr;

// set to log adding and removing items from queue
#define LOG_QUEUING                     0

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
 * Sends a message to the port. This will add the message to the message queue (if space permits)
 * and then wake up any waiting thread.
 *
 * Note that this does not guarantee the destination task has actually received the message, only
 * that we've queued it.
 */
int Port::send(const void *msgBuf, const size_t msgLen) {
    DECLARE_CRITICAL();
    Blocker *blocker = nullptr;

    // ensure message isn't too long
    if(!msgBuf || msgLen > kMaxMsgLen) {
        return -2;
    }

#if LOG_QUEUING
    log("sending %p (%d bytes) to %08x'h", msgBuf, msgLen, this->handle);
#endif

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
    blocker = this->receiverBlocker;

#if LOG_QUEUING
    log("enqueued %d %p", this->messages.size(), blocker);
#endif

    RW_UNLOCK_WRITE(&this->lock);
    CRITICAL_EXIT();

    // wake any pending task
    if(blocker) {
        blocker->messageQueued();
    }

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
    DECLARE_CRITICAL();

    CRITICAL_ENTER();
    RW_LOCK_WRITE(&this->lock);

    int err = 0, ret = -1;

    // pop messages off the message queue, if any
    if(!this->messages.empty()) {
        auto msg = this->messages.pop();
        const auto toCopy = (msg.contentLen > msgBufLen) ? msgBufLen : msg.contentLen;

        memcpy(msgBuf, msg.content, toCopy);
        sender = msg.sender;
        msg.done();

#if LOG_QUEUING
        log("dequeued (ts %llu pending %u)", msg.timestamp, this->messages.size());
#endif

        RW_UNLOCK_WRITE(&this->lock);
        CRITICAL_EXIT();
        return toCopy;
    }
    // no messages on the queue, but we don't want to block; so abort
    if(!blockUntil) {
        ret = -1;
        goto failedUnlock;
    }

    // otherwise, block waiting for a message to be received
    if(this->receiver || this->receiverBlocker) {
        // cannot block if someone else is! (XXX: fix this later)
        log("refusing to block on port %p", this);
        goto failedUnlock;
    } else {
        // set up the blocker and store it, then release the lock
        auto thread = sched::Thread::current();

        this->receiver = thread;
        this->receiverBlocker = new Blocker(this);

        RW_UNLOCK_WRITE(&this->lock);
        CRITICAL_EXIT();

        // perform block
        //log("blocking on: %p", this->receiverBlocker);
        err = thread->blockOn(this->receiverBlocker);
    }

    // if wake-up from block was spurious, return
    if(err) {
        return -1;
    }

    // after wake-up, see if we have a message to copy out
    CRITICAL_ENTER();
    RW_LOCK_WRITE(&this->lock);

    this->receiver = nullptr;
    if(this->receiverBlocker) {
        delete this->receiverBlocker;
        this->receiverBlocker = nullptr;
    }

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

