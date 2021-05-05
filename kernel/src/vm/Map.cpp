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
#define LOG_MAP_ADD                     0
/// Set to log deferred mappings
#define LOG_DEFERRED_MAP                0

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
    this->entries.iterate([](auto node) {
        node.entry->release();
    });

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

    // map each of the pages now if possible
    for(uintptr_t off = 0; off < length; off += pageSz) {
        const auto pa = physAddr + off;
        const auto va = vmAddr + off;

        err = this->table.mapPage(pa, va, write, execute, global, user, nocache);

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

        // ignore the case where there's no mapping at the address
        if(err && err != 1) {
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
 * Maps the given virtual memory object into this virtual memory map.
 *
 * If the base address is non-zero, the call will fail if placing it at that address would cause
 * it to overlap an already existing mapping.
 *
 * If the base address is zero, we'll place it at the first address in the range [searchStart,
 * searchEnd) where it will not overlap an existing allocation. The search always starts at the
 * start of the range, regardless of where the last allocation was placed.
 *
 * @param entry VM object to add to this VM map
 * @param task The task to which this VM map belongs
 * @param requestedBase Page aligned base address for the mapping, or 0 to find a suitable base
 * @param flags If specified, overrides some of the flags of the VM object (access permissions)
 * @param searchStart Lower bound for an automatically selected base address
 * @param searchEnd Upper bound (minus VM object size) for an automatically selected base address
 *
 * @return 0 on success
 */
int Map::add(MapEntry *entry, sched::Task *task, const uintptr_t requestedBase,
        const MappingFlags flagsMask, const uintptr_t searchStart, const uintptr_t searchEnd) {
    uintptr_t base = requestedBase;

    // retain entry
    entry = entry->retain();

    // either test whether the provided base address is valid, or select one
    RW_LOCK_WRITE(&this->lock);

    // test that the range of [base, base+length) does not overlap any existing mappings
    if(base) {
        // TODO
        goto fail;
    } 
    // find a large enough free space in the provided VM range
    else {
        // TODO
        goto fail;
    }

    // the mapping doesn't overlap, so add it
    this->entries.insert(base, entry->length, entry);
    RW_UNLOCK_WRITE(&this->lock);

    entry->addedToMap(this, task, base, flagsMask);
    return 0;

fail:;
    // ensure we release the lock on failure
    RW_UNLOCK_WRITE(&this->lock);

    // we don't wnat to add the map
    entry->release();
    return -1;
}

/**
 * Tests if the new size for the given map entry will cause any conflicts with existing mappings.
 *
 * In effect, this takes the region of [base+length, base+newlength] and checks if there exist any
 * mappings in thatspace.
 */
bool Map::canResize(MapEntry *entry, const uintptr_t base, const size_t oldSize, const size_t newSize) {
    // we can always support shrinking
    if(newSize <= oldSize) return true;

    // get the new range to check
    // const auto newBase = base + oldSize;
    // const auto sizeDiff = newSize - oldSize;

    // log("Old region: %08x -> %08x; new region to test %08x - %08x", base, base+oldSize, base+oldSize, base+newSize);

    RW_LOCK_READ_GUARD(this->lock);

    // TODO: implement
    return false;
}

/**
 * Searches all regions in this VM map to see if one contains the given virtual address.
 *
 * @note This does not consider manually added mappings, i.e. those that aren't represented by a
 * MapEntry object. This should, however, be the only type of mappings an user task will ever
 * have to encounter.
 */
bool Map::findRegion(const uintptr_t virtAddr, Handle &outHandle, uintptr_t &outOffset) {
    RW_LOCK_READ_GUARD(this->lock);

    auto vobj = this->entries.find(virtAddr, outOffset);
    if(vobj) {
        outHandle = vobj->getHandle();
        return true;
    }

    // if we get here, no region contains this address
    return false;
}

/**
 * Removes the given entry from this map.
 */
int Map::remove(MapEntry *entry, sched::Task *task) {
    REQUIRE(entry, "invalid %s", "vm object");
    REQUIRE(task, "invalid %s", "task");

    RW_LOCK_WRITE(&this->lock);

    // find its base address in the tree
    auto base = this->entries.baseAddressFor(entry);

    if(!base) {
        RW_UNLOCK_WRITE(&this->lock);
        return -1;
    }

    this->entries.remove(base);

    // finish the removal
    RW_UNLOCK_WRITE(&this->lock);

    entry->removedFromMap(this, task);
    entry->release();

    return 0;
}

/**
 * Iterates the list of allocated mappings to see if we contain one.
 */
const bool Map::contains(MapEntry *entry) {
    RW_LOCK_READ_GUARD(this->lock);
    return (this->entries.baseAddressFor(entry) != 0);
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
    uintptr_t offset;

    // is there a VM mapping to handle this pagefault?
    RW_LOCK_READ(&this->lock);
    auto region = this->entries.find(virtAddr, offset);
    RW_UNLOCK_READ(&this->lock);

    if(!region) {
        return false;
    }

    // if so, handle it
    return region->handlePagefault(this, virtAddr, offset, present, write);
}

