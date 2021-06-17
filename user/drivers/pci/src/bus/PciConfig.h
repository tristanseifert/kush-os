#ifndef BUS_PCICONFIG_H
#define BUS_PCICONFIG_H

#include <compare>
#include <cstdint>
#include <cstddef>

#include <libpci/UserClientTypes.h>
#include <fmt/format.h>

/**
 * Base interface for all PCI configuration space access methods. 
 */
class PciConfig {
    using DeviceAddress = libpci::BusAddress;

    public:
        /// Width of a configuration space access, in bits
        enum class Width {
            Byte                        = 8,
            Word                        = 16,
            DWord                       = 32,
            QWord                       = 64,
        };

    public:
        virtual ~PciConfig() = default;

        /**
         * Reads a register from the device's configuration space at a given offset.
         *
         * @param device Bus address of the device
         * @param reg Register offset (in bytes) into its configuration space
         * @param width Width of the read, in bits
         *
         * @return Value read from the register
         */
        virtual uint64_t read(const DeviceAddress &device, const size_t reg, const Width width) = 0;

        /**
         * Writes a register in a device's configuration space at a particular offset.
         *
         * @param device Bus address of the device
         * @param reg Register offset (in bytes) into its configuration space
         * @param width Width of the write, in bits
         * @param value Data to write to the register
         */
        virtual void write(const DeviceAddress &device, const size_t reg, const Width width,
                const uint64_t value) = 0;
};

namespace std {
template <> struct hash<libpci::BusAddress> {
    std::size_t operator()(const libpci::BusAddress &da) const {
        uint64_t temp{da.segment};
        temp |= (static_cast<uint64_t>(da.bus) << 16);
        temp |= (static_cast<uint64_t>(da.device) << 24);
        temp |= (static_cast<uint64_t>(da.function) << 32);
        return std::hash<uint64_t>()(temp);
    }
};
}

template <>
struct fmt::formatter<libpci::BusAddress> {
    constexpr auto parse(format_parse_context &ctx) { return ctx.begin(); }

    template <typename FormatContext>
    auto format(const libpci::BusAddress &a, FormatContext &ctx) {
        return format_to(ctx.out(), "{:04x}:{:02x}:{:02x}:{:02x}", a.segment, a.bus, a.device,
                a.function);
    }
};

#endif
