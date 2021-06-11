#include "Client.h"

#include <rpc/rt/ClientPortRpcStream.h>

using namespace libdriver;

RpcClient *RpcClient::gShared{nullptr};
std::once_flag RpcClient::gInitFlag;

/**
 * Initializes the shared RPC client instance if needed, and then returns pointer to the shared
 * instance.
 */
RpcClient *RpcClient::the() {
    std::call_once(gInitFlag, []() {
        auto io = std::make_shared<rpc::rt::ClientPortRpcStream>(kPortName);
        gShared = new RpcClient(io);
    });

    return gShared;
}
