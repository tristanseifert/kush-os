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

static vm::MapMode ConvertVmMode(const MappingFlags flags, const bool isUser);

/**
 * Deleter that will release a map entry back to the appropriate allocation pool
 */
struct MapEntryDeleter {
    void operator()(MapEntry *obj) const {
        gMapEntryAllocator->free(obj);
    }
};

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
MapEntry::MapEntry(const size_t _length, const MappingFlags _flags) : length(_length),
    flags(_flags) {
    // XXX: the allocator (which makes the shared ptr) is responsible for allocating handle
}

/**
 * Deallocator for map entry
 */
MapEntry::~MapEntry() {
    // release handle
    handle::Manager::releaseVmObjectHandle(this->handle);

    // release physical pages
    for(const auto &page : this->physOwned) {
        if(auto task = page.task.lock()) {
            __atomic_sub_fetch(&task->physPagesOwned, 1, __ATOMIC_RELAXED);
        }

        this->freePage(page.physAddr);
    }
}

/**
 * Allocates a VM map entry that refers to a contiguous range of physical memory.
 */
rt::SharedPtr<MapEntry> MapEntry::makePhys(const uint64_t physAddr, const size_t length,
        const MappingFlags flags, const bool kernel) {
    // allocate the bare map
    if(!gMapEntryAllocator) initAllocator();
    auto entry = gMapEntryAllocator->alloc(length, flags);
    entry->isKernel = kernel;

    // set up the phys entryping
    entry->physBase = physAddr;

    // create shared ptr
    auto ptr = rt::SharedPtr<MapEntry>(entry, MapEntryDeleter());
    entry->handle = handle::Manager::makeVmObjectHandle(ptr);

    return ptr;
}


/**
 * Allocates a new anonymous memory backed VM map entry.
 */
