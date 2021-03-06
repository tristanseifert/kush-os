/*
 * This RPC server stub was autogenerated by idlc (version c8601c6e). DO NOT EDIT!
 * Generated from WindowServer.idl for interface WindowServer at 2021-07-11T18:46:48-0500
 *
 * You should subclass this implementation and define the required abstract methods to complete
 * implementing the interface. Note that there are several helper methods available to simplify
 * this task, or to retrieve more information about the caller.
 *
 * See the full RPC documentation for more details.
 */
#ifndef RPC_SERVER_GENERATED_16174174863144938629
#define RPC_SERVER_GENERATED_16174174863144938629

#include <string>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace rpc {
namespace rt { class ServerRpcIoStream; }
class WindowServerServer {
    struct MessageHeader {
        enum Flags: uint32_t {
            Request                     = (1 << 0),
            Response                    = (1 << 1),
        };

        uint64_t type;
        Flags flags;
        uint32_t tag;

        std::byte payload[];
    };
    static_assert(!(offsetof(MessageHeader, payload) % sizeof(uintptr_t)),
        "message header's payload is not word aligned");

    constexpr static const std::string_view kServiceName{"WindowServer"};

    protected:
        using IoStream = rt::ServerRpcIoStream;

    public:
        WindowServerServer(const std::shared_ptr<IoStream> &stream);
        virtual ~WindowServerServer();

        // Server's main loop; continuously read and handle messages.
        bool run(const bool block = true);
        // Process a single message.
        bool runOne(const bool block);

    // These are methods the implementation provides to complete implementation of the interface
    protected:
        virtual void implSubmitKeyEvent(uint32_t scancode, bool release) = 0;
        virtual void implSubmitMouseEvent(uint32_t buttons, int32_t dX, int32_t dY, int32_t dZ) = 0;

    // Helpers provided to subclasses for implementation of interface methods
    protected:
        constexpr inline auto &getIo() {
            return this->io;
        }

        /// Handles errors occurring during server operations
        virtual void _HandleError(const bool fatal, const std::string_view &what);

    // Implementation details; pretend this does not exist
    private:
        std::shared_ptr<IoStream> io;
        size_t txBufSize{0};
        void *txBuf{nullptr};

        void _ensureTxBuf(const size_t);
        void _sendReply(const MessageHeader &, const size_t);

        void _marshallSubmitKeyEvent(const MessageHeader &, const std::span<std::byte> &payload);
        void _marshallSubmitMouseEvent(const MessageHeader &, const std::span<std::byte> &payload);
}; // class WindowServerServer
} // namespace rpc
#endif // defined(RPC_SERVER_GENERATED_16174174863144938629)
