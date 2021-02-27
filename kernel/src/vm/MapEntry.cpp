#include "MapEntry.h"
#include "Map.h"
#include "Mapper.h"

#include "mem/PhysicalAllocator.h"
#include "mem/SlabAllocator.h"
#include "sched/Task.h"

#include <arch.h>
#include <log.h>
#include <new>

using namespace vm;

static char gAllocBuf[sizeof(mem::SlabAllocator<MapEntry>)] __attribute__((aligned(64)));
static mem::SlabAllocator<MapEntry> *gMapEntryAllocator = nullptr;

static vm::MapMode ConvertVmMode(const MappingFlags flags);

/**
 * Initializes the VM mapp entry allocator.
 */
void MapEntry::initAllocator() {
    gMapEntryAllocator = reinterpret_cast<mem::SlabAllocator<MapEntry> *>(&gAllocBuf);
    new(gMapEntryAllocator) mem::SlabAllocator<MapEntry>();
}



/**
 * Creates a new VM mapping that encompasses the given address range.
 */
MapEntry::MapEntry(const uintptr_t _base, const size_t _length, const MappingFlags _flags) :
    base(_base), length(_length), flags(_flags) {
    // allocate a handle for it
    this->handle = handle::Manager::makeVmObjectHandle(this);
}

/**
 * Deallocator for map entry
 */
MapEntry::~MapEntry() {
    // release handle
    handle::Manager::releaseVmObjectHandle(this->handle);

    // release physical pages
    for(const auto &page : this->physOwned) {
        if(page.task) {
            __atomic_sub_fetch(&page.task->physPagesOwned, 1, __ATOMIC_RELAXED);
        }
        this->freePage(page.physAddr);
    }
}

/**
 * Allocates a VM map entry that refers to a contiguous range of physical memory.
 */
MapEntry *MapEntry::makePhys(const uint64_t physAddr, const uintptr_t address, const size_t length,
        const MappingFlags flags) {
    // allocate the bare map
    if(!gMapEntryAllocator) initAllocator();
    auto map = gMapEntryAllocator->alloc(address, length, flags);

    // set up the phys mapping
    map->physBase = physAddr;

    // done!
    return map;
}


/**
 * Allocates a new anonymous memory backed VM map entry.
 */
MapEntry *MapEntry::makeAnon(const uintptr_t address, const size_t length,
                const MappingFlags flags) {
    // allocate the bare map
    if(!gMapEntryAllocator) initAllocator();
    auto map = gMapEntryAllocator->alloc(address, length, flags);

    // set it up and fault in all pages if needed
    map->isAnon = true;

    // TODO: map pages

    // done
    return map;
}

/**
 * Frees a previously allocated VM map entry.
 */
void MapEntry::free(MapEntry *ptr) {
    gMapEntryAllocator->free(ptr);
}



/**
 * Resize the VM object.
 *
 * TODO: most of this needs to be implemented. should we support resizing shared mappings? this
 * adds a whole lot of complexity...
 */
int MapEntry::resize(const size_t newSize) {
    const auto pageSz = arch_page_size();

    // size must be page aligned
    if(newSize % pageSz) {
        return -1;
    } else if(!newSize) {
        return -1;
    }

    // take a lock on the entry
    RW_LOCK_WRITE_GUARD(this->lock);

    // are we shrinking the region?
    if(this->length > newSize) {
        // TODO: update mappings in all maps

        // release all physical memory for pages above the cutoff
        this->length = newSize;
        const auto endPageOff = newSize / pageSz;

        size_t i = 0;
        while(i < this->physOwned.size()) {
            const auto &info = this->physOwned[i];

            // if it lies beyond the end, release it
            if(info.pageOff >= endPageOff) {
                if(info.task) {
                    __atomic_sub_fetch(&info.task->physPagesOwned, 1, __ATOMIC_RELAXED);
                }

                this->freePage(info.physAddr);
                this->physOwned.remove(i);
            } 
            // otherwise, check next entry
            else {
                i++;
            }
        }

        // we've successfully resized the region
        return 0;
    }
    // no, it should be embiggened
    else {
        // check that the new size doesn't cause conflicts in maps we belong to
        for(auto &info : this->maps) {
            if(!info.mapPtr->canResize(this, info.base, this->length, newSize)) {
                return -1;
            }
        }

        // if no conflicts, we're good to perform resize
        this->length = newSize;
        return 0;
    }
}

/**
 * Handles a page fault for the given virtual address.
 *
 * @note As a precondition, the virtual address provided must fall in the range of this map.
 */
bool MapEntry::handlePagefault(Map *map, const uintptr_t address, const bool present,
        const bool write) {
    // only anonymous memory can be faulted in
    if(!this->isAnon) {
        return false;
    }

    // the page must be _not_ present
    if(present) {
        return false;
    }

    // fault it in
    this->faultInPage(address & ~0xFFF, map);

    return true;
}

