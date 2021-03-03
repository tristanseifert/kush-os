#include "PciConfig.h"

#include "x86_io.h"

#include <sys/syscalls.h>
#include <sys/x86/syscalls.h>

#include <stdexcept>
#include <system_error>

PciConfig *PciConfig::gShared = nullptr;

/// IO port address for the configuration address
constexpr static const uint16_t kConfigAddress  = 0xCF8;
/// IO port address for the configuration data port
constexpr static const uint16_t kConfigData     = 0xCFC;

/**
 * Initializes the PCI config handler.
 */
PciConfig::PciConfig() {
    // whitelist IO port for x86
#if defined(__i386__)
    int err;

    // allow access to the PCI configuration registers
    static const uint8_t bitmap[] = {
        0xFF
    };

    err = X86UpdateIopb(&bitmap, 8, kConfigAddress);
    if(err) {
        throw std::system_error(err, std::system_category(), "X86UpdateIopb");
    };
#endif
}

#if defined(__i386__)
/**
 * Converts an ACPI PCI ID and register offset into the value to write to the config address port
 * to access that register.
 *
 * @note The read address is always aligned to a dword boundary; if you want to address smaller
 * than this granularity, it has to be done in software and emulated.
 */
static uint32_t GetConfigAddress(const uint8_t lbus, const uint8_t ldev, const uint8_t lfunc,
        const size_t regOff) {
    return ((lbus << 16) | (ldev << 11) | (lfunc << 8) | (regOff & 0xfc) | (0x80000000));
}

/**
 * Reads a PCI register.
 */
const uint32_t PciConfig::read(const uint8_t bus, const uint8_t device, const uint8_t func,
        const size_t regOff, const Width width) {
    // perform the read
    io_outl(kConfigAddress, GetConfigAddress(bus, device, func, regOff));
    const auto read = io_inl(kConfigData);

    // yeet out the desired part
    switch(width) {
        case Width::Byte:
            return ((read >> ((regOff & 3) * 8)) & 0xff);
        case Width::Word:
            return ((read >> ((regOff & 2) * 8)) & 0xffff);
        case Width::QWord:
            return read;

        // shouldn't get here...
        default:
            throw std::invalid_argument("invalid width");
    }
}

/**
 * Writes to a PCI register.
 */
const void PciConfig::write(const uint8_t bus, const uint8_t device, const uint8_t func,
        const size_t regOff, const Width width, const uint32_t value) {
    // perform the write
    io_outl(kConfigAddress, GetConfigAddress(bus, device, func, regOff));

    switch(width) {
        case Width::QWord:
            io_outl(kConfigData, value);
            break;

        // XXX: will drop here for 8 and 16 bit writes. implement this
        default:
            throw std::invalid_argument("invalid width");
    }
}

#endif

