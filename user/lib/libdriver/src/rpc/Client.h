#ifndef LIBDRIVER_RPC_RPCCLIENT_H
#define LIBDRIVER_RPC_RPCCLIENT_H

#include "Client_Driverman.hpp"

#include <cstddef>
#include <mutex>
#include <span>
#include <string_view>
#include <vector>

namespace libdriver {
/**
 * Implements the interface to the driver manager
 */
class RpcClient: public rpc::DrivermanClient {
    using ByteSpan = std::span<std::byte>;
    using ByteVec = std::vector<std::byte>;

    /// Port name under which the driver manager is registered
    constexpr static const std::string_view kPortName{"me.blraaz.rpc.driverman"};

    public:
        /// Driverman specific notification keys
        enum class NoteKeys: uint64_t {
            /// Root filesystem has updated
            RootFsUpdated               = 0xA7CF01CC6579CB8EU
        };

    public:
        using rpc::DrivermanClient::SetDeviceProperty;
        using rpc::DrivermanClient::GetDeviceProperty;

        /// Returns the RPC client connection shared by the program
        static RpcClient *the();

        virtual void SetDeviceProperty(const std::string_view &path, const std::string_view &key,
                const ByteVec &data);
        virtual ByteVec GetDeviceProperty(const std::string_view &path,
                const std::string_view &key);

        /// Sends a notification to the driver manager.
        virtual int32_t NotifyDriverman(const NoteKeys key) {
            return this->Notify("", static_cast<uint64_t>(key));
        }

    private:
        RpcClient(const std::shared_ptr<IoStream> &io) : DrivermanClient(io) {}

    private:
        /// once token to ensure the client is only instantiated once
        static std::once_flag gInitFlag;
        /// shared instance of the client
        static RpcClient *gShared;
};
}

#endif
