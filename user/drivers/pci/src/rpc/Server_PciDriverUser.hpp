/*
 * This RPC server stub was autogenerated by idlc. DO NOT EDIT!
 * Generated from UserClient.idl for interface PciDriverUser at 2021-06-15T01:48:49-0500
 *
 * You should subclass this implementation and define the required abstract methods to complete
 * implementing the interface. Note that there are several helper methods available to simplify
 * this task, or to retrieve more information about the caller.
 *
 * See the full RPC documentation for more details.
 */
#pragma once

#include <string>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

#define RPC_USER_TYPES_INCLUDES
#include <driver/PciUserClientTypes.h>
#undef RPC_USER_TYPES_INCLUDES

namespace rpc {
namespace rt { class ServerRpcIoStream; }
class PciDriverUserServer {
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

    protected:
        using IoStream = rt::ServerRpcIoStream;

    public:
        PciDriverUserServer(const std::shared_ptr<IoStream> &stream);
        virtual ~PciDriverUserServer();

        // Server's main loop; continuously read and handle messages.
        bool run(const bool block = true);
        // Process a single message.
        bool runOne(const bool block);

    // These are methods the implementation provides to complete implementation of the interface
    protected:
        virtual std::string implGetDeviceAt(const libdriver::pci::BusAddress &address) = 0;
        virtual uint32_t implReadCfgSpace32(const libdriver::pci::BusAddress &address, uint16_t offset) = 0;
        virtual void implWriteCfgSpace32(const libdriver::pci::BusAddress &address, uint16_t offset, uint32_t value) = 0;

    // Helpers provided to subclasses for implementation of interface methods
    protected:
        constexpr inline auto &getIo() {
            return this->io;
        }

    // Implementation details; pretend this does not exist
    private:
        std::shared_ptr<IoStream> io;
        size_t txBufSize{0};
        void *txBuf{nullptr};

        void _ensureTxBuf(const size_t);
        void _doSendReply(const MessageHeader &, const std::span<std::byte> &);

        void _marshallGetDeviceAt(const MessageHeader &, const std::span<std::byte> &payload);
        void _marshallReadCfgSpace32(const MessageHeader &, const std::span<std::byte> &payload);
        void _marshallWriteCfgSpace32(const MessageHeader &, const std::span<std::byte> &payload);
}; // class PciDriverUserServer
} // namespace rpc
