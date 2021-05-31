#ifndef KERNEL_IPC_PORT_H
#define KERNEL_IPC_PORT_H

#include <stdint.h>

#include <handle/Manager.h>
#include <runtime/List.h>
#include <runtime/SmartPointers.h>
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
class Port: public rt::SharedFromThis<Port> {
    public:
        /// Allocates a new port
        static rt::SharedPtr<Port> alloc();

        // you ought to use the above functions instead
        Port() = default;
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
            Blocker(const rt::SharedPtr<Port> &_port) : port(_port) {}

            public:
                inline static auto make(Port *port) {
                    return make(port->sharedFromThis());
                }

                static rt::SharedPtr<Blocker> make(const rt::SharedPtr<Port> &port) {
                    rt::SharedPtr<Blocker> ptr(new Blocker(port));
                    ptr->us = rt::WeakPtr<Blocker>(ptr);
                    return ptr;
                }

            public:
                /// We're signalled whenever the port's message queue is not empty.
                bool isSignalled() override {
                    return this->port->messagesPending();
                }

                /// Reset the wake-up flag; this does nothing, you'll have to drain all messages
                void reset() override {
                    __atomic_clear(&this->unblockedSignalled, __ATOMIC_RELAXED);
                    __atomic_clear(&this->signalled, __ATOMIC_RELEASE);
                }

                /// Other threads may invoke this method when a message is enqueued.
                void messageQueued() {
                    if(!this->blocker) {
                        this->unblockedSignalled = true;
                        return;
                    }

                    // perform unblock if we are blocking AND haven't signalled
                    if( __atomic_load_n(&this->isBlocking, __ATOMIC_RELAXED) &&
                       !__atomic_test_and_set(&this->signalled, __ATOMIC_RELEASE)) {
                        this->blocker->unblock(this->us.lock());
                    }
                }

                /// Clear the "is blocking" flag
                void didUnblock() override {
                    Blockable::didUnblock();
                    __atomic_store_n(&this->isBlocking, false, __ATOMIC_RELAXED);
                }

                /// Sets the receive blockable object of the port when we're validly blocked on.
                int willBlockOn(const rt::SharedPtr<sched::Thread> &t) override {
                    if(int err = Blockable::willBlockOn(t)) {
                        return err;
                    }

                    if(this->signalled || this->unblockedSignalled) {
                        return -1;
                    }
                    // set blocking flag
                    __atomic_store_n(&this->isBlocking, true, __ATOMIC_RELAXED);
                    return 0;
                }

            private:
                rt::WeakPtr<Blocker> us;

                /// port we're gonna signal
                rt::SharedPtr<Port> port;

                bool unblockedSignalled = false;
                bool signalled = false;
                bool isBlocking = false;
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
        constexpr static const size_t kMaxMsgLen = 4096 * 9;
        /// Maximum number of messages that may be queued at once
        constexpr static const size_t kDefaultMaxMessages = 100;

        /// whether dequeuing of messages is logged
        static bool gLogQueuing;

        static void initAllocator();

    private:
        /// lock to protect access to all data inside the port
        DECLARE_RWLOCK(lock);

        /// kernel object handle for this port
        Handle handle;

        /// the blocker object for the receiver
        rt::SharedPtr<Blocker> receiverBlocker;

        /// maximum queue size (0 = unlimited)
        size_t maxMessages = kDefaultMaxMessages;
        /// pending messages
        rt::Queue<Message> messages;
};
}



#endif
