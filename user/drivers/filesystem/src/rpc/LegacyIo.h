#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <thread>

struct MessageHeader;

namespace rpc {
struct RpcPacket;
enum class FileIoEpType: uint32_t;
}

class MessageLoop;

/**
 * Handles running the legacy io service. This is used by early boot services and the dynamic
 * linker, when the full RPC framework isn't available yet. It supports read-only access to the
 * filesystem, and simply thunks through the RPC server's implementation.
 */
class LegacyIo {
    /// Legacy service provider name
    constexpr static const std::string_view kServiceName{"me.blraaz.rpc.fileio"};
    /// maximum length of messages to be received by the legacy handler; this includes headers
    constexpr static const size_t kMaxMsgLen{1024 * 16};
    /// maximum IO block size
    constexpr static const size_t kMaxBlockSize{4096 * 8};

    public:
        LegacyIo(MessageLoop *msg);
        ~LegacyIo();

    private:
        void main();

        void handleGetCaps(const struct MessageHeader *, const rpc::RpcPacket *, const size_t);
        void handleOpen(const struct MessageHeader *, const rpc::RpcPacket *, const size_t);
        void openFailed(const int, const rpc::RpcPacket *);
        void handleReadDirect(const struct MessageHeader *, const rpc::RpcPacket *, const size_t);
        void readFailed(const uintptr_t, const int, const rpc::RpcPacket *);

        void handleClose(const struct MessageHeader *, const rpc::RpcPacket *, const size_t);

        void reply(const rpc::RpcPacket *packet, const rpc::FileIoEpType type,
            const std::span<uint8_t> &buf);

        /// Ensures the read reply buffer is at least large enough to fit the given message.
        void ensureReadReplyBufferSize(const size_t payloadBytes);

    private:
        /// Message loop that implements the actual behaviors
        MessageLoop *ml;

        /// whether the legacy worker is running
        std::atomic_bool run{true};
        /// thread for the legacy worker
        std::unique_ptr<std::thread> worker;
        /// port handle for the legacy worker
        uintptr_t workerPort{0};

        /// Aligned buffer to assemble read request packets in
        void *readReplyBuffer{nullptr};
        /// Size of the read reply buffer, in bytes
        size_t readReplyBufferSize{0};
};
