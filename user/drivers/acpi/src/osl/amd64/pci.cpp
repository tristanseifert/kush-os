/*
 * Implementations of the ACPICA OS layer for addressing the PCI bus on amd64 machines.
 */
#include "../osl.h"
#include "log.h"

#include <system_error>
#include <sys/syscalls.h>
#include <sys/amd64/syscalls.h>
#include <acpi.h>

/// Whether PCI configuration accesses are logged
static bool gLogAcpicaPciIo = true;

/**
 * Initializes access to the PCI bus.
 */
void osl::InitPciConfig() {
    Warn("%s unimplemented", __PRETTY_FUNCTION__);
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

    // TODO: implement
    return AE_BAD_PARAMETER;
}

/**
 * Writes to a PCI register.
 */
ACPI_STATUS AcpiOsWritePciConfiguration(ACPI_PCI_ID *PciId, UINT32 reg, UINT64 val, UINT32 width) {
    if(gLogAcpicaPciIo) {
        Trace("AcpiOsWritePciConfiguration (Seg %u bus %u device %u:%u) reg %u -> %08x width %u",
                PciId->Segment, PciId->Bus, PciId->Device, PciId->Function, reg, val, width);
    }

    // TODO: implement
    return AE_BAD_PARAMETER;
}

