#include "Map.h"

#include <arch.h>
#include <log.h>

using namespace vm;

/// Set to 1 to log adding mappings
#define LOG_MAP_ADD     0

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
 * Determines if this map is currently loaded in the current processor.
 */
const bool Map::isActive() const {
    return this->table.isActive();
}

/**
 * Adds a translation to the memory map.
 *
 * @note Length is rounded up to the nearest multiple of the page size, if it's not aligned.
 */
int Map::add(const uint64_t physAddr, const uintptr_t _length, const uintptr_t vmAddr,
        const MapMode mode) {
    int err;
    const auto pageSz = arch_page_size();

    // round up length if needed
    uintptr_t length = _length;

    if(length % pageSz) {
        length += pageSz - (length % pageSz);
    }

#if LOG_MAP_ADD
    log("Adding mapping to %p: $%lx -> $%llx (length $%lx, mode $%04lx)", this, vmAddr, physAddr, length,
            (uint32_t) mode);
#endif

    // decompose the flags
    const bool write = flags(mode & MapMode::WRITE);
    const bool execute = flags(mode & MapMode::EXECUTE);
    const bool global = flags(mode & MapMode::GLOBAL);
    const bool user = flags(mode & MapMode::ACCESS_USER);

    // map each of the pages
    for(uintptr_t off = 0; off < length; off += pageSz) {
        err = this->table.mapPage(physAddr + off, vmAddr + off, write, execute, global, user);

        if(err) {
            return err;
        }
    }

    // all mappings completed
    return 0;
}
