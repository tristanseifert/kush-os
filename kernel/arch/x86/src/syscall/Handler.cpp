#include "Handler.h"

#include "gdt.h"
#include <arch/x86_msr.h>
#include <log.h>
#include <new>

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

    // configure code segment and entry point
    x86_msr_write(kSysenterCsMsr, GDT_KERN_CODE_SEG, 0);
    x86_msr_write(kSysenterEipMsr, reinterpret_cast<uintptr_t>(&arch_syscall_entry), 0);

    // allocate the syscall stub page and map it as RW into the kernel so we can initialize it
    this->stubPage = mem::PhysicalAllocator::alloc();
    REQUIRE(this->stubPage, "failed to allocate syscall stub page");

    auto m = ::vm::Map::kern();
    err = m->add(this->stubPage & ~0xFFF, 0x1000, kStubKernelVmAddr, ::vm::MapMode::kKernelRW);
    REQUIRE(!err, "failed to map syscall stub: %d", err);

    // copy over the code
    log("syscall stub %d bytes at %p", SIZE_OF_STUB, _binary_syscall_stub_start);
    memcpy((void *) kStubKernelVmAddr, _binary_syscall_stub_start, SIZE_OF_STUB);

    // TODO: unmap it from kernel again
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
void TaskWillStart(sched::Task *task) {
    Handler::taskCreated(task);
}

