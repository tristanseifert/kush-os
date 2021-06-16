#ifndef PCISRV_BUS_PCIE_DEVICE_H
#define PCISRV_BUS_PCIE_DEVICE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <memory>
#include <vector>

#include <driver/PciUserClientTypes.h>
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
    using DeviceAddress = libdriver::pci::BusAddress;

    constexpr static const std::string_view kPciAddressPropertyName{"pcie.info"};

    public:
        Device(const BusPtr &bus, const DeviceAddress &address);
        virtual ~Device();

        /// Get the path of this device in the forest.
        constexpr const std::string &getForestPath() const {
            return this->path;
        }

    private:
        void serializeAuxData(std::vector<std::byte> &, const uint16_t, const uint16_t,
                const uint8_t, const uint8_t);

    private:
        /// Whether the forest paths new devices are registered under is logged
        constexpr static const bool kLogPaths{false};

        /// the bus that this device is on
        std::weak_ptr<PciExpressBus> bus;
        /// device address
        DeviceAddress address;

        /// forest path of this device
        std::string path;
};
}

#endif
