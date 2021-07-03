#pragma once

#include <cstddef>
#include <cstdint>
#include <string_view>

struct hashmap;
struct MessageHeader;
struct DyldosrvMapSegmentRequest;

class Library;

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
        /// Entry in the hash map
        struct HashmapEntry {
            /// copy of the library's path
            const char *path{nullptr};
            /// number of characters in the library's path
            size_t pathLen{0};
            /// actual library object
            Library *library;
        };

        static uint64_t HashLib(const void *_item, uint64_t s0, uint64_t s1);
        static int CompareLib(const void *a, const void *b, void *udata);

    private:
        void handleMapSegment(const struct MessageHeader &);
        int loadSegment(const struct MessageHeader &, const DyldosrvMapSegmentRequest &, uintptr_t &);
        int mapSegment(const struct MessageHeader &, const DyldosrvMapSegmentRequest &, const uintptr_t);
        void storeInfo(const DyldosrvMapSegmentRequest &, const uintptr_t);
        void reply(const struct MessageHeader &, const DyldosrvMapSegmentRequest &, const int err);
        void reply(const struct MessageHeader &, const DyldosrvMapSegmentRequest &, const uintptr_t vmRegion);

    private:
        /// Message receive buffer
        void *rxBuf{nullptr};
        /// Port handle for the receive port
        uintptr_t port{0};

        /// Reply buffer
        void *txBuf{nullptr};

        /// mapping of all loaded libraries
        hashmap *libraries{nullptr};
};
