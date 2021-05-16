#include "Handler.h"
#include "Syscalls.h"

#include "gdt.h"
#include <arch/x86_msr.h>
#include <log.h>
#include <new>

#include <platform.h>

#include <mem/PhysicalAllocator.h>
#include <vm/Mapper.h>
#include <vm/Map.h>
#include <sys/Syscall.h>

using namespace arch::syscall;

char gSharedBuf[sizeof(Handler)] __attribute__((aligned(64)));
Handler *Handler::gShared = nullptr;

extern "C" void arch_syscall_entry();


/**
 * Initializes the shared syscall handler.
 */
void Handler::init() {
    gShared = reinterpret_cast<Handler *>(&gSharedBuf);
    new(gShared) Handler();
}

/**
 * Initializes a syscall handler.
 *
 * We'll program the MSRs for the SYSCALL/SYSRET instruction. We don't program in compatibility
 * mode entry points since we simply don't support this.
 */
Handler::Handler() {
    int err;
    auto m = ::vm::Map::kern();

    // configure code segment and entry point
    const auto entryAddr = reinterpret_cast<uintptr_t>(&arch_syscall_entry);
    x86_msr_write(X86_MSR_IA32_STAR, 0, (GDT_USER_CODE_SEG << 16) | (GDT_KERN_CODE_SEG));
    x86_msr_write(X86_MSR_IA32_LSTAR, entryAddr, entryAddr >> 32);

    // mask IRQ, clear dir, trace flags, alignment check
    x86_msr_write(X86_MSR_IA32_FMASK, 0x4700, 0);

    // allocate the time page
    this->timePage = mem::PhysicalAllocator::alloc();
    REQUIRE(this->timePage, "failed to allocate time page");

    err = m->add(this->timePage & ~0xFFF, 0x1000, kTimeKernelVmAddr, ::vm::MapMode::kKernelRW);
    REQUIRE(!err, "failed to map time page: %d", err);

    this->timeInfo = reinterpret_cast<TimeInfo *>(kTimeKernelVmAddr);
}

/**
 * Maps the kernel time info page into the specified task.
 */
void Handler::mapTimePage(rt::SharedPtr<sched::Task> &task) {
    int err;

    // map the stub into the userspace
    auto map = task->vm;

    err = map->add(this->timePage & ~0xFFF, 0x1000, kTimeUserVmAddr, ::vm::MapMode::kUserRead);
    REQUIRE(!err, "failed to map time page into task %p (%s): %d", static_cast<void *>(task),
            task->name, err);
}

/**
 * Writes to the time page.
 */
void Handler::updateTime() {
    uint32_t low, high;

    // get time and read tsc
    auto now = platform_timer_now();

    asm volatile("lfence; rdtsc; lfence" : "=a"(low), "=d"(high) : : "memory");
    uint64_t tsc =  (static_cast<uint64_t>(high) << 32ULL) | low;

    // do the atomic writes
    __atomic_store(&this->timeInfo->timeNsec, &now, __ATOMIC_RELAXED);
    __atomic_store(&this->timeInfo->kernelTsc, &tsc, __ATOMIC_SEQ_CST);
}



/**
 * Maps the syscall stub page into the given task.
 */
void arch::TaskWillStart(rt::SharedPtr<sched::Task> &task) {
    Handler::taskCreated(task);
}



void arch::Tick() {
    Handler::gShared->updateTime();
}
