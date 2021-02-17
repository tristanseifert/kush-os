#ifndef KERNEL_IPC_PORT_H
#define KERNEL_IPC_PORT_H

#include <stdint.h>

#include <handle/Manager.h>
#include <runtime/List.h>
#include <runtime/Queue.h>
#include <sched/Blockable.h>

#include <platform.h>

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

        /// Sends a message to the port
        int send(const void *msgBuf, const size_t msgLen);
        /// Receives a message from the port, optionally blocking the caller.
        int receive(Handle &sender, void *msgBuf, const size_t msgBufLen, const uint64_t blockUntil);

    public:
        /// Returns the port's kernel handle.
        const inline Handle getHandle() const {
            // TODO: does this need locking?
            return this->handle;
        }

        /// Returns whether there's any messages pending on the port
        const inline bool messagesPending() {
            RW_LOCK_READ_GUARD(this->lock);
            return !this->messages.empty();
        }

        /// Sets the queue depth
        void setQueueDepth(const size_t depth) {
            RW_LOCK_WRITE_GUARD(this->lock);
            this->maxMessages = depth;
        }

    private:
        // Blocking object for receiving a message
        class Blocker: public sched::Blockable {
            friend class Port;

            /// Construct a new blocker for the given port.
            Blocker(Port *_port) : port(_port) {}

            public:
                /// We're signalled whenever the port's message queue is not empty.
                bool isSignalled() override {
                    return this->port->messagesPending();
                }

                /// Reset the wake-up flag; this does nothing, you'll have to drain all messages
                void reset() override {
                    __atomic_clear(&this->signalled, __ATOMIC_RELEASE);
                }

                /// Other threads may invoke this method when a message is enqueued.
                void messageQueued() {
                    //bool yes = true, no = false;
                    //if(__atomic_compare_exchange(&this->signalled, &no, &yes, false, __ATOMIC_ACQUIRE, __ATOMIC_RELEASE)) {
                    if(!__atomic_test_and_set(&this->signalled, __ATOMIC_RELEASE)) {
                        this->unblock();
                    }
                }

            private:
                Port *port = nullptr;
                bool signalled = false;
        };

        // Message info
        struct Message {
            /// send timestamp
            uint64_t timestamp;
            /// thread handle of sender
            Handle sender;
            /// length of the message content, in bytes
            size_t contentLen;
            /// pointer to the message content
            void *content = nullptr;
            /// when set, we're responsible for deleting the content buffer
            bool ownsContent = false;

            Message() {
                this->timestamp = platform_timer_now();
            };
            /// Creates a new message struct that allocates a buffer, and copies the given payload
            Message(const void *buf, const size_t _len) : contentLen(_len) {
                this->sender = sched::Thread::current()->handle;

                this->timestamp = platform_timer_now();
                this->content = mem::Heap::alloc(_len);
                REQUIRE(this->content, "failed to alloc message buffer");
                this->ownsContent = true;

                memcpy(this->content, buf, _len);
            }

            /// if we own the content buffer, release it
            void done() {
                if(this->ownsContent) {
                    this->ownsContent = false;
                    mem::Heap::free(this->content);
                    this->content = nullptr;
                }
            }
        };

    private:
        /// maximum length of message
        constexpr static const size_t kMaxMsgLen = 4096;

        static void initAllocator();

    private:
        /// lock to protect access to all data inside the port
        DECLARE_RWLOCK(lock);

        /// kernel object handle for this port
        Handle handle;

        /// thread that's waiting to receive from this port, if any
        sched::Thread *receiver = nullptr;
        /// the blocker object for the receiver
        Blocker *receiverBlocker = nullptr;

        /// maximum queue size (0 = unlimited)
        size_t maxMessages = 0;
        /// pending messages
        rt::Queue<Message> messages;
};
}



#endif
