#include "MapEntry.h"
#include "Map.h"
#include "Mapper.h"

#include "mem/PhysicalAllocator.h"
#include "mem/SlabAllocator.h"

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
        mem::PhysicalAllocator::free(page.physAddr);
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

    // take al ock on the entry
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
                mem::PhysicalAllocator::free(info.physAddr);
                this->physOwned.remove(i);
            } 
            // otherwise, check next entry
            else {
                i++;
            }
        }
    }
    // no, it should be embiggened
    else {
        // TODO: check that this will succeed in all maps

        return -1;
    }

    // if we get here, the region was successfully resized
    return 0;
}

/**
 * Handles a page fault for the given virtual address.
 *
 * @note As a precondition, the virtual address provided must fall in the range of this map.
 */
bool MapEntry::handlePagefault(const uintptr_t address, const bool present, const bool write) {
    // only anonymous memory can be faulted in
    if(!this->isAnon) {
        return false;
    }

    // the page must be _not_ present
    if(present) {
        return false;
    }

    // fault it in
    this->faultInPage(address & ~0xFFF, Map::current());

    return true;
}

/**
 * Faults in a page.
 */
void MapEntry::faultInPage(const uintptr_t address, Map *map) {
    RW_LOCK_WRITE_GUARD(this->lock);

    int err;
    const auto pageSz = arch_page_size();

    // allocate physical memory and map it in
    const auto page = mem::PhysicalAllocator::alloc();
    REQUIRE(page, "failed to allocage physical page for %08x", address);

    // insert page info
    AnonPageInfo info;
    info.physAddr = page;
    info.pageOff = (address - this->base) / pageSz;

    this->physOwned.push_back(info);

    // map it
    const auto mode = ConvertVmMode(this->flags);
    err = map->add(page, pageSz, address, mode);
    REQUIRE(!err, "failed to map page %d for map %p ($%08x'h)", info.pageOff, this, this->handle);
}



/**
 * Maps the address range of this entry.
 *
 * If it is backed by anonymous memory, we map all pages that have been faulted in so far;
 * otherwise, we map the entire region.
 */
void MapEntry::addedToMap(Map *map) {
    RW_LOCK_READ_GUARD(this->lock);

    // map all allocated physical anon pages
    log("mapping %p to %p", this, map);
    if(this->isAnon) {
        this->mapAnonPages(map);
    }
    // otherwise, map the whole thing
    else {
        this->mapPhysMem(map);
    }
}

/**
 * Maps all allocated physical pages.
 */
void MapEntry::mapAnonPages(Map *map) {
    int err;

    const auto pageSz = arch_page_size();

    // convert flags
    const auto mode = ConvertVmMode(this->flags);

    // map the pages
    for(const auto &page : this->physOwned) {
        // calculate the address
        const auto vmAddr = this->base + (page.pageOff * pageSz);

        // map it
        err = map->add(page.physAddr, pageSz, vmAddr, mode);
        REQUIRE(!err, "failed to map vm object %p ($%08x'h) addr $%08x %d", this, this->handle,
                vmAddr, err);
    }
}

/**
 * Maps the entire underlying physical memory range.
 */
void MapEntry::mapPhysMem(Map *map) {
    int err;

    const auto mode = ConvertVmMode(this->flags);
    err = map->add(this->physBase, this->length, this->base, mode);

    REQUIRE(!err, "failed to map vm object %p ($%08x'h) addr $%08x %d", this, this->handle,
            this->base, err);
}

/**
 * Unmaps the address range of this map entry.
 */
void MapEntry::removedFromMap(Map *map) {
    int err;

    // simply unmap the entire range
    err = map->remove(this->base, this->length);
    REQUIRE(!err, "failed to unmap vm object %p ($%08x'h): %d", this, this->handle, err);
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
