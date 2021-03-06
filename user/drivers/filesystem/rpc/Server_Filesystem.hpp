/*
 * This RPC server stub was autogenerated by idlc (version 8a02fc5d). DO NOT EDIT!
 * Generated from Filesystem.idl for interface Filesystem at 2021-06-26T17:21:48-0500
 *
 * You should subclass this implementation and define the required abstract methods to complete
 * implementing the interface. Note that there are several helper methods available to simplify
 * this task, or to retrieve more information about the caller.
 *
 * See the full RPC documentation for more details.
 */
#ifndef RPC_SERVER_GENERATED_18393244566832765147
#define RPC_SERVER_GENERATED_18393244566832765147

#include <string>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace rpc {
namespace rt { class ServerRpcIoStream; }
class FilesystemServer {
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

    constexpr static const std::string_view kServiceName{"Filesystem"};

    protected:
        using IoStream = rt::ServerRpcIoStream;
        // Return types for method 'OpenFile'
        struct OpenFileReturn {
            int32_t status;
            uint64_t handle;
            uint64_t fileSize;
        };
        // Return types for method 'SlowRead'
        struct SlowReadReturn {
            int32_t status;
            std::vector<std::byte> data;
        };

    public:
        FilesystemServer(const std::shared_ptr<IoStream> &stream);
        virtual ~FilesystemServer();

        // Server's main loop; continuously read and handle messages.
        bool run(const bool block = true);
        // Process a single message.
        bool runOne(const bool block);

    // These are methods the implementation provides to complete implementation of the interface
    protected:
        virtual OpenFileReturn implOpenFile(const std::string &path, uint32_t mode) = 0;
        virtual SlowReadReturn implSlowRead(uint64_t handle, uint64_t offset, uint16_t numBytes) = 0;
        virtual int32_t implCloseFile(uint64_t handle) = 0;

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

        void _marshallOpenFile(const MessageHeader &, const std::span<std::byte> &payload);
        void _marshallSlowRead(const MessageHeader &, const std::span<std::byte> &payload);
        void _marshallCloseFile(const MessageHeader &, const std::span<std::byte> &payload);
}; // class FilesystemServer
} // namespace rpc
#endif // defined(RPC_SERVER_GENERATED_18393244566832765147)
