#ifndef LIBPCI_USERCLIENT_TYPES_H
#define LIBPCI_USERCLIENT_TYPES_H

#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace libpci {
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

}

namespace rpc {
void serialize(std::span<std::byte> &, const libpci::BusAddress &);
void deserialize(const std::span<std::byte> &, libpci::BusAddress &);
size_t bytesFor(const libpci::BusAddress &);
}
#endif
