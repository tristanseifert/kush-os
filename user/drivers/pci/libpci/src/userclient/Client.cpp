#include "Client.h"

#include <cstring>

#include <rpc/rt/ClientPortRpcStream.h>

using namespace libpci;

UserClient *UserClient::gShared{nullptr};
std::once_flag UserClient::gInitFlag;

/**
 * Initializes the shared RPC client instance if needed, and then returns pointer to the shared
 * instance.
 */
UserClient *UserClient::the() {
    std::call_once(gInitFlag, []() {
        auto io = std::make_shared<rpc::rt::ClientPortRpcStream>(kPortName);
        gShared = new UserClient(io);
    });

    return gShared;
}



/**
 * Serializes a device address; this just packs the address values sequentially.
 */
bool rpc::serialize(std::span<std::byte> &outData, const BusAddress &addr) {
    if(outData.size() < 5) return false;
    auto bytes = outData.data();

    // copy the variables
    memcpy(&bytes[0], &addr.segment, sizeof(addr.segment));
    memcpy(&bytes[2], &addr.bus, sizeof(addr.bus));
    memcpy(&bytes[3], &addr.device, sizeof(addr.device));
    memcpy(&bytes[4], &addr.function, sizeof(addr.function));

    return true;
}
/**
 * Deserializes a device address; this unpacks the address values that were packed sequentially
 * after one another.
 */
bool rpc::deserialize(const std::span<std::byte> &bytes, BusAddress &addr) {
    if(bytes.size() < 5) return false;

    memcpy(&addr.segment, &bytes[0], sizeof(addr.segment));
    memcpy(&addr.bus, &bytes[2], sizeof(addr.bus));
    memcpy(&addr.device, &bytes[3], sizeof(addr.device));
    memcpy(&addr.function, &bytes[4], sizeof(addr.function));

    return true;
}

/**
 * The PCI bus address is always packed into a five byte long structure.
 */
size_t rpc::bytesFor(const libpci::BusAddress &) {
    return 5;
}

