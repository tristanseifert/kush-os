#ifndef LIBDRIVER_RPC_PCICLIENT_H
#define LIBDRIVER_RPC_PCICLIENT_H

#include "Client_PciDriverUser.hpp"

#include <driver/PciUserClientTypes.h>
#include <capnp/serialize.h>

#include <cstddef>
#include <cstdint>
#include <mutex>
#include <memory>
#include <string_view>

namespace libdriver::pci {
/**
 * Provides an interface to the PCI bus driver, so that drivers that were instantiated
 * from a PCI device node can read the device config space, among other tasks.
 */
class UserClient: public rpc::PciDriverUserClient {
    /// Name of the PCI driver's run loop port
    constexpr static const std::string_view kPortName{"me.blraaz.driver.pci"};

    public:
        /// Returns the shared instance of the user client
        static UserClient *the();

    private:
        UserClient(const std::shared_ptr<IoStream> &io) : PciDriverUserClient(io) {}

    private:
        static std::once_flag gInitFlag;
        static UserClient *gShared;
};
}

#endif
