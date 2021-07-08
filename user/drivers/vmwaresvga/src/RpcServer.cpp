#include "RpcServer.h"
#include "SVGA.h"
#include "Commands2D.h"

#include "Log.h"

#include <DriverSupport/gfx/Helpers.h>
#include <DriverSupport/gfx/Types.h>
#include <rpc/rt/ServerPortRpcStream.h>
#include <sys/syscalls.h>

using namespace svga;

/**
 * Allocates a new RPC server instance that will use the provided port to listen for requests.
 *
 * @param _port Port handle to listen on; we take ownership of this object.
 */
RpcServer::RpcServer(::SVGA *gpu, const uintptr_t _port) :
    rpc::DisplayServer(std::make_shared<rpc::rt::ServerPortRpcStream>(_port)), s(gpu),
    port(_port) {

}

/**
 * Destroy any outstanding mappings into device memory.
 */
RpcServer::~RpcServer() {
    // close the port
    PortDestroy(this->port);
}

/**
 * Encodes connection information.
 */
void RpcServer::encodeInfo(std::vector<std::byte> &outInfo) {
    auto success = DriverSupport::gfx::EncodeConnectionInfo(this->port, 0, outInfo);
    REQUIRE(success, "%s failed", __PRETTY_FUNCTION__);
}



/**
 * Returns the device capabilities. We only support the "update rect" capability right now.
 */
RpcServer::GetDeviceCapabilitiesReturn RpcServer::implGetDeviceCapabilities() {
    using Caps = DriverSupport::gfx::DisplayCapabilities;

    return {0, Caps::kUpdateRects};
}

/**
 * Sets the output state.
 */
int32_t RpcServer::implSetOutputEnabled(bool enabled) {
    if(enabled) {
        this->s->enable();
    } else {
        this->s->disable();
    }

    return 0;
}

/**
 * Sets the video mode for the display's output.
 */
int32_t RpcServer::implSetOutputMode(const DriverSupport::gfx::DisplayMode &mode) {
    uint8_t bpp;
    switch(mode.bpp) {
        case DriverSupport::gfx::DisplayMode::Bpp::Indexed8:
            bpp = 8;
            break;
        case DriverSupport::gfx::DisplayMode::Bpp::RGB24:
            bpp = 24;
            break;
        case DriverSupport::gfx::DisplayMode::Bpp::RGBA32:
            bpp = 32;
            break;
    }

    const auto [w, h] = mode.resolution;
    return this->s->setMode(w, h, bpp);
}

/**
 * Invalidates the provided region.
 */
int32_t RpcServer::implRegionUpdated(int32_t x, int32_t y, int32_t w, int32_t h) {
    auto &c2 = this->s->get2DCommands();
    return c2->update({x, y}, {w, h});
}

