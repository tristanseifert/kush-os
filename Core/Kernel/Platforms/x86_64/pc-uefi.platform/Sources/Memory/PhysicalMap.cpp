#include "PhysicalMap.h"

#include <Logging/Console.h>

using namespace Platform::Amd64Uefi::Memory;

bool PhysicalMap::gIsEarlyBoot{true};

/**
 * Returns the address of the specified physical address in our physical aperture.
 *
 * We don't actually have to do any work here, as 2TB of virtual address space in kernel land are
 * reserved for a physical aperture. (TODO: investigate security, cache coherency implications)
 */
int PhysicalMap::Add(const uintptr_t physical, const size_t length, void **outVirtual) {
    REQUIRE(length && !(length & 0xFFF), "invalid length: $%llx", length);
    REQUIRE(outVirtual, "invalid output address");

    if(gIsEarlyBoot) {
        REQUIRE(physical < 0x1'0000'0000, "addr ($%016llx) out of range for early boot", physical);

        *outVirtual = reinterpret_cast<void *>(physical);
        return 0;
    }
    // use the kernel VM aperture
    else {
        // TODO: implement
        REQUIRE(false, "unimplemented");
    }
}

/**
 * Unmaps a previously mapped physical region, based on its virtual address.
 *
 * @param virtualAddr Base of a previously mapped physical region
 * @param length Length of the region, in bytes
 *
 * @return 0 on success.
 *
 * @remark This is a no-op.
 */
int PhysicalMap::Remove(const void *virtualAddr, const size_t length) {
    return 0;
}

