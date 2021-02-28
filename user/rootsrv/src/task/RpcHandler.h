#ifndef TASK_RPCHANDLER_H
#define TASK_RPCHANDLER_H

#include <atomic>
#include <cstddef>
#include <memory>
#include <span>
#include <string_view>
#include <thread>

#include "rpc/RootSrvTaskEndpoint.hpp"
#include <rpc/RpcPacket.hpp>

struct MessageHeader;

namespace task {
/**
 * Handle RPC requests for the task creation endpoint.
 *
 * This encapsulates the port on which we listen, as well as the thread that processes messages
 * received on it.
 */
class RpcHandler {
    public:
        /// Name of the task service
        static const std::string_view kPortName;
        /// maximum length of messages to be received by this handler; this includes headers
        constexpr static const size_t kMaxMsgLen = (1024 * 16);

    public:
        static void init() {
            gShared = new RpcHandler;
        }

    private:
        RpcHandler();

        void main();
        /// Handles a "create task" request
        void handleCreate(const struct MessageHeader *, const rpc::RpcPacket *);

        void reply(const rpc::RpcPacket *, const rpc::RootSrvTaskEpType, const std::span<uint8_t> &);

    private:
        static RpcHandler *gShared;

        /// handle of the task handler port
        uintptr_t portHandle;

        /// when set, the worker thread will continue executing
        std::atomic_bool run;
        /// the worker thread
        std::unique_ptr<std::thread> worker;
};
}

#endif
