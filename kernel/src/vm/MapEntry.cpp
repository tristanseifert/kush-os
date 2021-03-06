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

    // default owner is current task
    this->owner = sched::Task::current();
}

/**
 * Deallocator for map entry
 */
MapEntry::~MapEntry() {
    // release handle
    handle::Manager::releaseVmObjectHandle(this->handle);

    // release physical pages
    for(const auto info : this->pages) {
        this->freePage(*info);
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

    // set it up
    entry->isAnon = true;

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

        log("releasing all pages above offset %lu (new size %lu)", endPageOff, newSize);

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
    RW_LOCK_WRITE(&this->lock);
    this->faultInPage(base, offset, map, true);
    RW_UNLOCK_WRITE(&this->lock);

    return true;
}

/**
 * Faults in a page.
 *
 * @param runDetector When set, the sequence state detector state machine is run.
 *
 * @note You must hold the rwlock for the map entry when invoking the method.
 */
void MapEntry::faultInPage(const uintptr_t base, const uintptr_t offset, Map *map, const bool runDetector) {
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
    if(auto page = this->pages.findKey(pageOff)) {
        const auto destAddr = base + (pageOff * pageSz);
        const auto mode = ConvertVmMode(flg, !this->isKernel);

        err = map->add(page->physAddr, pageSz, destAddr, mode);
        REQUIRE(!err, "failed to map page %d for map %p ($%08x'h)", pageOff, this, this->handle);

        arch::InvalidateTlb(destAddr);
        return;
    }

    // allocate physical memory and map it in
    const auto page = mem::PhysicalAllocator::alloc();
    REQUIRE(page, "failed to allocage physical page for %p+%x", base, offset);

    auto task = sched::Task::current();
    if(task) {
        __atomic_add_fetch(&task->physPagesOwned, 1, __ATOMIC_RELEASE);
    }

    // handle sequence detection state machine
    if(runDetector) {
        this->detectFaultSequence(base, offset, map, pageOff);
    }

    // insert page info
    auto info = new AnonInfoLeaf(pageOff, page);
    this->pages.insert(info);

    // map it
    const auto destAddr = base + (pageOff * pageSz);
    const auto mode = ConvertVmMode(flg, !this->isKernel);

    err = map->add(page, pageSz, destAddr, mode);
    REQUIRE(!err, "failed to map page %d for map %p ($%08x'h)", info->pageOff, this, this->handle);

    // invalidate TLB entry
    arch::InvalidateTlb(destAddr);
}

/**
 * Runs a step of the page fault sequence detection machinery. This will wait for two consecutive
 * page faults with the same stride, then fault in one page. If the sequence continues, each fault
 * will allocate double the number of pages.
 */
void MapEntry::detectFaultSequence(const uintptr_t base, const uintptr_t offset, Map *map,
                const size_t pageOff) {
    const auto pageSz = arch_page_size();

    switch(this->seqState) {
        case SequenceDetectorState::Idle:
            this->lastFaultOffset = pageOff;
            this->lastFaultStride = 0;
            this->seqState = SequenceDetectorState::Detect1;
            break;

        case SequenceDetectorState::Detect1:
            // TODO: support negative offsets
            if(pageOff <= this->lastFaultOffset) {
                this->seqState = SequenceDetectorState::Idle;
                break;
            }
            // calculate stride
            this->lastFaultStride = pageOff - this->lastFaultOffset;
            this->lastFaultOffset = pageOff;
            this->seqState = SequenceDetectorState::Detect2;
            break;

        case SequenceDetectorState::Detect2:
            // TODO: support negative offsets
            if(pageOff <= this->lastFaultOffset) {
                this->seqState = SequenceDetectorState::Idle;
            }
            // calculate stride and map pages in
            else {
                size_t i{0};
                const auto stride = pageOff - this->lastFaultOffset;
                if(stride != this->lastFaultStride) {
                    this->numSeqFaults = 0;
                    this->seqState = SequenceDetectorState::Idle;
                    break;
                }
                this->lastFaultOffset = pageOff;
                this->numSeqFaults++;

                // figure out how many pages to fault in
                const auto numFault = (this->numSeqFaults < kMaxSequentialPrefault) ?
                    this->numSeqFaults : kMaxSequentialPrefault;

                for(i = 0; i < numFault; i++) {
                    // ensure it's in bounds
                    const size_t byteOff = (i + 1) * pageSz;
                    if((offset + byteOff) >= this->length) goto exit;

                    // yeet it out
                    this->faultInPage(base, offset + byteOff, map, false);
                }

exit:;
                // update the faulting info
                if(i == numFault) {
                    this->lastFaultOffset += numFault;
                }
                // part of the pages were after the end of the map; reset the sequence detector
                else {
                    this->numSeqFaults = 0;
                    this->seqState = SequenceDetectorState::Idle;
                    break;
                }
            }

            break;
    }
}

/**
 * Allocates physical memory for all pages this anonymous memory object maps.
 *
 * @note This will allocate physical memory for ALL pages, but won't actually map them yet. This
 * will overwrite any existing physical allocations.
 *
 * @return 0 if all pages were faulted in, a negative error code otherwise.
 */
int MapEntry::faultInAllPages() {
    REQUIRE(this->isAnon, "cannot fault in pages for non-anonymous memory");
    REQUIRE(this->pages.empty(), "can't fault in all pages, as there are already some allocated");

    // calculate how many pages to fault in
    const auto pageSz = arch_page_size();
    const auto numPages = this->length / pageSz;

    // allocate all the pages and insert the appropriate info structs
    for(size_t pageOff = 0; pageOff < numPages; pageOff++) {
        const auto page = mem::PhysicalAllocator::alloc();
        if(!page) return -1;

        /*auto task = sched::Task::current();
        if(task) {
            __atomic_add_fetch(&task->physPagesOwned, 1, __ATOMIC_RELEASE);
        }*/

        // TODO: zero page?

        // create the info struct
        auto info = new AnonInfoLeaf(pageOff, page);
        this->pages.insert(info);
    }

    return 0;
}

/**
 * Frees a memory page belonging to this map.
 */
void MapEntry::freePage(const AnonInfoLeaf &info) {
    mem::PhysicalAllocator::free(info.physAddr);

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

    const auto oldFlags = this->flags;

    // ensure we only update the permissions and cacheability fields
    auto flags = this->flags;
    flags &= ~MappingFlags::PermissionsMask;
    flags &= ~MappingFlags::CacheabilityMask;

    flags |= newFlags;

    this->flags = flags;

    // update mappings if flags actually changed
    if(oldFlags != flags) {
        this->updateExistingMappingFlags();
    }

    return 0;
}



/**
 * Updates the flags for any existing mappings.
 *
 * @note You must hold the lock to this entry when calling the function.
 */
void MapEntry::updateExistingMappingFlags() {
    for(const auto &view : this->mappedIn) {
        auto map = view.task->vm.get();

        if(this->isAnon) {
            this->mapAnonPages(map, view.base, view.flags, true);
        } else {
            this->mapPhysMem(map, view.base, view.flags, true);
        }
    }
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
    REQUIRE(baseAddr, "failed to get base address for map entry %p", this);

    // store the mapping info
    this->mappedIn.append(ViewInfo(task, baseAddr, flagsMask));

    // map all allocated physical anon pages
    RW_LOCK_WRITE_GUARD(this->lock);
    if(this->isAnon) {
        this->mapAnonPages(map, baseAddr, flagsMask, false);
    }
    // otherwise, map the whole thing
    else {
        this->mapPhysMem(map, baseAddr, flagsMask, false);
    }
}

/**
 * Maps all allocated physical pages.
 *
 * @param update Whether we're updating an existing mapping, or performing the initial mapping
 */
void MapEntry::mapAnonPages(Map *map, const uintptr_t base, const MappingFlags mask,
        const bool update) {
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
    for(const auto info : this->pages) {
        // calculate the address
        const auto vmAddr = base + (info->pageOff * pageSz);

        // map it
        err = map->add(info->physAddr, pageSz, vmAddr, mode);
        REQUIRE(!err, "failed to map vm object %p ($%08x'h) addr $%08x %d", this, this->handle,
                vmAddr, err);

        // flush TLB if not initial mapping
        if(update) {
            arch::InvalidateTlb(vmAddr);
        }
    }
}

/**
 * Maps the entire underlying physical memory range.
 *
 * @param update Whether we're updating an existing mapping, or performing the initial mapping
 */
void MapEntry::mapPhysMem(Map *map, const uintptr_t base, const MappingFlags mask,
        const bool update) {
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
    RW_LOCK_WRITE_GUARD(this->lock);

    // remove it from the provided map
    int err = map->remove(base, length);
    REQUIRE(!err, "failed to unmap vm object: %d", err);

    // remove the view info object
    this->mappedIn.removeMatching([](void *task, ViewInfo &view) -> bool {
        return (view.task.get() == task);
    }, task.get());

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