rt::SharedPtr<MapEntry> MapEntry::makeAnon(const size_t length, const MappingFlags flags, const bool kernel) {
    // allocate the bare map
    if(!gMapEntryAllocator) initAllocator();
    auto entry = gMapEntryAllocator->alloc(length, flags);
    entry->isKernel = kernel;

    // set it up and fault in all pages if needed
    entry->isAnon = true;

    // TODO: map pages

    // create a shared ptr
    auto ptr = rt::SharedPtr<MapEntry>(entry, MapEntryDeleter());
    entry->handle = handle::Manager::makeVmObjectHandle(ptr);

    return ptr;
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
 * @note This does NOT update the size of the mapping windows in any dependant maps; if the entry
 * is shrunk, however, any mapped pages beyond the cutoff will immediately be unmapped from all
 * memory maps the entry occurs in.
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
        // TODO: Remove mapped pages above cutoff from all tasks that map us

        // release all physical memory for pages above the cutoff
        this->length = newSize;
        const auto endPageOff = newSize / pageSz;

        size_t i = 0;
        while(i < this->physOwned.size()) {
            const auto &info = this->physOwned[i];

            // if it lies beyond the end, release it
            if(info.pageOff >= endPageOff) {
                if(auto task = info.task.lock()) {
                    __atomic_sub_fetch(&task->physPagesOwned, 1, __ATOMIC_RELAXED);
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
        this->length = newSize;
        return 0;
    }
}

/**
 * Handles a page fault for the given virtual address.
 *
 * @param map Memory map of the faulting task
 * @param base Virtual base address of this VM object in the given map
 * @param offset Offset into this VM object, in bytes
 *
 * @note As a precondition, the virtual address provided must fall in the range of this map.
 */
bool MapEntry::handlePagefault(Map *map, const uintptr_t base, const uintptr_t offset,
        const bool present, const bool write) {
    // only anonymous memory can be faulted in
    if(!this->isAnon) {
        return false;
    }
    // the page must be _not_ present
    else if(present) {
        return false;
    }
    // offset musn't be past the end of the region (it was shrunk, but someone is mapping us still)
    else if(offset >= this->length) {
        return false;
    }

    // fault it in
    this->faultInPage(base, offset, map);

    return true;
}

/**
 * Faults in a page.
 */
void MapEntry::faultInPage(const uintptr_t base, const uintptr_t offset, Map *map) {
    RW_LOCK_WRITE_GUARD(this->lock);

    int err;
    const auto pageSz = arch_page_size();
    const auto pageOff = offset / pageSz; 

    // TODO: figure out the proper flags
    auto flg = this->flags;
    /*if(mask != MappingFlags::None) {
        flg &= ~MappingFlags::PermissionsMask;
        flg |= (flg & mask);
    }*/

    // check if we already own such a physical page (shared memory case)
    for(auto &physPage : this->physOwned) {
        if(physPage.pageOff == pageOff) {
            const auto destAddr = base + (pageOff * pageSz);
            const auto mode = ConvertVmMode(flg, !this->isKernel);
            err = map->add(physPage.physAddr, pageSz, destAddr, mode);
            REQUIRE(!err, "failed to map page %d for map %p ($%08x'h)", pageOff, this, this->handle);

            return;
        }
    }

    // allocate physical memory and map it in
    const auto page = mem::PhysicalAllocator::alloc();
    REQUIRE(page, "failed to allocage physical page for %p+%x", base, offset);

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
    const auto destAddr = base + (pageOff * pageSz);
    const auto mode = ConvertVmMode(flg, !this->isKernel);

    err = map->add(page, pageSz, destAddr, mode);
    REQUIRE(!err, "failed to map page %d for map %p ($%08x'h)", info.pageOff, this, this->handle);

    // invalidate TLB entry
    arch::InvalidateTlb(destAddr);

    // zero it (XXX: this should be done BEFORE it's mapped!)
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
 * Callback invoked when this entry is added to a VM map.
 *
 * If it is backed by anonymous memory, we map all pages that have been faulted in so far;
 * otherwise, we map the entire region.
 *
 * @param map Address space into which this VM object was inserted
 * @param task Task that owns the address space into which we've been inserted
 * @param baseAddress Virtual address for the mapping in this address space
 * @param flagsMask Flags to overwrite from the original mapping
 */
void MapEntry::addedToMap(Map *map, const rt::SharedPtr<sched::Task> &task, const uintptr_t baseAddr,
        const MappingFlags flagsMask) {
    RW_LOCK_READ(&this->lock);
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

    const auto mode = ConvertVmMode(flg, !this->isKernel);

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

    const auto mode = ConvertVmMode(flg, !this->isKernel);

    // insert the mapping
    err = map->add(this->physBase, this->length, base, mode);

    REQUIRE(!err, "failed to map vm object %p ($%p'h) %d", this, this->handle, err);
}

struct RemoveCtxInfo {
    MapEntry *entry;
    Map *map;

    RemoveCtxInfo(MapEntry *_entry, Map *_map) : entry(_entry), map(_map) {};
};



/**
 * Callback invoked after the VM object is removed from a memory map.
 *
 * Any mappings this object owns in the provided memory map are removed.
 *
 * @param map Memory map to which this VM object was added
 * @param task Task to which the memory map belongs
 * @param base Virtual base address of this VM object in the given task
 * @param length Total length of the mapped region in the memory map
 */
void MapEntry::removedFromMap(Map *map, const rt::SharedPtr<sched::Task> &task,
        const uintptr_t base, const size_t length) {
    RemoveCtxInfo info(this, map);

    // iterate the maps info dict to get our true base address
    RW_LOCK_WRITE(&this->lock);
    size_t i = 0;
    while(i < this->maps.size()) {
        // ensure it's the map we're after
        if(this->maps[i] != map) {
            i++;
            continue;
        }

        // unmap the range
        int err = map->remove(base, length);
        REQUIRE(!err, "failed to unmap vm object: %d", err);

        // remove it
        this->maps.remove(i);

        // TODO: find new task to transfer ownership of pages to
        /*sched::Task *newOwner = nullptr;

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
        }*/
    }

    RW_UNLOCK_WRITE(&this->lock);
}



/**
 * Converts map entry flags to those suitable for updating VM maps.
 */
static vm::MapMode ConvertVmMode(const MappingFlags flags, const bool isUser) {
    vm::MapMode mode = isUser ? vm::MapMode::ACCESS_USER : vm::MapMode::None;

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
