#include "DisplayClient.h"
#include "Helpers.h"

#include <cstdint>
#include <cstdio>

#include <driver/DrivermanClient.h>
#include <rpc/rt/ClientPortRpcStream.h>
#include <sys/syscalls.h>

using namespace DriverSupport::gfx;

/// Region of virtual memory in which framebuffers are mapped
static uintptr_t kPrivateMappingRange[2] = {
    // start
    0x110B0000000,
    // end
    0x110D0000000,
};

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
    this->mapFramebuffer();
}

/**
 * Performs cleanup of any allocated memory regions.
 */
Display::~Display() {
    int err;

    // unmap framebuffer region
    if(this->framebufferRegion) {
        err = UnmapVirtualRegion(this->framebufferRegion);
        if(err) {
            fprintf(stderr, "[%s] %s failed: %d\n", "DriverSupport::gfx", __PRETTY_FUNCTION__, err);
        }
    }
}



/**
 * Gets information about the framebuffer vm region and attempts to map it.
 */
int Display::mapFramebuffer() {
    int err;
    uintptr_t base;

    // get info
    const auto info = this->GetFramebuffer();
    if(info.status) return info.status;

    // map it
    const auto size = info.size;

    err = MapVirtualRegionRange(info.handle, kPrivateMappingRange, size, 0, &base);
    kPrivateMappingRange[0] += size;
    if(err) {
        fprintf(stderr, "[%s] %s failed: %d\n", "DriverSupport::gfx", "MapVirtualRegion", err);
        return err;
    }

    this->framebufferRegion = info.handle;
    this->framebufferBytes = size;
    this->framebuffer = reinterpret_cast<void *>(base);

    // success!
    return 0;
}
