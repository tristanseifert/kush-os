#pragma once

#include <cstdint>
#include <tuple>
#include <memory>
#include <string_view>

namespace rpc {
class WindowServerClient;
}

/**
 * Generated events from mice and keyboard are processed via this class and sent to the window
 * server which then handles them appropriately.
 */
class EventSubmitter {
    /// Name under which the RPC port for the window server is registered
    constexpr static const std::string_view kWindowServerPortName{"me.blraaz.rpc.windowserver"};

    public:
        /// Return the shared instance event submitter object
        static EventSubmitter *the();

        /// A keyboard event has been generated
        void submitKeyEvent(const uint32_t key, const bool isMake);
        /// A mouse event has been generated
        void submitMouseEvent(const uintptr_t buttons, const std::tuple<int, int, int> &delta);

    private:
        /// Attempts to establish the RPC connection
        bool connect();

    private:
        static EventSubmitter *gShared;

        /// RPC connection handler
        std::unique_ptr<rpc::WindowServerClient> rpc;
};
