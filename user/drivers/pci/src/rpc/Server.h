#ifndef PCIESRV_RPC_SERVER_H
#define PCIESRV_RPC_SERVER_H

#include "Server_PciDriverUser.hpp"

#include <driver/PciUserClientTypes.h>

#include <cstdint>
#include <memory>
#include <string_view>

class RpcServer: public rpc::PciDriverUserServer {
    public:
        /// Initialize the global RPC server instance
        static void init();
        /// Return the global shared instance
        static inline auto the() {
            return gShared;
        }

    public:
        std::string implGetDeviceAt(const libdriver::pci::BusAddress &address) override;
        uint32_t implReadCfgSpace32(const libdriver::pci::BusAddress &address, uint16_t offset) override;
        void implWriteCfgSpace32(const libdriver::pci::BusAddress &address, uint16_t offset,
                uint32_t value) override;

    private:
        /// Sets up the RPC server with the given IO stream.
        RpcServer(const std::shared_ptr<IoStream> &strm) : PciDriverUserServer(strm) {};

    private:
        /// Name to register the RPC service under
        constexpr static const std::string_view kRpcEndpointName{"me.blraaz.driver.pci"};

        /// shared RPC server instance
        static RpcServer *gShared;

};

#endif
