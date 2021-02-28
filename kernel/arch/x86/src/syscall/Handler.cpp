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

extern "C" char _binary_syscall_stub_start[], _binary_syscall_stub_end[];
#define SIZE_OF_STUB (_binary_syscall_stub_end - _binary_syscall_stub_start)

extern "C" void arch_syscall_entry();

/// Total number of architecture-specific syscalls
static const size_t gNumArchSyscalls = 2;
static int (* const gArchSyscalls[])(const sys::Syscall::Args *, const uintptr_t) = {
    // 0x00: Update task IO permissions
    UpdateTaskIopb,
    // 0x01: Update a thread's thread-local base address
    UpdateThreadTlsBase,
};


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
 * We'll program the MSRs to make use of the SYSENTER/SYSEXIT instructions for fast syscalls. This
 * will land in the fast assembly stubs (in entry.S) so we can do really fast message passing, or
 * for a real syscall, fall back into the C++ "slow path."
 */
Handler::Handler() {
    int err;
    auto m = ::vm::Map::kern();

    // configure code segment and entry point
    x86_msr_write(kSysenterCsMsr, GDT_KERN_CODE_SEG, 0);
    x86_msr_write(kSysenterEipMsr, reinterpret_cast<uintptr_t>(&arch_syscall_entry), 0);

    // allocate the syscall stub page and map it as RW into the kernel so we can initialize it
    this->stubPage = mem::PhysicalAllocator::alloc();
    REQUIRE(this->stubPage, "failed to allocate syscall stub page");

    err = m->add(this->stubPage & ~0xFFF, 0x1000, kStubKernelVmAddr, ::vm::MapMode::kKernelRW);
    REQUIRE(!err, "failed to map syscall stub: %d", err);

    // copy over the code
    memset((void *) kStubKernelVmAddr, 0, 0x1000);
    memcpy((void *) kStubKernelVmAddr, _binary_syscall_stub_start, SIZE_OF_STUB);

    // TODO: unmap it from kernel again

    // allocate the time page
    this->timePage = mem::PhysicalAllocator::alloc();
    REQUIRE(this->timePage, "failed to allocate time page");

    err = m->add(this->timePage & ~0xFFF, 0x1000, kTimeKernelVmAddr, ::vm::MapMode::kKernelRW);
    REQUIRE(!err, "failed to map time page: %d", err);

    this->timeInfo = reinterpret_cast<TimeInfo *>(kTimeKernelVmAddr);
}

/**
 * Maps the kernel syscall stub into the specified task.
 */
void Handler::mapSyscallStub(sched::Task *task) {
    int err;

    // map the stub into the userspace
    auto map = task->vm;

    err = map->add(this->stubPage & ~0xFFF, 0x1000, kStubUserVmAddr, ::vm::MapMode::kUserExec);
    REQUIRE(!err, "failed to map syscall stub into task %p (%s): %d", task, task->name, err);
}
/**
 * Maps the kernel time info page into the specified task.
 */
void Handler::mapTimePage(sched::Task *task) {
    int err;

    // map the stub into the userspace
    auto map = task->vm;

    err = map->add(this->timePage & ~0xFFF, 0x1000, kTimeUserVmAddr, ::vm::MapMode::kUserRead);
    REQUIRE(!err, "failed to map time page into task %p (%s): %d", task, task->name, err);
}

/**
 * Writes to the time page.
 */
void Handler::updateTime() {
    // get current time in nsec, and break into seconds and nsec component
    const auto now = platform_timer_now();

    uint32_t secs = now / 1000000000ULL;
    uint32_t nsec = now % 1000000000ULL;

    // do the atomic writes
    __atomic_store(&this->timeInfo->timeSecs, &secs, __ATOMIC_RELAXED);
    __atomic_store(&this->timeInfo->timeNsec, &nsec, __ATOMIC_SEQ_CST);
    __atomic_store(&this->timeInfo->timeSecs2, &secs, __ATOMIC_RELAXED);
}



/**
 * Handles the given syscall.
 */
uintptr_t arch_syscall_handle(const uintptr_t number, const void *_args) {
    const auto args = reinterpret_cast<const sys::Syscall::Args *>(_args);

    // handle platform specific syscalls

    // handle HW specific syscalls

    // kernel syscalls
    return sys::Syscall::handle(args, number);
    return 0;
}

/**
 * Slow path for the msgsend-family of functions.
 */
uintptr_t arch_syscall_msgsend_slow(const uintptr_t type) {
    log("Slow path for msgsend (type %08x)", type);
    return -1;
}



/**
 * Maps the syscall stub page into the given task.
 */
void arch::TaskWillStart(sched::Task *task) {
    Handler::taskCreated(task);
}



/**
 * Dispatch an architecture-specific system call.
 */
int arch::HandleSyscall(const sys::Syscall::Args *args, const uintptr_t _number) {
    // validate the arch specific syscall number
    const auto index = (_number & 0xFFFF0000) >> 16;
    if(index > gNumArchSyscalls) {
        return -5; // invalid syscall
    }

    // dispatch it
    return (gArchSyscalls[index])(args, _number);
}

void arch::Tick() {
    Handler::gShared->updateTime();
}