/**
 * Faults in a page.
 */
void MapEntry::faultInPage(const uintptr_t address, Map *map) {
    RW_LOCK_WRITE_GUARD(this->lock);

    int err;
    const auto pageSz = arch_page_size();
  
    // get map specific base
    uintptr_t thisBase = 0;
    bool foundBase = false;
    auto mask = MappingFlags::None;

    for(auto &info : this->maps) {
        if(info.mapPtr == map) {
            thisBase = (info.base ? info.base : this->base);
            mask = info.flagsMask;
            foundBase = true;
            goto beach;
        }
    }

beach:;
    REQUIRE(foundBase, "failed to find base address");

    const auto pageOff = (address - thisBase) / pageSz; 

    // figure out the proper flags
    auto flg = this->flags;
    if(mask != MappingFlags::None) {
        flg &= ~MappingFlags::PermissionsMask;
        flg |= (flg & mask);
    }

    // check if we already own such a physical page (shared memory case)
    for(auto &physPage : this->physOwned) {
        if(physPage.pageOff == pageOff) {
            const auto destAddr = thisBase + (pageOff * pageSz);
            const auto mode = ConvertVmMode(flg);
            err = map->add(physPage.physAddr, pageSz, destAddr, mode);
            REQUIRE(!err, "failed to map page %d for map %p ($%08x'h)", pageOff, this, this->handle);

            return;
        }
    }

    // allocate physical memory and map it in
    const auto page = mem::PhysicalAllocator::alloc();
    REQUIRE(page, "failed to allocage physical page for %08x", address);

    auto task = sched::Task::current();
    if(task) {
        __atomic_add_fetch(&task->physPagesOwned, 1, __ATOMIC_RELEASE);
    }

    // insert page info
    AnonPageInfo info;
    info.physAddr = page;
    info.pageOff = pageOff;
    info.task = sched::Task::current();

    this->physOwned.push_back(info);

    // map it
    const auto destAddr = thisBase + (pageOff * pageSz);
    const auto mode = ConvertVmMode(flg);
    err = map->add(page, pageSz, destAddr, mode);
    REQUIRE(!err, "failed to map page %d for map %p ($%08x'h)", info.pageOff, this, this->handle);

    // invalidate the address
    asm volatile ( "invlpg (%0)" : : "b"(destAddr) : "memory" );

    // zero it
    memset(reinterpret_cast<void *>(destAddr), 0, pageSz);
}

/**
 * Frees a memory page belonging to this map.
 */
void MapEntry::freePage(const uintptr_t page) {
    mem::PhysicalAllocator::free(page);

    /*
     * Decrement the task's owned pages counter.
     *
     * This relies on callers being nice :) and not allocating pages in one task, then freeing
     * them in yet another task.
     */
    auto task = sched::Task::current();
    if(task) {
        __atomic_sub_fetch(&task->physPagesOwned, 1, __ATOMIC_RELEASE);
    }
}


/**
 * Updates the mapping's flags.
 *
 * This currently only affects it in new mappings and for new pages that are to be yeeted into
 * memory; it is a TODO that this works properly in shared memory situations, and updates the
 * permissions of existing mappings.
 *
 * @return 0 on success, a negative error code otherwise.
 */
int MapEntry::updateFlags(const MappingFlags newFlags) {
    RW_LOCK_WRITE_GUARD(this->lock);

    auto oldFlags = this->flags;
    oldFlags &= ~(MappingFlags::Read | MappingFlags::Write | MappingFlags::Execute);
    oldFlags &= ~(MappingFlags::MMIO);

    oldFlags |= newFlags;

    this->flags = oldFlags;

    return 0;
}


/**
 * Maps the address range of this entry.
 *
 * If it is backed by anonymous memory, we map all pages that have been faulted in so far;
 * otherwise, we map the entire region.
 */
void MapEntry::addedToMap(Map *map, sched::Task *task, const uintptr_t _base, const MappingFlags flagsMask) {
    RW_LOCK_READ(&this->lock);

    const auto baseAddr = (_base ? _base : this->base);
    REQUIRE(baseAddr, "failed to get base address for map entry %p", this);

    // map all allocated physical anon pages
    if(this->isAnon) {
        this->mapAnonPages(map, baseAddr, flagsMask);
    }
    // otherwise, map the whole thing
    else {
        this->mapPhysMem(map, baseAddr, flagsMask);
    }
    RW_UNLOCK_READ(&this->lock);

    // insert owner info
    RW_LOCK_WRITE(&this->lock);
    this->maps.push_back(MapInfo(map, task, baseAddr, flagsMask));
    RW_UNLOCK_WRITE(&this->lock);
}

