#include "Parser.h"

#include <log.h>
#include <printf.h>

#include <bootboot.h>
#include "types.h"
#include "system-table.h"

extern "C" BOOTBOOT bootboot;

using namespace platform;

/**
 * Prints each of the tables we found (including their GUIDs) to the output.
 */
void EfiTableParser::PrintTables() {
    // get sysinfo
    const auto &a = bootboot.arch.x86_64;
    auto sysinfo = reinterpret_cast<const efi_system_table *>(kPhysIdentityMap + a.efi_ptr);

    const auto &eh = sysinfo->Hdr;
    REQUIRE(eh.Signature == EFI_SYSTEM_TABLE_SIGNATURE,
            "invalid EFI system table signature: $%016x", eh.Signature);

    const auto numTables = sysinfo->NumberOfTableEntries;
    log("Have %lu EFI system tables (signature %016lx rev %08x) at %p", numTables, eh.Signature,
            eh.Revision, sysinfo->ConfigurationTable);

    // iterate the tables
    auto tables = reinterpret_cast<const efi_configuration_table *>(kPhysIdentityMap + reinterpret_cast<uintptr_t>(sysinfo->ConfigurationTable));

    for(size_t i = 0; i < numTables; i++) {
        char guidBuf[40];

        const auto &table = tables[i];
        PrintGuid(guidBuf, sizeof(guidBuf), table.VendorGuid);

        log("Table %2lu %s: %p", i, guidBuf, table.VendorTable);
    }
}

/**
 * Finds a table with the given GUID.
 *
 * @return On success, the physical address of the table, or 0 on error.
 */
uint64_t EfiTableParser::FindTable(const efi_guid &guid) {
    const auto &a = bootboot.arch.x86_64;
    auto sysinfo = reinterpret_cast<const efi_system_table *>(kPhysIdentityMap + a.efi_ptr);

    const auto &eh = sysinfo->Hdr;
    REQUIRE(eh.Signature == EFI_SYSTEM_TABLE_SIGNATURE,
            "invalid EFI system table signature: $%016x", eh.Signature);

    // iterate all tables
    const auto inGuidBytes = reinterpret_cast<const uint8_t *>(&guid);
    auto tables = reinterpret_cast<const efi_configuration_table *>(kPhysIdentityMap + reinterpret_cast<uintptr_t>(sysinfo->ConfigurationTable));

    for(size_t i = 0; i < sysinfo->NumberOfTableEntries; i++) {
        // check if GUID matches
        const auto &table = tables[i];
        const auto thisGuidBytes = reinterpret_cast<const uint8_t *>(&table.VendorGuid);

        for(size_t j = 0; j < sizeof(*thisGuidBytes); j++) {
            if(thisGuidBytes[j] != inGuidBytes[j]) goto next;
        }

        // the GUID matched so return its physical address
        return reinterpret_cast<uint64_t>(table.VendorTable);

next:;
    }

    // failed to find table
    return 0;
}


/**
 * Prints a GUID string.
 */
int EfiTableParser::PrintGuid(char *buf, const size_t bufSz, const efi_guid &guid) {
    const auto &b = guid.data4;

    return snprintf(buf, bufSz,
            "%08x-%04x-%04x-%02x%02x-%02x%02x%02x%02x%02x%02x", guid.data1,
            guid.data2, guid.data3, b[0],b[1],b[2],b[3],b[4],b[5],b[6],b[7]);
}

