#ifndef RPC_MESSAGELOOP_H
#define RPC_MESSAGELOOP_H

#include <atomic>
#include <cassert>
#include <cstdint>

class MessageLoop {
    public:
        /// Set up the global message loop.
        static void init() {
            assert(!gShared);
            gShared = new MessageLoop;
        }

        /// Return the global message loop.
        static MessageLoop * _Nonnull the() {
            return gShared;
        }

        /// Main loop for the message loop
        void run();

        MessageLoop();
        ~MessageLoop();

    private:
        /// name of the service to register
        constexpr static const char * _Nonnull kServiceName = "me.blraaz.rpc.driverman";
        /// max received message length
        constexpr static const size_t kRxBufSize = (16 * 1024);

        static MessageLoop * _Nonnull gShared;

        /// message receive buffer
        void * _Nonnull rxBuf;

        /// process messages as long as this flag is set
        std::atomic_bool shouldRun;
        /// port to receive requests on
        uintptr_t port = 0;
};

#endif
