/*
 * Implementations of the ACPICA OS layer to implement table overrides.
 *
 * None of these currently do anything.
 */
#include "osl.h"
#include "log.h"

#include <acpi.h>
#include <threads.h>

#include <sys/syscalls.h>

/**
 * Override an object in ACPI namespace.
 */
ACPI_STATUS AcpiOsPredefinedOverride(const ACPI_PREDEFINED_NAMES *PredefinedObject,
        ACPI_STRING *NewValue) {
    *NewValue = NULL;
    return AE_OK;
}

/**
 * Overwrite an entire ACPI table.
 */
ACPI_STATUS AcpiOsTableOverride(ACPI_TABLE_HEADER *ExistingTable,
        ACPI_TABLE_HEADER **NewTable) {
    *NewTable = NULL;
    return AE_OK;
}

/**
 * Overwrite an ACPI table with a differing physical address.
 */
ACPI_STATUS AcpiOsPhysicalTableOverride(ACPI_TABLE_HEADER *ExistingTable,
        ACPI_PHYSICAL_ADDRESS *NewAddress, UINT32 *NewTableLength) {
    *NewAddress = 0;
    return AE_OK;
}

/**
 * Locates the ACPI table root pointer (RSDP).
 *
 * We simply use the built in function to scan the first 1M of physical memory for the ACPI table
 * root pointer.
 */
ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer() {
    ACPI_PHYSICAL_ADDRESS  Ret;
    Ret = 0;
    AcpiFindRootPointer(&Ret);
    return Ret;
}
