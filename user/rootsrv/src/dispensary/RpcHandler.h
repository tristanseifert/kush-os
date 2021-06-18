#ifndef DISPENSARY_RPCHANDLER_H
#define DISPENSARY_RPCHANDLER_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <thread>
#include <span>

namespace rpc {
struct RpcPacket;
enum class RootSrvDispensaryEpType: uint32_t;
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
        /// Registers a new port.
        void handleRegister(const struct MessageHeader *, const rpc::RpcPacket *, const size_t);

        void reply(const rpc::RpcPacket *packet, const rpc::RootSrvDispensaryEpType type,
            const std::span<uint8_t> &buf);

    private:
        static RpcHandler *gShared;

        /// whether dispensary resolutions are logged
        constexpr static const bool kLogRequests{false};

        /// port on which we listen for requests
        uintptr_t portHandle = 0;
        /// as long as this set, the worker will continue execute
        std::atomic_bool run;
        /// runloop thread
        std::unique_ptr<std::thread> worker;
};
}

#endif
