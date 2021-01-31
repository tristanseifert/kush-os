#include "init.h"
#include "mem/PhysicalAllocator.h"
#include "vm/Mapper.h"

#include <stdint.h>

#include <printf.h>
#include <log.h>

static void *get_pc () { return __builtin_return_address(0); }

/**
 * Early kernel initialization
 */
void kernel_init() {
    // set up the physical allocator and VM system
    mem::PhysicalAllocator::init();

    vm::Mapper::init();
    mem::PhysicalAllocator::vmAvailable();
    vm::Mapper::loadKernelMap();
}

/**
 * After all systems have been initialized, we'll jump here. Higher level kernel initialization
 * takes place and we go into the scheduler.
 */
void kernel_main() {
    log("kush time: PC = $%p", get_pc());

    auto page1 = mem::PhysicalAllocator::alloc();
    auto page2 = mem::PhysicalAllocator::alloc();
    log("Allocated page1: $%p, page2: $%p", (void *) page1, (void *) page2);

    mem::PhysicalAllocator::free(page1);

    page1 = mem::PhysicalAllocator::alloc();
    log("Allocated page1: $%p, page2: $%p", (void *) page1, (void *) page2);

    // should never get here
    while(1) {}
}
