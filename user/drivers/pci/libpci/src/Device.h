#ifndef LIBPCI_DEVICE_H
#define LIBPCI_DEVICE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

#include <libpci/UserClientTypes.h>

namespace libpci {
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

#endif
