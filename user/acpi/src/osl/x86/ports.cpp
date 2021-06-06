/*
 * Implementations of the ACPICA OS layer for doing port IO
 *
 * This is the x86 implementation thereof.
 */
#include "osl.h"
#include "log.h"
#include "x86_io.h"

#include <cstdlib>
#include <cstdint>
#include <unordered_set>
#include <utility>
#include <system_error>
#include <sys/x86/syscalls.h>

#include <acpi.h>

/// Whether port accesses are logged
static bool gLogAcpicaPortIo = false;

/// Represents an IO port base + length
using IoRange = std::pair<uint16_t, size_t>;
// hash implementation for IO port ranges
namespace std {
template <> struct hash<IoRange> {
    inline size_t operator()(const IoRange &v) const {
        std::hash<uintptr_t> hasher;
        return hasher(v.first & 0xFFFF) ^ hasher((v.second & 0xFFFF) << 16);
     }
};
}

/// Pairs of (base, range) of all IO ports we've whitelisted.
static std::unordered_set<IoRange> gWhitelistedPorts;

/**
 * Whitelists the IO port and width (in bits) if needed.
 */
static void EnsurePortAccess(const uint16_t port, const uint8_t width) {
    int err;

    // see if this exact range is whitelisted
    const IoRange r(port, (width / 8));
    if(gWhitelistedPorts.contains(r)) {
        return;
    }

    // it isn't, so make the call
    static const uint8_t bitmap[] = {
        0xFF
    };

    if(gLogAcpicaPortIo) Trace("Whitelisting port $%04x (len %u)", port, (width / 8));

    err = X86UpdateIopb(&bitmap, (width / 8), port);
    if(err) {
        Warn("%s failed: %d", "X86UpdateIopb", err);
        throw std::system_error(err, std::system_category(), "X86UpdateIopb");
    }
    // if success, update our list
    gWhitelistedPorts.emplace(std::move(r));
}

/**
 * Reads from an IO port.
 */
ACPI_STATUS AcpiOsReadPort(ACPI_IO_ADDRESS addr, UINT32 *outVal, UINT32 width) {
    // validate arguments
    if(addr >= UINT16_MAX || !outVal) {
        return AE_BAD_PARAMETER;
    }
    else if(width != 8 && width != 16 && width != 32) {
        return AE_BAD_PARAMETER;
    }

    EnsurePortAccess(addr, width);

    // perform the read
    switch(width) {
        case 8:
            *outVal = io_inb(addr);
            break;
        case 16:
            *outVal = io_inw(addr);
            break;
        case 32:
            *outVal = io_inl(addr);
            break;

        // unsupported bit width; shouldn't get here!
        default:
            return AE_BAD_PARAMETER;
    }

    if(gLogAcpicaPortIo) Trace("AcpiOsReadPort $%04x <- $%08x (width %u)", addr, *outVal, width);
    return AE_OK;
}

/**
 * Writes to an IO port.
 */
ACPI_STATUS AcpiOsWritePort(ACPI_IO_ADDRESS addr, UINT32 value, UINT32 width) {
    if(gLogAcpicaPortIo) Trace("AcpiOsWritePort $%04x -> $%08x (width %u)", addr, value, width);

    // validate arguments
    if(addr >= UINT16_MAX) {
        return AE_BAD_PARAMETER;
    }
    else if(width != 8 && width != 16 && width != 32) {
        return AE_BAD_PARAMETER;
    }

    EnsurePortAccess(addr, width);

    // perform the write
    switch(width) {
        case 8:
            io_outb(addr, (value & 0xFF));
            break;
        case 16:
            io_outw(addr, (value & 0xFFFF));
            break;
        case 32:
            io_outl(addr, value);
            break;

        // unsupported bit width; shouldn't get here!
        default:
            return AE_BAD_PARAMETER;
    }

    return AE_OK;
}
