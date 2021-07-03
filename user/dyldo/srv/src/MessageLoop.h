#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

struct MessageHeader;

/**
 * Main run loop for the dynamic linker server
 */
class MessageLoop {
    /// Name under which the RPC port is advertised
    constexpr static const std::string_view kPortName{"me.blraaz.rpc.dyldosrv"};
    /// Maximum receive message size
    constexpr static const size_t kMaxMsgLen{2048};

    public:
        MessageLoop();
        ~MessageLoop();

        /// Enters the run loop processing messages until shutdown message is received
        void run();

    private:
        void handleMapSegment(struct MessageHeader *);

    private:
        /// Message receive buffer
        void *rxBuf{nullptr};

        /// Port handle for the receive port
        uintptr_t port{0};
};
