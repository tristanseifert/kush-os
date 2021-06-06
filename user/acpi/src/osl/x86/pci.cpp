/*
 * Implementations of the ACPICA OS layer for addressing the PCI bus using the legacy x86 IO port
 * access mechanism.
 */
#include "osl.h"
#include "x86_io.h"
#include "log.h"

#include <system_error>
#include <sys/syscalls.h>
#include <sys/x86/syscalls.h>
#include <acpi.h>

/// IO port address for the configuration address
constexpr static const uint16_t kConfigAddress  = 0xCF8;
/// IO port address for the configuration data port
constexpr static const uint16_t kConfigData     = 0xCFC;

/// Whether PCI configuration accesses are logged
static bool gLogAcpicaPciIo = false;

/**
 * Initializes the PCI bus configuration mechanism.
 */
void osl::InitPciConfig() {
    int err;

    // allow access to the PCI configuration registers
    static const uint8_t bitmap[] = {
        0xFF
    };

    err = X86UpdateIopb(&bitmap, 8, kConfigAddress);
    if(err) {
        throw std::system_error(err, std::system_category(), "X86UpdateIopb");
    }
}

/**
 * Converts an ACPI PCI ID and register offset into the value to write to the config address port
 * to access that register.
 *
 * @note The read address is always aligned to a dword boundary; if you want to address smaller
 * than this granularity, it has to be done in software and emulated.
 */
static uint32_t GetConfigAddress(ACPI_PCI_ID *id, const size_t regOff) {
    // extract info
    uint32_t lbus  = id->Bus;
    uint32_t ldev  = id->Device;
    uint32_t lfunc = id->Function;

    return ((lbus << 16) | (ldev << 11) | (lfunc << 8) | (regOff & 0xfc) | (0x80000000));
}

/**
 * Reads a PCI register.
 *
 * TODO: Support 64-bit wide register reads
 */
ACPI_STATUS AcpiOsReadPciConfiguration(ACPI_PCI_ID *PciId, UINT32 reg, UINT64 *outVal,
        UINT32 width) {
    if(gLogAcpicaPciIo) {
        Trace("AcpiOsReadPciConfiguration (Seg %u bus %u device %u:%u) reg %u width %u",
                PciId->Segment, PciId->Bus, PciId->Device, PciId->Function, reg, width);
    }


    // validate arguments
    if(!outVal) {
        return AE_BAD_PARAMETER;
    } else if(width != 8 && width != 16 && width != 32) {
        return AE_BAD_PARAMETER;
    }

    // perform the read
    io_outl(kConfigAddress, GetConfigAddress(PciId, reg));
    const auto read = io_inl(kConfigData);

    // yeet out the desired part
    switch(width) {
        case 8:
            *outVal = ((read >> ((reg & 3) * 8)) & 0xff);
            break;
        case 16:
            *outVal = ((read >> ((reg & 2) * 8)) & 0xffff);
            break;
        case 32:
            *outVal = read;
            break;

        // shouldn't get here...
        default:
            return AE_BAD_PARAMETER;
    }

    return AE_OK;
}

/**
 * Writes to a PCI register.
 */
ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId, UINT32 reg, UINT64 val, UINT32 width) {
    if(gLogAcpicaPciIo) {
        Trace("AcpiOsWritePciConfiguration (Seg %u bus %u device %u:%u) reg %u -> %08x width %u",
                PciId->Segment, PciId->Bus, PciId->Device, PciId->Function, reg, val, width);
    }

    // validate arguments
    if(width != 8 && width != 16 && width != 32) {
        return AE_BAD_PARAMETER;
    }

    // perform the write
    io_outl(kConfigAddress, GetConfigAddress(PciId, reg));

    switch(width) {
        case 32:
            io_outl(kConfigData, val);
            break;

        // XXX: will drop here for 8 and 16 bit writes. implement this
        default:
            return AE_BAD_PARAMETER;
    }

    return AE_OK;
}

