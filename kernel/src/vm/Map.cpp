#include "Map.h"
#include "MapEntry.h"

#include <new>
#include <arch.h>
#include <log.h>

#include "mem/SlabAllocator.h"

using namespace vm;

static char gAllocBuf[sizeof(mem::SlabAllocator<Map>)] __attribute__((aligned(64)));
static mem::SlabAllocator<Map> *gMapAllocator = nullptr;

/// pointer to the currently active map
Map *Map::gCurrentMap = nullptr;

/// Set to 1 to log adding mappings
#define LOG_MAP_ADD     0

/**
 * Initializes the VM mapping allocator.
 */
void Map::initAllocator() {
    gMapAllocator = reinterpret_cast<mem::SlabAllocator<Map> *>(&gAllocBuf);
    new(gMapAllocator) mem::SlabAllocator<Map>();
}

/**
 * Allocates a new memory map.
 *
 * The high section where the kernel lives (0xC0000000 and above) is referenced by this map if
 * requested. Note that this directly points to the 
 */
Map::Map(const bool copyKernel) : 
    // this is a little cursed
    table(arch::vm::PTEHandler((copyKernel ? (&Map::kern()->table) : nullptr))) {

}

/**
 * Decrements the ref counts of all memory referenced by this map.
 */
Map::~Map() {
    // release all map entries
    for(auto entry : this->entries) {
        entry->release();
    }

    // TODO: implement
}

/**
 * Allocates a new VM map.
 */
Map *Map::alloc() {
    if(!gMapAllocator) initAllocator();
    return gMapAllocator->alloc(true);
}

/**
 * Frees a previously allocated VM map.
 */
void Map::free(Map *ptr) {
    gMapAllocator->free(ptr);
}


/**
 * Activates this map, switching the processor to use the translations defined within.
 */
void Map::activate() {
    this->table.activate();
    gCurrentMap = this;
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

    RW_LOCK_WRITE_GUARD(this->lock);

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
    const bool write = TestFlags(mode & MapMode::WRITE);
    const bool execute = TestFlags(mode & MapMode::EXECUTE);
    const bool global = TestFlags(mode & MapMode::GLOBAL);
    const bool user = TestFlags(mode & MapMode::ACCESS_USER);
    const bool nocache = TestFlags(mode & MapMode::CACHE_DISABLE);

    // map each of the pages
    for(uintptr_t off = 0; off < length; off += pageSz) {
        err = this->table.mapPage(physAddr + off, vmAddr + off, write, execute, global, user, nocache);

        if(err) {
            return err;
        }
    }

    // all mappings completed
    return 0;
}

/**
 * Removes a translation from the memory map.
 */
int Map::remove(const uintptr_t vmAddr, const uintptr_t _length) {
    int err;
    const auto pageSz = arch_page_size();

    RW_LOCK_WRITE_GUARD(this->lock);

    // round up length if needed
    uintptr_t length = _length;
    if(length % pageSz) {
        length += pageSz - (length % pageSz);
    }

    for(uintptr_t off = 0; off < length; off += pageSz) {
        err = this->table.unmapPage(vmAddr + off);

        if(err) {
            return err;
        }
    }

    // all mappings completed
    return 0;

}

/**
 * Invokes the platform-specific PTE handler to determine the physical address for a given virtual
 * address.
 *
 * @return Negative if error, 1 if there is no mapping at that location, or 0 if a mapping exists.
 */
int Map::get(const uintptr_t virtAddr, uint64_t &phys, MapMode &mode) {
    RW_LOCK_READ_GUARD(this->lock);

    // this translation of flags is honestly a little cursed
    int err;
    bool w = false, x = false, g = false, u = false, c = false;

    err = this->table.getMapping(virtAddr, phys, w, x, g, u, c);
    if(!err) {
        MapMode temp = MapMode::READ;

        if(w) temp |= MapMode::WRITE;
        if(x) temp |= MapMode::EXECUTE;
        if(g) temp |= MapMode::GLOBAL;
        if(u) temp |= MapMode::ACCESS_USER;
        if(c) temp |= MapMode::CACHE_DISABLE;

        mode = temp;
    }

    return err;
}



/**
 * Maps the given virtual memory object.
 *
 * @return 0 on success
 */
int Map::add(MapEntry *_entry) {
    RW_LOCK_WRITE(&this->lock);

    // retain entry
    auto entry = _entry->retain();

    // check that it doesn't overlap with anything else
    for(auto entry : this->entries) {
        if(entry->intersects(_entry)) {
            goto fail;
        }
    }

    // the mapping doesn't overlap, so add it
    this->entries.push_back(entry);

    // allow the performing of mapping modifications by the entry
    RW_UNLOCK_WRITE(&this->lock);

    entry->addedToMap(this);
    return 0;

fail:;
    // ensure we release the lock on failure
    RW_UNLOCK_WRITE(&this->lock);

    // we don't wnat to add the map
    entry->release();
    return -1;
}

/**
 * Removes the given entry from this map.
 */
int Map::remove(MapEntry *_entry) {
    RW_LOCK_WRITE_GUARD(this->lock);

    // iterate over all entries...
    for(size_t i = 0; i < this->entries.size(); i++) {
        auto entry = this->entries[i];

        // we've found it! so invoke its removal callback
        if(entry == _entry) {
            entry->removedFromMap(this);

            // then remove it
            this->entries.remove(i);
            entry->release();

            return 0;
        }
    }

    // if we get here, the entry wasn't found
    return -1;
}



/**
 * Handles an userspace address range page fault.
 *
 * This handles the following situations:
 * - Anonymous allocation that hasn't had all pages faulted in yet
 * - Copy-on-write pages
 *
 * Any other type of fault will cause a signal to be sent to the process.
 */
bool Map::handlePagefault(const uintptr_t virtAddr, const bool present, const bool write) {
    RW_LOCK_READ_GUARD(this->lock);

    // test all our regions
    for(auto entry : this->entries) {
        // this one contains it :D
        if(entry->contains(virtAddr)) {
            return entry->handlePagefault(virtAddr, present, write);
        }
    }

    // if we get here, no region contains the address
    return false;
}

