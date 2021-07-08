#pragma once

#include <cstddef>
#include <vector>

#include <DriverSupport/gfx/Server_Display.hpp>

class SVGA;

namespace svga {
/**
 * Provides the RPC server interface for the SVGA driver.
 */
class RpcServer: public rpc::DisplayServer {
    public:
        RpcServer(::SVGA *gpu, const uintptr_t port);
        virtual ~RpcServer();

        /// Encodes the connection info for this server.
        void encodeInfo(std::vector<std::byte> &outInfo);

        GetDeviceCapabilitiesReturn implGetDeviceCapabilities() override;
        int32_t implSetOutputEnabled(bool enabled) override;
        int32_t implSetOutputMode(const DriverSupport::gfx::DisplayMode &mode) override;
        int32_t implRegionUpdated(int32_t x, int32_t y, uint32_t w, uint32_t h) override;
        GetFramebufferReturn implGetFramebuffer() override;
        GetFramebufferInfoReturn implGetFramebufferInfo() override;

    private:
        /// Graphics device to perform operations on
        SVGA *s;
        /// Port handle to listen to connections on
        uintptr_t port{0};
};
}
