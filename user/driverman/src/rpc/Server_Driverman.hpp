/*
 * This RPC server stub was autogenerated by idlc (version 8a02fc5d). DO NOT EDIT!
 * Generated from Driverman.idl for interface Driverman at 2021-06-26T17:21:35-0500
 *
 * You should subclass this implementation and define the required abstract methods to complete
 * implementing the interface. Note that there are several helper methods available to simplify
 * this task, or to retrieve more information about the caller.
 *
 * See the full RPC documentation for more details.
 */
#ifndef RPC_SERVER_GENERATED_11260871874244005202
#define RPC_SERVER_GENERATED_11260871874244005202

#include <string>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

namespace rpc {
namespace rt { class ServerRpcIoStream; }
class DrivermanServer {
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

    constexpr static const std::string_view kServiceName{"Driverman"};

    protected:
        using IoStream = rt::ServerRpcIoStream;
        // Return types for method 'GetDeviceProperty'
        struct GetDevicePropertyReturn {
            int32_t status;
            std::vector<std::byte> data;
        };

    public:
        DrivermanServer(const std::shared_ptr<IoStream> &stream);
        virtual ~DrivermanServer();

        // Server's main loop; continuously read and handle messages.
        bool run(const bool block = true);
        // Process a single message.
        bool runOne(const bool block);

    // These are methods the implementation provides to complete implementation of the interface
    protected:
        virtual std::string implAddDevice(const std::string &parent, const std::string &driverId) = 0;
        virtual int32_t implSetDeviceProperty(const std::string &path, const std::string &key, const std::vector<std::byte> &data) = 0;
        virtual GetDevicePropertyReturn implGetDeviceProperty(const std::string &path, const std::string &key) = 0;
        virtual int32_t implStartDevice(const std::string &path) = 0;
        virtual int32_t implStopDevice(const std::string &path) = 0;
        virtual int32_t implNotify(const std::string &path, uint64_t key) = 0;

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

        void _marshallAddDevice(const MessageHeader &, const std::span<std::byte> &payload);
        void _marshallSetDeviceProperty(const MessageHeader &, const std::span<std::byte> &payload);
        void _marshallGetDeviceProperty(const MessageHeader &, const std::span<std::byte> &payload);
        void _marshallStartDevice(const MessageHeader &, const std::span<std::byte> &payload);
        void _marshallStopDevice(const MessageHeader &, const std::span<std::byte> &payload);
        void _marshallNotify(const MessageHeader &, const std::span<std::byte> &payload);
}; // class DrivermanServer
} // namespace rpc
#endif // defined(RPC_SERVER_GENERATED_11260871874244005202)
