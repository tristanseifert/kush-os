#pragma once

#include <memory>
#include <string_view>
#include <vector>

class Compositor;

/**
 * Provides the window server's RPC interface, which applications use to create windows on screen.
 */
class RpcServer {
    public:
        /// Name under which the RPC port is registered
        constexpr static const std::string_view kPortName{"me.blraaz.rpc.windowserver"};

    public:
        /// Initializes the RPC server with the given compositor.
        RpcServer(const std::shared_ptr<Compositor> &comp) {
            this->addCompositor(comp);
        }

        /// Registers a compositor, which is responsible for a particular display.
        void addCompositor(const std::shared_ptr<Compositor> &comp);
        /// Executes the message processing loop.
        int run();

    private:
        /// All active compositors
        std::vector<std::shared_ptr<Compositor>> comps;
};
