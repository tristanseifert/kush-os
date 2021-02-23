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

    // handle deferred maps
    size_t numDeferred = 0;
    __atomic_load(&this->numDeferredMaps, &numDeferred, __ATOMIC_ACQUIRE);

    if(numDeferred) {
        RW_LOCK_WRITE_GUARD(this->lock);
        int err;

#if LOG_DEFERRED_MAP
        log("Performing %u deferred mappings", numDeferred);
#endif
        while(!this->deferredMaps.empty()) {
            const auto toMap = this->deferredMaps.pop();

            const bool write = TestFlags(toMap.mode & MapMode::WRITE);
            const bool execute = TestFlags(toMap.mode & MapMode::EXECUTE);
            const bool global = TestFlags(toMap.mode & MapMode::GLOBAL);
            const bool user = TestFlags(toMap.mode & MapMode::ACCESS_USER);
            const bool nocache = TestFlags(toMap.mode & MapMode::CACHE_DISABLE);

#if LOG_DEFERRED_MAP
            log("Mapping %016llx -> %08x w %d x %d g %d u %d c %d", toMap.physAddr, toMap.vmAddr,
                    write, execute, global, user, !nocache);
#endif

            err = this->table.mapPage(toMap.physAddr, toMap.vmAddr, write, execute, global, user,
                    nocache);
            if(err) {
                log("Failed to add mapping in %p: %d (phys %016llx, virt %08x, mode %08x)",
                        this, err, toMap.physAddr, toMap.vmAddr, toMap.mode);
            }
        }

        // clear the counter
        size_t zero = 0;
        __atomic_store(&this->numDeferredMaps, &zero, __ATOMIC_RELEASE);
    }
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

        if(this->isActive() || (!this->isActive() && this->table.supportsUnmappedModify(va))) {
            err = this->table.mapPage(pa, va, write, execute, global, user, nocache);

            if(err) {
                return err;
            }
        } 
        // otherwise, defer the mapping to the next time this map is activated
        else {
            // TODO: it would be nice to coalesce adjacent mappings

            // build the mapping info
            DeferredMapping def(pa, va, mode);
            this->deferredMaps.push_back(DeferredMapping(def));

            __atomic_add_fetch(&this->numDeferredMaps, 1, __ATOMIC_RELAXED);
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
 * Maps the given virtual memory object.
 *
 * @return 0 on success
 */
int Map::add(MapEntry *_entry, sched::Task *task, const uintptr_t _base,
        const MappingFlags flagsMask) {
    RW_LOCK_WRITE(&this->lock);

    // retain entry
    auto entry = _entry->retain();
    uintptr_t base = _base;

    if(entry->base || base) {
        if(!base) {
            // check that it doesn't overlap with anything else
            for(auto i : this->entries) {
                if(i->intersects(this, entry)) {
                    goto fail;
                }
            }
        }
        // check the desired new base address
        else {
            for(auto i : this->entries) {
                if(i->contains(this, base, entry->length)) {
                    goto fail;
                }
            }
        }
    } else {
        // find a VM address for this
        base = kVmSearchBase;

        while((base + _entry->length) < kVmMaxAddr) {
            // check whether this address is suitable
            for(auto i : this->entries) {
                // it's intersected an existing mapping; try again after that mapping
                if(i->contains(this, base, entry->length)) {
                    base = (i->getBaseAddressIn(this) + i->length);
                    continue;
                }
            }

            // if we get here, it didn't collide with any other mappings
            goto beach;
        }

        // if we're here, we cannot find a suitable base address
        goto fail;
    }

beach:;
    // the mapping doesn't overlap, so add it
    this->entries.push_back(entry);

    // allow the performing of mapping modifications by the entry
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
    const auto newBase = base + oldSize;
    const auto sizeDiff = newSize - oldSize;

    // log("Old region: %08x -> %08x; new region to test %08x - %08x", base, base+oldSize, base+oldSize, base+newSize);

    RW_LOCK_READ_GUARD(this->lock);

    for(auto i : this->entries) {
        if(i->contains(this, newBase, sizeDiff)) {
            return false;
        }
    }

    // if we get down here, there's no conflicts
    return true;
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

    for(auto i : this->entries) {
        if(i->contains(this, virtAddr, 1)) {
            outHandle = i->getHandle();
            outOffset = virtAddr - i->getBaseAddressIn(this);
            return true;
        }
    }

    // if we get here, no region contains this address
    return false;
}

/**
 * Removes the given entry from this map.
 */
int Map::remove(MapEntry *_entry, sched::Task *task) {
    MapEntry *removed = nullptr;

    RW_LOCK_WRITE(&this->lock);

    // iterate over all entries...
    for(size_t i = 0; i < this->entries.size(); i++) {
        auto entry = this->entries[i];

        // we've found it! so invoke its removal callback
        if(entry == _entry) {
            // then remove it
            this->entries.remove(i);
            removed = entry;
            goto beach;
        }
    }

    // if we get here, the entry wasn't found
    RW_UNLOCK_WRITE(&this->lock);
    return -1;

beach:;
    // finish the removal
    RW_UNLOCK_WRITE(&this->lock);
    REQUIRE(removed, "wtf");

    removed->removedFromMap(this, task);
    removed->release();

    return 0;
}

/**
 * Iterates the list of allocated mappings to see if we contain one.
 */
const bool Map::contains(MapEntry *entry) {
    RW_LOCK_READ_GUARD(this->lock);
    for(auto map : this->entries) {
        if(map == entry) {
            return true;
        }
    }

    return false;
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
    MapEntry *region = nullptr;

    RW_LOCK_READ(&this->lock);

    // test all our regions
    for(auto entry : this->entries) {
        // this one contains it :D
        if(entry->contains(this, virtAddr)) {
            region = entry;
            goto beach;
        }
    }
    RW_UNLOCK_READ(&this->lock);

    // if we get here, no region contains the address
    return false;

beach:;
    RW_UNLOCK_READ(&this->lock);
    // we found a region to handle it
    return region->handlePagefault(this, virtAddr, present, write);
}

