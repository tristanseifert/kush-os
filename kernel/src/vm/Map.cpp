#include "Map.h"

#include <arch.h>
#include <log.h>

using namespace vm;

/**
 * Allocates a new memory map.
 *
 * The high section where the kernel lives (0xC0000000 and above) is referenced by this map if
 * requested. Note that this directly points to the 
 */
Map::Map(const bool copyKernel) {

}

/**
 * Decrements the ref counts of all memory referenced by this map.
 */
Map::~Map() {
    // TODO: implement
}



/**
 * Activates this map, switching the processor to use the translations defined within.
 */
void Map::activate() {
    this->table.activate();
}

/**
 * Adds a translation to the memory map.
 *
 * @note Length is rounded up to the nearest multiple of the page size, if it's not aligned.
 */
int Map::add(const uint64_t physAddr, const uintptr_t _length, const uintptr_t vmAddr,
        const MapMode mode) {
    const auto pageSz = arch_page_size();

    // round up length if needed
    uintptr_t length = _length;

    if(length % pageSz) {
        length += pageSz - (length % pageSz);
    }

    log("Adding mapping to %p: $%lx -> $%llx (length $%lx, mode $%04lx)", this, vmAddr, physAddr, length,
            (uint32_t) mode);

    // if we get here, the mapping failed to be added
    return -1;
}
