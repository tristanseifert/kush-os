#ifndef PCISRV_BUS_PCIE_DEVICE_H
#define PCISRV_BUS_PCIE_DEVICE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <memory>
#include <vector>

#include "bus/PciConfig.h"

class PciExpressBus;

namespace pcie {
/**
 * Encapsulates a single PCIe device.
 *
 * For multifunction devices, each function is advertised as its own device.
 */
class Device {
    using BusPtr = std::shared_ptr<PciExpressBus>;
    using DeviceAddress = PciConfig::DeviceAddress;

    constexpr static const std::string_view kPciAddressPropertyName{"pcie.address"};

    public:
        Device(const BusPtr &bus, const DeviceAddress &address);
        virtual ~Device();

    private:
        void serializeAuxData(std::vector<std::byte> &);

    private:
        /// the bus that this device is on
        std::weak_ptr<PciExpressBus> bus;
        /// device address
        DeviceAddress address;

        /// forest path of this device
        std::string path;
};
}

#endif
