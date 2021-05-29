#include "init.h"
#include "mem/PhysicalAllocator.h"
#include "mem/StackPool.h"
#include "mem/Heap.h"
#include "vm/Mapper.h"
#include "vm/Map.h"
#include "sched/Scheduler.h"
#include "sched/Task.h"
#include "sys/Syscall.h"
#include "handle/Manager.h"

#include <arch.h>
#include <platform.h>
#include <stdint.h>

#include <version.h>
#include <log.h>

static void PrintBanner();

/// root init server
rt::SharedPtr<sched::Task> gRootServer;

/**
 * Early kernel initialization
 */
void kernel_init() {
    // set up the physical allocator and VM system
    mem::PhysicalAllocator::init();

    vm::Mapper::init();

    vm::Mapper::loadKernelMap();
    vm::Mapper::lateInit();

    log("VM: activated kernel map");

    // set up heap and expand physical allocator pool, as well as other runtime stuff
    mem::Heap::init();
    mem::StackPool::init();

    mem::PhysicalAllocator::vmAvailable();

    handle::Manager::init();
    sys::Syscall::init();

    // notify architecture we've got VM
    arch_vm_available();

    // set up scheduler (platform code may set up threads)
    sched::Scheduler::Init();

    // notify other components of VM availability
    platform_vm_available();
}

/**
 * After all systems have been initialized, we'll jump here. Higher level kernel initialization
 * takes place and we go into the scheduler.
 */
void kernel_main() {
    PrintBanner();

    // kernel is initialized. launch the root server
    gRootServer = platform_init_rootsrv();

    // invoke the scheduler
    sched::Scheduler::get()->run();
    panic("scheduler returned, this should never happen");
}

static void PrintBanner() {
    // get first 8 chars of hash
    char hash[9]{0};
    memcpy(hash, gVERSION_HASH, 8);

    // print
    log("Starting kush-os (build %s) - "
        "Copyright 2021 Tristan Seifert", hash);
}
