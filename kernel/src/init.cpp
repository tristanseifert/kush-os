#include "init.h"
#include "mem/AnonPool.h"
#include "mem/PhysicalAllocator.h"
#include "mem/StackPool.h"
#include "mem/Heap.h"
#include "vm/Mapper.h"
#include "vm/Map.h"
#include "sched/Scheduler.h"
#include "sched/Task.h"
#include "sys/Syscall.h"
#include "handle/Manager.h"

#include "sched/Thread.h"
#include "sched/IdleWorker.h"

#include <arch.h>
#include <platform.h>
#include <stdint.h>

#include <version.h>
#include <printf.h>
#include <log.h>

static void ThreadThymeCock(uintptr_t);

/// root init server
sched::Task *gRootServer = nullptr;

/**
 * Early kernel initialization
 */
void kernel_init() {
    *((volatile uint16_t *) 0xb8010) = 0x5148;
    
    // set up the physical allocator and VM system
    mem::PhysicalAllocator::init();

    vm::Mapper::init();
    mem::PhysicalAllocator::vmAvailable();

    vm::Mapper::loadKernelMap();
    vm::Mapper::lateInit();

    mem::AnonPool::init();
    mem::StackPool::init();
    mem::Heap::init();

    handle::Manager::init();
    sys::Syscall::init();

    // notify architecture we've got VM
    arch_vm_available();

    // set up scheduler (platform code may set up threads)
    sched::Scheduler::init();

    // notify other components of VM availability
    platform_vm_available();

    // test map the first 4K of VGA memory
#if 1
    auto m = vm::Map::kern();
    int err = m->add(0xb8000, 0x1000, 0xfc000000, vm::MapMode::kKernelRW);
    REQUIRE(!err, "failed to map vga: %d", err);
#endif
}

/**
 * After all systems have been initialized, we'll jump here. Higher level kernel initialization
 * takes place and we go into the scheduler.
 */
void kernel_main() {
    log("kush (%s) starting", gVERSION_SHORT);

    auto task2 = sched::Task::alloc();
    auto thread8 = sched::Thread::kernelThread(task2, &ThreadThymeCock, 0);
    thread8->setName("thyme caulk");

    sched::Scheduler::get()->scheduleRunnable(task2);

    *((uint32_t *) 0xfc000000) = 0x04410E41;

    // kernel is initialized. launch the root server
    gRootServer = platform_init_rootsrv();

    // invoke the scheduler
    sched::Scheduler::get()->run();
    panic("scheduler returned, this should never happen");
}

static void ThreadThymeCock(uintptr_t) {
    static char line[300] = {0};

    uintptr_t i = 0;
    auto last = platform_timer_now();
    uint64_t lastIdle = 0;

    static uint16_t *base = (uint16_t *) 0xfc0000a0;

    while(1) {
        const auto nsec = platform_timer_now();
        const auto since = nsec - last;
        const auto idle =sched::Scheduler::get()->idle->thread->cpuTime; 
        last = nsec;

        const auto idleDiff = idle - lastIdle;
        lastIdle = idle;

        const float idlePct = (((double) idleDiff) / ((double) since)) * 100.;

        // clear screen
        memset(base, 0, sizeof(uint16_t) * 80 * 24);

        // idle state
        const uint64_t secs = (nsec / (1000ULL * 1000ULL * 1000ULL));
        int written = snprintf(line, 300, "o'clock %16llu ns (%02lld:%02lld) yeet %7gmS "
                "idle %g sec (%5.2g %%)\nPhys alloc: %8uK / %8uK (%6.3f%% used)\n", nsec,
                secs / 60, secs % 60, ((float) since / (1000. * 1000.)),
                ((float) idle / (1000. * 1000. * 1000.)), idlePct,
                mem::PhysicalAllocator::getAllocPages() * 4,
                mem::PhysicalAllocator::getTotalPages() * 4,
                ((double) mem::PhysicalAllocator::getAllocPages()) / 
                ((double) mem::PhysicalAllocator::getTotalPages()) * 100.
                );

        static size_t x = 0, y = 0;
        x = y = 0;

        for(int i = 0; i < written; i++) {
            if(line[i] == '\n') {
                x = 0;
                y++;
                continue;
            }

            base[x + (y * 80)] = 0x0700 | line[i];
            x++;
        }

        // each task
        sched::Scheduler::get()->iterateTasks([](sched::Task *task) {
            // task header
            int written = snprintf(line, 300, "* Task %4u %08x %24s %8uK\n", task->pid,
                    task->handle, task->name, (task->physPagesOwned * arch_page_size()) / 1024);

            for(int i = 0; i < written; i++) {
                if(line[i] == '\n') {
                    x = 0;
                    y++;
                    continue;
                }

                base[x + (y * 80)] = 0x0700 | line[i];
                x++;
            }

            // now write each thread
            for(auto thread : task->threads) {
                written = snprintf(line, 300, "  x %08x %24s %u %6.2f %08x\n", thread->handle,
                        thread->name, (uintptr_t) thread->state,
                        ((float) thread->cpuTime / (1000. * 1000. * 1000.)), thread->lastSyscall);

                for(int i = 0; i < written; i++) {
                    if(line[i] == '\n') {
                        x = 0;
                        y++;
                        continue;
                    }

                    base[x + (y * 80)] = 0x0700 | line[i];
                    x++;
                }
            }
        });

        // yeet
        i++;
        sched::Thread::sleep(100 * 1000 * 1000);
        //sched::Thread::yield();
    }
}
