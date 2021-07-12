#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "Server_WindowServer.hpp"

class Compositor;

/**
 * Provides the window server's RPC interface, which applications use to create windows on screen.
 */
class RpcServer: public rpc::WindowServerServer {
    public:
        /// Name under which the RPC port is registered
        constexpr static const std::string_view kPortName{"me.blraaz.rpc.windowserver"};

    public:
        /// Initializes the RPC server with the given compositor.
        RpcServer(const std::shared_ptr<Compositor> &comp);
        /// Cleans up the RPC server.
        virtual ~RpcServer() = default;

        /// Registers a compositor, which is responsible for a particular display.
        void addCompositor(const std::shared_ptr<Compositor> &comp);

    protected:
        void implSubmitKeyEvent(uint32_t scancode, bool release) override;
        void implSubmitMouseEvent(uint32_t buttons, int32_t dX, int32_t dY, int32_t dZ) override;

    private:
        /// All active compositors
        std::vector<std::shared_ptr<Compositor>> comps;
};
