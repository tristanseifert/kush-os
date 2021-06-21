/*
 * Implementations of the ACPICA OS layer for doing port IO. On amd64, all port IO goes through
 * the kernel via some syscalls. Additionally, IO port ranges are still placed on a blocklist by
 * default, so we need to mark them as allowed the first time they're used.
 */
#include "../osl.h"
#include "log.h"

#include <cstdlib>
#include <cstdint>
#include <unordered_set>
#include <utility>
#include <system_error>
#include <sys/amd64/syscalls.h>

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

    err = Amd64UpdateAllowedIoPorts(&bitmap, (width / 8), port);
    if(err) {
        Warn("%s failed: %d", "Amd64UpdateAllowedIoPorts", err);
        throw std::system_error(err, std::system_category(), "Amd64UpdateAllowedIoPorts");
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

    // set up flags
    uintptr_t flags = 0;

    switch(width) {
        case 8:
            flags |= SYS_ARCH_AMD64_PORT_BYTE;
            break;
        case 16:
            flags |= SYS_ARCH_AMD64_PORT_WORD;
            break;
        case 32:
            flags |= SYS_ARCH_AMD64_PORT_DWORD;
            break;

        // unsupported bit width; shouldn't get here!
        default:
            return AE_BAD_PARAMETER;
    }

    // do read
    int err = Amd64PortRead(addr, flags, outVal);
    if(err) {
        Warn("Amd64PortRead(%04x, %04x, %p) failed: %d", addr, flags, outVal, err);
        return AE_ERROR;
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

    // set up flags
    uintptr_t flags = 0;

    switch(width) {
        case 8:
            flags |= SYS_ARCH_AMD64_PORT_BYTE;
            break;
        case 16:
            flags |= SYS_ARCH_AMD64_PORT_WORD;
            break;
        case 32:
            flags |= SYS_ARCH_AMD64_PORT_DWORD;
            break;

        // unsupported bit width; shouldn't get here!
        default:
            return AE_BAD_PARAMETER;
    }

    // do write
    int err = Amd64PortWrite(addr, flags, value);
    if(err) {
        Warn("Amd64PortWrite(%04x, %04x, %08x) failed: %d", addr, flags, value, err);
        return AE_ERROR;
    }

    return AE_OK;
}
