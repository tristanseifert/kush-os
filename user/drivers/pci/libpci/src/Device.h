#ifndef LIBPCI_DEVICE_H
#define LIBPCI_DEVICE_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <libpci/UserClientTypes.h>

namespace libpci {
/**
 * Object representing a PCI device.
 */
class Device {
    constexpr static const std::string_view kPciExpressInfoPropertyName{"pcie.info"};

    public:
        /// Represents an entry in the PCI capability list.
        struct Capability {
            /// ID of the capability
            uint16_t id;
            /// Location of the capability in configuration space
            uint16_t offset;
            /// Version of the capability (or 0xFF if not specified)
            uint8_t version{0xFF};

            /// ID for the message signalled interrupts
            constexpr static const uint16_t kIdMsi{0x05};
            /// ID for the extended message signalled interrupts
            constexpr static const uint16_t kIdMsiX{0x11};
        };

        /**
         * Defines the names of the different base address registers (BARs) available on the
         * device. Note that less than the maximum 6 will be available if the device has a
         * header type that is not the default.
         */
        enum class BaseAddress: uint8_t {
            BAR0, BAR1, BAR2, BAR3, BAR4, BAR5
        };

        /**
         * Each of the memory regions exposed by the device is represented as one of these
         * structures, a small encapsulation around the BAR values the device provides.
         */
        struct AddressResource {
            friend class Device;

            enum class Type: uint8_t {
                Memory, IO
            };

            /// Resource type
            Type type;
            /// The original device BAR this resource was allocated from
            BaseAddress bar;
            /// If not an IO resource, the memory is prefetchable
            bool prefetchable{false};
            /// If not an IO resource, the memory is 64-bit addressable.
            bool supports64Bit{false};

            /// Base address of the resource
            uintptr_t base{0};
            /// Size of the resource, in bytes
            size_t length{0};

            private:
                /// Creates an address resource for an IO region
                AddressResource(const BaseAddress from, const uintptr_t _base,
                        const size_t _length) : type(Type::IO), bar(from), base(_base),
                        length(_length) {}
                /// Creates an address resource for a memory region
                AddressResource(const BaseAddress from, const uintptr_t _base,
                        const size_t _length, const bool _prefetch, const bool _is64Bit) :
                        type(Type::Memory), bar(from), prefetchable(_prefetch),
                        supports64Bit(_is64Bit), base(_base), length(_length) {}
        };

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
        /// Return the device's address resources
        constexpr const auto &getAddressResources() const {
            return this->bars;
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
        void readCapabilities();
        void readExtendedCapabilities();
        void readAddrRegions();

    private:
        /// Forest path of this device
        std::string path;
        /// Bus address of this device
        BusAddress address;

        /// vendor and product ids
        uint16_t vid{0}, pid{0};
        /// Class and subclass
        uint8_t classId{0}, subclassId{0};
        /// Header type
        uint8_t headerType{0};

        /// Capability list
        std::vector<Capability> capabilities;
        /// Address resource list
        std::vector<AddressResource> bars;
};
}

#endif
