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

/**
 * Allows setting keys with a string_view instead of a string.
 */
void RpcClient::SetDeviceProperty(const std::string_view &_path, const std::string_view &_key,
        const std::span<std::byte> &data) {
    const std::string path(_path), key(_key);
    this->SetDeviceProperty(path, key, data);
}
/**
 * Allows getting keys with a string_view instead of a string.
 */
RpcClient::ByteVec RpcClient::GetDeviceProperty(const std::string_view &_path,
        const std::string_view &_key) {
    const std::string path(_path), key(_key);
    return this->GetDeviceProperty(path, key);
}

