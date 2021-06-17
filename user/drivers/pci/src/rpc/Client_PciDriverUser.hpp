/*
 * This RPC client stub was autogenerated by idlc. DO NOT EDIT!
 * Generated from UserClient.idl for interface PciDriverUser at 2021-06-17T13:20:46-0500
 *
 * You may use these generated stubs directly as the RPC interface, or you can subclass it to
 * override the behavior of the function calls, or to perform some preprocessing to the data as
 * needed before sending it.
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
#include <libpci/UserClientTypes.h>
#undef RPC_USER_TYPES_INCLUDES

namespace rpc {
namespace rt { class ClientRpcIoStream; }
class PciDriverUserClient {
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
        using IoStream = rt::ClientRpcIoStream;

    public:
        PciDriverUserClient(const std::shared_ptr<IoStream> &stream);
        virtual ~PciDriverUserClient();

        virtual std::string GetDeviceAt(const libpci::BusAddress &address);
        virtual uint32_t ReadCfgSpace32(const libpci::BusAddress &address, uint16_t offset);
        virtual void WriteCfgSpace32(const libpci::BusAddress &address, uint16_t offset, uint32_t value);

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

        uint32_t nextTag{0};

        void _ensureTxBuf(const size_t);
        uint32_t _sendRequest(const uint64_t type, const std::span<std::byte> &);
}; // class PciDriverUserClient
} // namespace rpc
