#include "Vm/Map.h"
#include "Vm/Manager.h"
#include "Vm/ContiguousPhysRegion.h"

#include "Logging/Console.h"

#include <Intrinsics.h>
#include <Platform.h>

using namespace Kernel::Vm;

/**
 * @brief Initialize a new contiguous physical memory region.
 *
 * @param physBase Physical base address of the mapping
 * @param length Size of the mapping, in bytes
 * @param mode Desired access mode
 */
ContiguousPhysRegion::ContiguousPhysRegion(const uint64_t physBase, const size_t length,
        const Mode mode) : MapEntry(length, mode), physBase(physBase) {

}

/**
 * @brief Writes all page table entries to map this region.
 *
 * @param base Virtual base address
 * @param map VM map to add to
 * @param pt Physical page tables backing the map, these are modified
 *
 * @todo Add support for large pages
 */
void ContiguousPhysRegion::addedTo(const uintptr_t base, Map &map, Platform::PageTable &pt) {
    int err;

    for(size_t off = 0; off < this->getLength(); off += Platform::PageTable::PageSize()) {
        const auto phys = this->physBase + off;
        const auto virt = base + off;

        err = pt.mapPage(phys, virt, this->getAccessMode(map));
        REQUIRE(!err, "failed to map %016llx to %16llx: %d", phys, virt, err);
    }
}

