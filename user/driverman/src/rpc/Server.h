#pragma once

#include "Server_Driverman.hpp"

#include <memory>
#include <string_view>

class RpcServer: public rpc::DrivermanServer {
    public:
        enum Status: int32_t {
            /// The request completed successfully
            Success                     = 0,
            /// There is no device at this path
            NoDevice                    = -1,
        };

        /// Initialize the global RPC server instance
        static void init();
        /// Return the global shared instance
        static inline auto the() {
            return gShared;
        }

        std::string implAddDevice(const std::string &parent, const std::string &driverId) override;
        void implSetDeviceProperty(const std::string &path, const std::string &key, const std::span<std::byte> &data) override;
        std::vector<std::byte> implGetDeviceProperty(const std::string &path, const std::string &key) override;

        int32_t implStartDevice(const std::string &path) override;
        int32_t implStopDevice(const std::string &path) override;

        int32_t implNotify(const std::string &path, uint64_t key) override;

    private:
        /// Sets up the RPC server with the given IO stream.
        RpcServer(const std::shared_ptr<IoStream> &strm) : DrivermanServer(strm) {};

    private:
        /// Name to register the RPC service under
        constexpr static const std::string_view kRpcEndpointName{"me.blraaz.rpc.driverman"};
        /// whether property set/gets are logged
        constexpr static const bool kLogProperties{false};

        /// shared RPC server instance
        static RpcServer *gShared;
};
