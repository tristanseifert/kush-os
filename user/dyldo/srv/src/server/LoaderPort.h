#ifndef SERVER_LOADERPORT_H
#define SERVER_LOADERPORT_H

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <memory>
#include <thread>

#include <rpc/RpcPacket.hpp>
#include "rpc/LoaderPort.h"

namespace server {
/**
 * Provides an RPC interface used primarily by the program loader to bootstrap newly create tasks
 * that need the support of the dynamic runtime.
 */
class LoaderPort {
    private:
        /// maximum message size that can be received (minus MessageHeader)
        constexpr static const size_t kMsgBufLen = 4096;
        /// name under which we register the port
        const static std::string kPortName;

    public:
        static void init();

    private:
        LoaderPort();

        void main();
        void handleTaskCreated(const struct MessageHeader *, const rpc::RpcPacket *);

        void reply(const rpc::RpcPacket *, const rpc::DyldoLoaderEpType,
                const std::span<uint8_t> &);

    private:
        static LoaderPort *gShared;

    private:
        /// Port handle to receive requests on
        uintptr_t port;

        /// when set, the worker will process requests
        std::atomic_bool run;
        /// worker thread
        std::unique_ptr<std::thread> worker;
};
}

#endif
