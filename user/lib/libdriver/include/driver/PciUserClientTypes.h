#ifndef LIBDRIVER_RPC_PCIUSERCLIENT_TYPES_H
#define LIBDRIVER_RPC_PCIUSERCLIENT_TYPES_H

#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace libdriver::pci {
/**
 * Represents the address of a device on the PCI bus.
 */
struct BusAddress {
    /// Bus segment; this should always be 0 for legacy PCI.
    uint16_t segment{0};
    uint8_t bus{0};
    uint8_t device{0};
    uint8_t function{0};

    /// Create an empty device address.
    BusAddress() = default;
    /// Create a device address with the given segment, bus, device and function.
    BusAddress(const uint16_t _segment, const uint8_t _bus, const uint8_t _device,
            const uint8_t _function = 0) : segment(_segment), bus(_bus), device(_device),
            function(_function) {};
    /// Get the device address of a device's alternate function.
    BusAddress(const BusAddress &da, const uint8_t _function) : segment(da.segment),
        bus(da.bus), device(da.device), function(_function) {}

    inline auto operator<=>(const BusAddress &) const = default;
};

/**
 * Object representing a PCI device.
 */
class Device {
    constexpr static const std::string_view kPciExpressInfoPropertyName{"pcie.info"};


    public:
        Device() = delete;
        /// Creates a new device at the given bus address
        Device(const BusAddress &addr);
        /// Creates a new device from the given forest path
        Device(const std::string_view &path);

        virtual ~Device() = default;

        /// Returns the path to this device in the forest
        constexpr const auto &getPath() const {
            return this->path;
        }
        /// Returns this device's address.
        constexpr const auto &getAddress() const {
            return this->address;
        }

        /// Returns the device's vendor ID
        constexpr const auto getVid() const {
            return this->vid;
        }
        /// Return the device's product ID
        constexpr const auto getPid() const {
            return this->pid;
        }
        /// Return the device's class ID
        constexpr const auto getClassId() const {
            return this->classId;
        }
        /// Return the device's subclass ID
        constexpr const auto getSubclassId() const {
            return this->subclassId;
        }

        /// Writes a 32-bit value to the device's config space.
        void writeCfg32(const size_t index, const uint32_t value);

        /// Reads a 32-bit value from the device's config space.
        uint32_t readCfg32(const size_t index) const;
        /// Reads a 16-bit value from the device's config space.
        inline uint16_t readCfg16(const size_t index) const {
            const auto temp = this->readCfg32(index & ~0x3);
            if(index & 0x2) {
                return (temp & 0xFFFF0000) >> 16;
            } else {
                return (temp & 0xFFFF);
            }
        }
        /// Reads a 8-bit value from the device's config space.
        inline uint8_t readCfg8(const size_t index) const {
            const auto temp = this->readCfg16(index & ~0x1);
            if(index & 0x1) {
                return (temp & 0xFF00) >> 8;
            } else {
                return (temp & 0xFF);
            }
        }

    private:
        void probeConfigSpace();

    private:
        /// Forest path of this device
        std::string path;
        /// Bus address of this device
        BusAddress address;

        /// vendor and product ids
        uint16_t vid{0}, pid{0};
        /// Class and subclass
        uint8_t classId{0}, subclassId{0};
};
}

namespace rpc {
void serialize(std::vector<std::byte> &, const libdriver::pci::BusAddress &);
void deserialize(const std::span<std::byte> &, libdriver::pci::BusAddress &);
}
#endif
