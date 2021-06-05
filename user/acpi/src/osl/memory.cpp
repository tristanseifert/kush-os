/*
 * Implementations of the ACPICA OS layer, for functions to do with memory allocation and virtual
 * memory.
 */
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <sys/syscalls.h>

#include <acpi.h>

#include "log.h"

/// Whether memory map/unmap is logged
static bool gLogAcpicaMemMap = false;
/// Whether memory read/write is logged
static bool gLogAcpicaMemOps = true;


/// VM range [start, end) into which ACPI mappings are placed
static const uintptr_t kVmMappingRange[2] = {
    0x10000000000, 0x20000000000
};
/**
 * Allocate memory; thunk through to the C heap.
 */
void *AcpiOsAllocate(ACPI_SIZE Size) {
    return malloc(Size);
}
/**
 * Releases memory; again thunk through to the C heap.
 */
void AcpiOsFree(void *Memory) {
    free(Memory);
}

/**
 * Requests mapping of virtual memory.
 *
 * Gotchas with this method include that the physical address nor page number may not be aligned.
 */
void *AcpiOsMapMemory(ACPI_PHYSICAL_ADDRESS PhysicalAddress, ACPI_SIZE Length) {
    uintptr_t region, regionBase;
    int err;

    if(gLogAcpicaMemMap) Trace("AcpiOsMapMemory %08x len %u", PhysicalAddress, Length);

    // round to page size
    const long pageSz = sysconf(_SC_PAGESIZE);
    if(pageSz <= 0) return NULL;

    const uintptr_t physBase = (PhysicalAddress / pageSz) * pageSz;
    const uintptr_t physOffset = PhysicalAddress % pageSz;
    const uintptr_t pageLength = ((Length + pageSz - 1) / pageSz) * pageSz;

    // perform the mapping; we want it to be RW
    err = AllocVirtualPhysRegion(physBase, pageLength, VM_REGION_RW, &region);
    if(err) {
        Warn("%s failed: %d", "AllocVirtualPhysRegion", err);
        return NULL;
    }

    // map it
    err = MapVirtualRegionRange(region, kVmMappingRange, pageLength, 0, &regionBase);
    if(err) {
        Warn("%s failed: %d", "MapVirtualRegionRange", err);
        return NULL;
    }

    // return a correctly offset pointer
    return reinterpret_cast<void *>(regionBase + physOffset);
}

/**
 * Unmaps memory that was placed in our VM address space by AcpiOsMapMemory().
 */
void AcpiOsUnmapMemory(void *where, ACPI_SIZE length) {
    uintptr_t region = 0;
    int err;

    if(gLogAcpicaMemMap) Trace("AcpiOsUnmapMemory %p len %u", where, length);

    // find the VM object corresponding to this address
    err = VirtualGetHandleForAddr(reinterpret_cast<uintptr_t>(where), &region);
    if(err != 1) {
        // mapping not found
        if(!err) {
            Warn("AcpiOsUnmapMemory failed: range (%p, len %u) is not mapped!", where, length);
            return;
        }
        // error during mapping
        else if(err < 0) {
            Warn("%s failed: %d", "VirtualGetHandleForAddr", err);
            return;
        }
    }

    // unmap it
    err = UnmapVirtualRegion(region);
    if(err) {
        Warn("%s failed: %d", "UnmapVirtualRegion", err);
    }
}

/**
 * Checks whether the given memory is readable.
 *
 * This will convert the address to a VM region handle, and if it exists, assume success: it is not
 * possible to create a non-readable mapping.
 *
 * TODO: We should probably check that the _entire_ range is mapped.
 */
BOOLEAN AcpiOsReadable(void *Memory, ACPI_SIZE Length) {
    uintptr_t region = 0;
    int err;

    // find the VM object corresponding to this address
    err = VirtualGetHandleForAddr(reinterpret_cast<uintptr_t>(Memory), &region);
    if(err < 0) {
        Warn("%s failed: %d", "VirtualGetHandleForAddr", err);
        return FALSE;
    }

    return (err == 1);
}



/**
 * Reads from a particular physical address.
 *
 * TODO: We should probably keep a cache of mappings.
 */
ACPI_STATUS AcpiOsReadMemory(ACPI_PHYSICAL_ADDRESS address, UINT64 *outVal, UINT32 width) {
    if(gLogAcpicaMemOps) Trace("AcpiOsReadMemory $%08x width %u", address, width);

    // validate arguments
    if(!outVal) {
        return AE_BAD_PARAMETER;
    } else if(width != 8 && width != 16 && width != 32 && width != 64) {
        return AE_BAD_PARAMETER;
    }

    // create temporary mapping
    void *ptr = AcpiOsMapMemory(address, width / 8);
    if(!ptr) {
        return AE_ERROR;
    }

    // perform the read
    switch(width) {
        case 8:
            *outVal = *reinterpret_cast<uint8_t *>(ptr);
            break;
        case 16:
            *outVal = *reinterpret_cast<uint16_t *>(ptr);
            break;
        case 32:
            *outVal = *reinterpret_cast<uint32_t *>(ptr);
            break;
        case 64:
            *outVal = *reinterpret_cast<uint64_t *>(ptr);
            break;
    }

    // remove the memory mapping
    AcpiOsUnmapMemory(ptr, (width / 8));
    return AE_OK;
}

/**
 * Writes to a physical memory address.
 *
 * TODO: Same caveat as for AcpiOsReadMemory applies; we should cache VM mappings.
 */
ACPI_STATUS AcpiOsWriteMemory(ACPI_PHYSICAL_ADDRESS address, UINT64 val, UINT32 width) {
    if(gLogAcpicaMemOps) Trace("AcpiOsWriteMemory $%08x -> $%08x width %u", address, val, width);

    // validate arguments
    if(width != 8 && width != 16 && width != 32 && width != 64) {
        return AE_BAD_PARAMETER;
    }

    // create temporary mapping
    void *ptr = AcpiOsMapMemory(address, width / 8);
    if(!ptr) {
        return AE_ERROR;
    }

    // perform the write
    switch(width) {
        case 8:
            *reinterpret_cast<uint8_t *>(ptr) = (val & 0xFF);
            break;
        case 16:
            *reinterpret_cast<uint16_t *>(ptr) = (val & 0xFFFF);
            break;
        case 32:
            *reinterpret_cast<uint32_t *>(ptr) = (val & 0xFFFFFFFF);
            break;
        case 64:
            *reinterpret_cast<uint64_t *>(ptr) = val;
            break;
    }

    // remove the memory mapping
    AcpiOsUnmapMemory(ptr, (width / 8));
    return AE_OK;
}
