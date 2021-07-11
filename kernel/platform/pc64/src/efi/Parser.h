#ifndef PLATFORM_PC64_EFI_PARSER_H
#define PLATFORM_PC64_EFI_PARSER_H

#include <stddef.h>
#include <stdint.h>

struct efi_guid;

namespace platform {
/**
 * Provides an interface for enumerating and accessing EFI configuration tables.
 */
class EfiTableParser {
    public:
        /// Prints all tables on the system.
        static void PrintTables();

        /// Attempts to locate a table with the given GUID.
        static uint64_t FindTable(const efi_guid &guid);

        /// Prints an EFI GUID to a string buffer.
        static int PrintGuid(char *buf, const size_t bufSz, const efi_guid &guid);

    private:
        /// Base address of the physical memory identity mapping zone
        constexpr static uintptr_t kPhysIdentityMap{0xffff800000000000};
};
}

#endif
