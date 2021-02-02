#include "AnonPool.h"
#include "PhysicalAllocator.h"

#include "vm/Mapper.h"
#include "vm/Map.h"

#include <arch.h>
#include <log.h>
#include <new>

using namespace mem;

static char gAllocatorBuf[sizeof(AnonPool)];
AnonPool *AnonPool::gShared = (AnonPool *) &gAllocatorBuf;

/**
 * Sets up the anon pool.
 */
void AnonPool::init() {
    int err;
    auto map = vm::Mapper::getKernelMap();

    // figure out how many pages we need
    const auto pageSz = arch_page_size();
    const auto numPages = (sizeof(AnonPool) + pageSz - 1) / pageSz;

    // allocate and map them at the beginning of the anon pool memory
    for(size_t i = 0; i < numPages; i++) {
        auto page = mem::PhysicalAllocator::alloc();
        REQUIRE(page, "failed to get phys page for anon pool struct");

        const auto virtAddr = kBaseAddr + (i * pageSz);
        err = map->add(page, pageSz, virtAddr, vm::MapMode::kKernelRW | vm::MapMode::GLOBAL);
        REQUIRE(!err, "failed to map anon pool: %d", err);
    }

    // allocate the pool there
    gShared = (AnonPool *) kBaseAddr;
    new(gShared) AnonPool(kBaseAddr + (numPages * pageSz));
}

/**
 * Initializes the anon pool. This just sets up the housekeeping for deciding what virtual
 * addresses are available.
 */
AnonPool::AnonPool(const uintptr_t allocBase) : nextAddr(allocBase) {
    log("%p base alloc %08lx", this, this->nextAddr);
    // TODO: stuff
}



/**
 * Gets a new page from the pool.
 */
void *AnonPool::alloc(uint64_t &physAddr) {
    int err;

    // get the physical page
    auto page = mem::PhysicalAllocator::alloc();
    if(!page) return nullptr;

    // map it into VM
    const auto virtAddr = this->nextAddr;
    this->nextAddr += arch_page_size();

    auto map = vm::Mapper::getKernelMap();
    err = map->add(page, arch_page_size(), virtAddr, vm::MapMode::kKernelRW | vm::MapMode::GLOBAL);
    if(err) {
        log("failed to map anon pool allocation: %d", err);
        return nullptr;
    }

    physAddr = page;
    return (void *) virtAddr;
}

/**
 * Releases a given page.
 */
void AnonPool::free(void *virtAddr) {
    // get the physical page that backs it

    // unmap

    // free physical page
}
