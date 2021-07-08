#include "DisplayClient.h"
#include "Helpers.h"

#include <mpack/mpack.h>
#include <driver/DrivermanClient.h>
#include <rpc/rt/ClientPortRpcStream.h>

using namespace DriverSupport::gfx;

/**
 * Allocates a graphics driver client with the given forest path. It will read out the connection
 * info and establish the RPC connection.
 */
int Display::Alloc(const std::string_view &path, std::shared_ptr<Display> &outPtr) {
    // get the RPC connection info property of this device
    auto driverman = libdriver::RpcClient::the();

    auto value = driverman->GetDeviceProperty(path, kConnectionPropertyName);
    if(value.empty()) {
        return Errors::InvalidPath;
    }

    // extract from it the port handle and display id
    const auto [port, dispId] = DecodeConnectionInfo(value);
    if(!port) {
        return Errors::InvalidConnectionInfo;
    }

    auto io = std::make_shared<rpc::rt::ClientPortRpcStream>(port);
    std::shared_ptr<Display> ptr(new Display(path, dispId, io));

    if(ptr->status) {
        return ptr->status;
    }

    outPtr = ptr;
    return 0;
}

/**
 * Initializes a driver client.
 */
Display::Display(const std::string_view &path, const uint32_t _displayId,
        const std::shared_ptr<IoStream> &io) : rpc::DisplayClient(io), forestPath(path),
        displayId(_displayId) {

}

/**
 * Performs cleanup of any allocated memory regions.
 */
Display::~Display() {
    // TODO: implement
}