/**
 * Maps all allocated physical pages.
 */
void MapEntry::mapAnonPages(Map *map, const uintptr_t base, const MappingFlags mask) {
    int err;

    const auto pageSz = arch_page_size();

    // convert flags
    auto flg = this->flags;
    if(mask != MappingFlags::None) {
        flg &= ~MappingFlags::PermissionsMask;
        flg |= (flg & mask);
    }

    const auto mode = ConvertVmMode(flg);

    // map the pages
    for(const auto &page : this->physOwned) {
        // calculate the address
        const auto vmAddr = base + (page.pageOff * pageSz);

        // map it
        err = map->add(page.physAddr, pageSz, vmAddr, mode);
        REQUIRE(!err, "failed to map vm object %p ($%08x'h) addr $%08x %d", this, this->handle,
                vmAddr, err);
    }
}

/**
 * Maps the entire underlying physical memory range.
 */
void MapEntry::mapPhysMem(Map *map, const uintptr_t base, const MappingFlags mask) {
    int err;

    // determine flags
    auto flg = this->flags;
    if(mask != MappingFlags::None) {
        flg &= ~MappingFlags::PermissionsMask;
        flg |= (flg & mask);
    }

    const auto mode = ConvertVmMode(flg);

    // insert the mapping
    err = map->add(this->physBase, this->length, base, mode);

    REQUIRE(!err, "failed to map vm object %p ($%08x'h) addr $%08x %d", this, this->handle,
            this->base, err);
}

struct RemoveCtxInfo {
    MapEntry *entry;
    Map *map;

    RemoveCtxInfo(MapEntry *_entry, Map *_map) : entry(_entry), map(_map) {};
};

/**
 * Unmaps the address range of this map entry.
 */
void MapEntry::removedFromMap(Map *map, sched::Task *task) {
    RemoveCtxInfo info(this, map);

    // iterate the maps info dict to get our true base address
    RW_LOCK_WRITE(&this->lock);
    size_t i = 0;
    while(i < this->maps.size()) {
        // ensure it's the map we're after
        if(this->maps[i].mapPtr != map) {
            i++;
            continue;
        }

        // unmap the range
        int err = map->remove(this->maps[i].base, this->length);
        REQUIRE(!err, "failed to unmap vm object: %d", err);

        // remove it
        this->maps.remove(i);

        // find the next first task that's mapped us still
        sched::Task *newOwner = nullptr;
        if(!this->maps.empty()) {
            newOwner = this->maps[0].task;
        }

        // transfer ownership of physical pages to the next task that maps us
        uintptr_t owned = 0;
        for(auto &physPage : this->physOwned) {
            if(physPage.task == task) {
                physPage.task = newOwner;
                owned++;
            }
        }

        __atomic_sub_fetch(&task->physPagesOwned, owned, __ATOMIC_RELAXED);
        if(newOwner) {
            __atomic_add_fetch(&newOwner->physPagesOwned, owned, __ATOMIC_RELAXED);
        }
    }

    RW_UNLOCK_WRITE(&this->lock);
}

/**
 * Gets info about this map entry from the perspective of a map.
 */
int MapEntry::getInfo(Map *map, uintptr_t &outBase, uintptr_t &outLength, MappingFlags &outFlags) {
    RW_LOCK_READ_GUARD(this->lock);

    // iterate the map infos
    for(const auto &info : this->maps) {
        if(info.mapPtr == map) {
            outBase = info.base;
            outLength = this->length;

            auto flg = this->flags;
            if(info.flagsMask != MappingFlags::None) {
                //log("Maskule %08x", (uintptr_t) info.flagsMask);

                flg &= ~MappingFlags::PermissionsMask;
                flg |= (flg & info.flagsMask);
            }

            //log("fuck %08x %08x %08x %08x %08x", info.base, (uintptr_t) this->flags, (uintptr_t) info.flagsMask, (uintptr_t) flg,
            //        info.base);

            outFlags = flg;

            return 0;
        }
    }

    // if we get here, we're not mapped by this map
    return -1;
}




/**
 * Converts map entry flags to those suitable for updating VM maps.
 */
static vm::MapMode ConvertVmMode(const MappingFlags flags) {
    vm::MapMode mode = vm::MapMode::ACCESS_USER;

    if(TestFlags(flags & MappingFlags::Read)) {
        mode |= vm::MapMode::READ;
    }
    if(TestFlags(flags & MappingFlags::Write)) {
        mode |= vm::MapMode::WRITE;
    }
    if(TestFlags(flags & MappingFlags::Execute)) {
        mode |= vm::MapMode::EXECUTE;
    }
    if(TestFlags(flags & MappingFlags::MMIO)) {
        mode |= vm::MapMode::CACHE_DISABLE;
    }

    return mode;
}
