#pragma once

#include "Server_Driverman.hpp"

#include <memory>
#include <string_view>

class RpcServer: public rpc::DrivermanServer {
    public:
        /// Initialize the global RPC server instance
        static void init();
        /// Return the global shared instance
        static inline auto the() {
            return gShared;
        }

        std::string implAddDevice(const std::string &parent, const std::string &driverId) override;
        void implSetDeviceProperty(const std::string &path, const std::string &key, const std::span<std::byte> &data) override;
        std::vector<std::byte> implGetDeviceProperty(const std::string &path, const std::string &key) override;

    private:
        /// Sets up the RPC server with the given IO stream.
        RpcServer(const std::shared_ptr<IoStream> &strm) : DrivermanServer(strm) {};

    private:
        /// Name to register the RPC service under
        constexpr static const std::string_view kRpcEndpointName{"me.blraaz.rpc.driverman"};

        /// shared RPC server instance
        static RpcServer *gShared;
};
