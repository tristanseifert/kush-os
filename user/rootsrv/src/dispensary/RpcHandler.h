#ifndef DISPENSARY_RPCHANDLER_H
#define DISPENSARY_RPCHANDLER_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>

namespace rpc {
struct RpcPacket;
}

namespace dispensary {
/**
 * RPC interface to the dispensary; allows tasks to look up ports and register them.
 */
class RpcHandler {
    public:
        /// maximum size of RPC messages for this endpoint
        constexpr static const size_t kMaxMsgLen = (1024 * 2);

    public:
        /// Initialize the shared rpc handler object
        static void init() {
            gShared = new RpcHandler;
        }

    private:
        RpcHandler();

        /// Message loop
        void main();
        /// Handles a lookup request
        void handleLookup(const struct MessageHeader *, const rpc::RpcPacket *, const size_t);

    private:
        static RpcHandler *gShared;

        /// port on which we listen for requests
        uintptr_t portHandle = 0;
        /// as long as this set, the worker will continue execute
        std::atomic_bool run;
        /// runloop thread
        std::unique_ptr<std::thread> worker;
};
}

#endif
