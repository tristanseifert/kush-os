#ifndef PCISRV_BUS_PCIE_PCIEXPRESSBUS_H
#define PCISRV_BUS_PCIE_PCIEXPRESSBUS_H

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>

#include <libpci/UserClientTypes.h>

#include "ConfigSpaceReader.h"

namespace pcie { class Device; }

/**
 * Represents a PCI Express bus in the system.
 */
class PciExpressBus: public std::enable_shared_from_this<PciExpressBus> {
    friend class pcie::ConfigSpaceReader;

    /// Key under which the ECAM information is stored
    constexpr static const std::string_view kEcamPropertyKey{"pcie.resources"};

    using DevicePtr = std::shared_ptr<pcie::Device>;
    using DeviceAddr = libpci::BusAddress;

    public:
        PciExpressBus(const std::string_view &forestPath);
        ~PciExpressBus();

        /// Test whether the given device address lies on this bus.
        constexpr bool containsDevice(const DeviceAddr &a) const {
            if(a.segment != this->segment) return false;
            else if(a.bus < this->busses.first && a.bus > this->busses.second) return false;
            return true;
        }
        /// Test whether we've scanned and found a device at the given address.
        constexpr bool hasDevice(const DeviceAddr &a) const {
            return this->devices.contains(a);
        }
        /// Returns the device object at the given address.
        inline auto getDevice(const DeviceAddr &a) const {
            return this->devices.at(a);
        }


        /// Get the path of this bus object.
        constexpr const std::string &getForestPath() const {
            return this->forestPath;
        }
        /// Returns the PCI config space reader object for this bus.
        PciConfig *getConfigIo() {
            return this->cfgReader;
        }

        /// Scan all devices on the bus.
        void scan();
        /// Return a reference to all found devices.
        constexpr const auto &getDevices() const {
            return this->devices;
        }

        /// Gets the range of bus numbers this bus controls
        constexpr auto getBusRange() const {
            return this->busses;
        }
        /// Gets the segment number of this bus.
        constexpr auto getSegment() const {
            return this->segment;
        }

    private:
        void decodeEcamInfo(const std::span<std::byte> &);
        void probeDevice(const DeviceAddr &);
        void createDeviceIfNeeded(const DeviceAddr &);

    private:
        static const uintptr_t kEcamMappingRange[2];

        /// Forest node for this bus
        std::string forestPath;

        /// PCIe segment controlled by this bus
        uint16_t segment{0};
        /// Range of busses numbers that segment this bridge controls
        std::pair<uint8_t, uint8_t> busses{0, 255};

        /// Physical base address of the configuration aperture
        uint64_t ecamPhysBase{0};
        /// VM region handle for this bus' ECAM region
        uintptr_t ecamRegionVmHandle{0};
        /// Base address to this bus' ECAM region in virtual address space
        void *ecamRegion{nullptr};

        /// Config space IO machine
        pcie::ConfigSpaceReader *cfgReader{nullptr};

        /// All devices we've found on the bus
        std::unordered_map<DeviceAddr, DevicePtr> devices;
};

#endif
