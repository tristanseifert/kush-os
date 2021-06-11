#ifndef LIBDRIVER_RPC_RPCCLIENT_H
#define LIBDRIVER_RPC_RPCCLIENT_H

#include "Client_Driverman.hpp"

#include <mutex>
#include <string_view>

namespace libdriver {
/**
 * Implements the interface to the driver manager
 */
class RpcClient: public rpc::DrivermanClient {
    /// Port name under which the driver manager is registered
    constexpr static const std::string_view kPortName{"me.blraaz.rpc.driverman"};

    public:
        /// Returns the RPC client connection shared by the program
        static RpcClient *the();

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
