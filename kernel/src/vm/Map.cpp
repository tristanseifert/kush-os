#include "Map.h"
#include "MapEntry.h"

#include <new>
#include <arch.h>
#include <arch/PerCpuInfo.h>

#include <log.h>

#include "mem/SlabAllocator.h"

using namespace vm;

bool Map::gLogAdd = false;

static char gAllocBuf[sizeof(mem::SlabAllocator<Map>)] __attribute__((aligned(64)));
static mem::SlabAllocator<Map> *gMapAllocator = nullptr;

/**
 * Deleter that will release a map back to the appropriate allocation pool
 */
struct MapDeleter {
    void operator()(Map *obj) const {
        gMapAllocator->free(obj);
    }
};

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
 * Performs cleanup of the map.
 *
 * We don't have to deallocate entries, as these are referenced from the map tree with smart
 * pointers.
 */
Map::~Map() {

}

/**
 * Allocates a new VM map.
 */
rt::SharedPtr<Map> Map::alloc() {
    if(!gMapAllocator) initAllocator();
    auto map = gMapAllocator->alloc(true);

    return rt::SharedPtr<Map>(map, MapDeleter());
}


/**
 * Activates this map, switching the processor to use the translations defined within.
 */
void Map::activate() {
    // avoid activating map if not necessary
    if(!this->table.isActive()) {
        this->table.activate();
    }
    arch::PerCpuInfo::get()->activeMap = this;
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

    if(gLogAdd) {
        log("Adding mapping to %p: $%lx -> $%llx (length $%lx, mode $%04lx)", this, vmAddr, physAddr, length,
                (uint32_t) mode);
    }

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
            continue;
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
int Map::add(const rt::SharedPtr<MapEntry> &entry, const rt::SharedPtr<sched::Task> &task,
        const uintptr_t requestedBase, const MappingFlags flagsMask, const size_t _size,
        const uintptr_t searchStart, const uintptr_t searchEnd) {
    REQUIRE(entry, "invalid %s", "vm object");
    REQUIRE(task, "invalid %s", "task");

    uintptr_t nextFree;

    uintptr_t base = requestedBase;
    const auto size = _size ? _size : entry->length;

    // either test whether the provided base address is valid, or select one
    RW_LOCK_WRITE(&this->lock);

    // test that the range of [base, base+length) does not overlap any existing mappings
    if(base) {
        if(this->entries.isRegionFree(base, size, nextFree)) {
            goto success;
        }

        goto fail;
    } 
    // find a large enough free space in the provided VM range
    else {
        base = searchStart;

        while(base < (searchEnd - size)) {
            // test for overlapping
            if(this->entries.isRegionFree(base, size, nextFree)) {
                goto success;
            }

            // not free so increase past end of region
            base = nextFree;
        }

        // failed to find a large enough space
        goto fail;
    }

success:;
    // the mapping doesn't overlap, so add it
    this->entries.insert(base, size, entry);
    RW_UNLOCK_WRITE(&this->lock);

    entry->addedToMap(this, task, base, flagsMask);
    return 0;

fail:;
    // release lock
    RW_UNLOCK_WRITE(&this->lock);

    return -1;
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
 * Returns the base address of a particular VM object in this memory map.
 *
 * @note This method uses an address of 0 to indicate the region was not found; this means that we
 * don't really support mappings at address zero.
 *
 * @return Virtual base address of the region, or 0 if not found
 */
const uintptr_t Map::getRegionBase(const rt::SharedPtr<MapEntry> entry) {
    RW_LOCK_READ_GUARD(this->lock);
    return this->entries.baseAddressFor(entry);
}

/**
 * Gets information about a particular VM object mapped into this address space.
 *
 * @param region VM object to get info for
 * @param outBase Base address of the object
 * @param outSize Mapped size of the object (may differ from object size!)
 * @param outFlags Actual flags that the object is mapped with
 *
 * @return 0 on success, negative error code otherwise
 */
int Map::getRegionInfo(rt::SharedPtr<MapEntry> region, uintptr_t &outBase, size_t &outSize,
        MappingFlags &outFlags) {
    size_t length;
    MappingFlags flags = MappingFlags::None;

    // get info on the region
    RW_LOCK_READ_GUARD(this->lock);
    const auto base = this->entries.baseAddressFor(region, length, flags);
    if(!base) {
        return -1;
    }

    // region was found, return info
    outBase = base;
    outSize = length;

    if(flags != MappingFlags::None) {
        outFlags = flags;
    } else {
        outFlags = region->getFlags();
    }

    return 0;
}


/**
 * Removes the given entry from this map.
 */
int Map::remove(const rt::SharedPtr<MapEntry> &entry, const rt::SharedPtr<sched::Task> &task) {
    REQUIRE(task, "invalid %s", "task");

    RW_LOCK_WRITE(&this->lock);

    // find its base address in the tree
    size_t length;
    auto base = this->entries.baseAddressFor(entry, length);

    if(!base) {
        RW_UNLOCK_WRITE(&this->lock);
        return -1;
    }

    this->entries.remove(base);

    // finish the removal
    RW_UNLOCK_WRITE(&this->lock);

    entry->removedFromMap(this, task, base, length);

    return 0;
}

/**
 * Iterates the list of allocated mappings to see if we contain one.
 */
const bool Map::contains(const rt::SharedPtr<MapEntry> &entry) {
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
    uintptr_t base = 0, offset = 0;

    // is there a VM mapping to handle this pagefault?
    RW_LOCK_READ(&this->lock);
    auto region = this->entries.find(virtAddr, offset, base);
    RW_UNLOCK_READ(&this->lock);

    if(!region) {
        return false;
    }

    // if so, handle it
    return region->handlePagefault(this, base, offset, present, write);
}

/**
 * Prints all mappings in this map
 */
void Map::printMappings() {
    RW_LOCK_READ_GUARD(this->lock);

    log("Active mappings for map %p:", this);
    for(const auto &leaf : this->entries) {
        log("%p - %p: %04x object $%p'h (%04x)", leaf->address, (leaf->address + leaf->size - 1),
                static_cast<uintptr_t>(leaf->flags), leaf->entry->getHandle(),
                static_cast<uintptr_t>(leaf->entry->flags));
    }
}
