/*
 * Implementations of the ACPICA OS layer to implement table overrides.
 *
 * None of these currently do anything.
 */
#include "../osl.h"
#include "log.h"

#include <stdint.h>

#include <acpi.h>
#include <threads.h>

#include <sys/syscalls.h>
#include <sys/amd64/syscalls.h>

#include "bootboot.h"
#include "efi/types.h"
#include "efi/system-table.h"

/// cached bootloader info, we load this from kernel once
static BOOTBOOT gLoaderInfo;
/// ensure the loader info is retrieved from the kernel exactly once
static once_flag gLoaderInfoInitFlag = ONCE_FLAG_INIT;
/// address of the ACPI base table
static uintptr_t gTableAddress = 0;
/// whether the loader info is actually valid
static bool gLoaderInfoValid = false;

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
 * Loads the boot information structure from the kernel, and reads out the ACPI root table base
 * address from it.
 */
static void InitAcpiTableBase() {
    size_t cfgTableBytes;
    uintptr_t cfgTablePhys;

    // read boot info structure
    int err = Amd64CopyLoaderInfo(&gLoaderInfo, sizeof(gLoaderInfo));
    if(err < 0) {
        Abort("%s failed: %d", "Amd64CopyLoaderInfo", err);
    }

    Trace("EFI sysinfo at phys $%p", gLoaderInfo.arch.x86_64.efi_ptr);

    {
        // map the EFI info table and get the config table base
        const auto sysinfoPhys = gLoaderInfo.arch.x86_64.efi_ptr;
        void *sysinfoMapped = AcpiOsMapMemory(sysinfoPhys, sizeof(efi_system_table));
        if(!sysinfoMapped) {
            Abort("Failed to map %s (phys $%p)", "EFI system info table", sysinfoPhys);
        }

        const auto sysinfo = reinterpret_cast<const efi_system_table *>(sysinfoMapped);

        Trace("EFI sysinfo has %lu configuration tables at $%p",
                sysinfo->NumberOfTableEntries, sysinfo->ConfigurationTable);

        // unmap the system table now once we've read the cfg table info
        cfgTableBytes = sizeof(efi_configuration_table) * sysinfo->NumberOfTableEntries;
        cfgTablePhys = reinterpret_cast<uintptr_t>(sysinfo->ConfigurationTable);

        AcpiOsUnmapMemory(sysinfoMapped, sizeof(efi_system_table));
    }

    if(!cfgTablePhys || !cfgTableBytes) {
        Abort("EFI system table has no configuration tables; cannot find RSDP");
    }

    // map config tables base
    void *cfgTableBase = AcpiOsMapMemory(cfgTablePhys, cfgTableBytes);
    if(!cfgTableBase) {
        Abort("Failed to map %s (phys $%p)", "EFI configuration tables", cfgTablePhys);
    }

    // define the table GUIDs we're interested in
    static const efi_guid kAcpi20Guid = ACPI_20_TABLE_GUID;

    // iterate over all config tables
    auto tables = reinterpret_cast<const efi_configuration_table *>(cfgTableBase);
    const auto cfgTableNum = cfgTableBytes / sizeof(efi_configuration_table);

    for(size_t i = 0; i < cfgTableNum; i++) {
        const auto &table = tables[i];

        // check the GUID: is it the ACPI 2.0 table?
        if(!memcmp(&kAcpi20Guid, &table.VendorGuid, sizeof(efi_guid))) {
            gTableAddress = reinterpret_cast<uintptr_t>(table.VendorTable);
            goto beach;
        }
    }

    // failed to find ACPI table
    Abort("Failed to find ACPI RSDP in EFI config tables!");

beach:;
    Trace("Found RSDP at phys $%p", gTableAddress);

    // clean up
    AcpiOsUnmapMemory(cfgTableBase, cfgTableBytes);

    // mark as valid
    __atomic_store_n(&gLoaderInfoValid, true, __ATOMIC_RELAXED);
}

/**
 * Locates the ACPI table root pointer (RSDP).
 *
 * We look in the BOOTBOOT info structure for the EFI info block, which then points us to the RSDP
 * that we can return here. This is all taking place in `InitAcpiTableBase`.
 */
ACPI_PHYSICAL_ADDRESS AcpiOsGetRootPointer() {
    // perform initialization if needen, then wait til valid
    call_once(&gLoaderInfoInitFlag, InitAcpiTableBase);

    while(!__atomic_load_n(&gLoaderInfoValid, __ATOMIC_RELAXED)) {
        asm volatile("pause" ::: "memory");
    }

    return gTableAddress;
}
