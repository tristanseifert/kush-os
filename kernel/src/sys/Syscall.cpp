#include "Syscall.h"
#include "Handlers.h"

#include "vm/Map.h"
#include "sched/Task.h"
#include "sched/Thread.h"

#include <arch.h>

#include <new>
#include <log.h>

using namespace sys;

// if set, logging about each user pointer validation is performed
#define LOG_PTR_VALIDATE                0

static uint8_t gSharedBuf[sizeof(Syscall)] __attribute__((aligned(64)));
Syscall *Syscall::gShared = nullptr;

/**
 * Global syscall table
 */
static int (* const gSyscalls[])(const Syscall::Args *, const uintptr_t) = {
    // 0x00-0x07: Message passing IPC
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    // 0x08-0x0F: Notifications
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

    // 0x10: Create VM region
    VmAlloc,
    // 0x11: Create anonymous VM region
    VmAllocAnon,
    // 0x12: Destroy VM region
    nullptr,
    // 0x13: Update VM region permissions
    VmRegionUpdatePermissions,
    // 0x14: Resize VM region
    VmRegionResize,
    // 0x15: Map VM region
    VmRegionMap,
    // 0x16: Unmaps VM region
    VmRegionUnmap,

    // 0x17-0x1F: VM calls (reserved)
    nullptr, 
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

    // 0x20: Return current thread handle
    [](auto, auto) -> int {
        auto thread = sched::Thread::current();
        if(!thread) {
            return Errors::GeneralError;
        }
        return static_cast<int>(thread->handle);
    },
    // 0x21: Give up remaining CPU quantum
    [](auto, auto) -> int {
        sched::Thread::yield();
        return Errors::Success;
    },
    // 0x22: Microsecond granularity sleep
    [](auto args, auto) -> int {
        // validate args
        const uint64_t sleepNs = 1000ULL * args->args[0];
        if(!sleepNs) return Errors::InvalidArgument;
        // call sleep handler
        sched::Thread::sleep(sleepNs);
        return Errors::Success;
    },
    // 0x23: Create thread
    ThreadCreate,
    // 0x24: Join with thread
    nullptr,
    // 0x25: Destroy thread
    ThreadDestroy,
    // 0x26: Set thread state 
    nullptr,
    // 0x27: Set thread priority 
    ThreadSetPriority,
    // 0x28: Set thread notification mask
    ThreadSetNoteMask,
    // 0x29: Set thread name
    ThreadSetName,
    // 0x2A-0x2F: reserved
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

    // 0x30: Get task handle
    [](auto, auto) -> int {
        auto thread = sched::Thread::current();
        if(!thread || !thread->task) {
            return Errors::GeneralError;
        }
        return static_cast<int>(thread->task->handle);
    },
    // 0x31: Create task
    TaskCreate,
    // 0x32: Terminate task,
    TaskTerminate,
    // 0x33: Initialize task
    TaskInitialize,
    // 0x34: Set task name
    TaskSetName,
    // 0x35: Wait on task
    nullptr,
    // 0x36: Debug printing
    TaskDbgOut,
};
static const size_t gNumSyscalls = 0x37;

/**
 * Initializes the syscall handler.
 */
void Syscall::init() {
    gShared = reinterpret_cast<Syscall *>(&gSharedBuf);
    new(gShared) Syscall();
}

/**
 * Handles a syscall that wasn't handled by fast path code, nor platform or architecture code.
 */
int Syscall::_handle(const Args *args, const uintptr_t code) {
    // validate syscall number
    const size_t syscallIdx = (code & 0xFFFF);
    if(syscallIdx >= gNumSyscalls) {
        return Errors::InvalidSyscall;
    }

    // invoke it
    return gSyscalls[syscallIdx](args, code);
}



/**
 * Validates a pointer provided by the calling task for a syscall. This will perform the following
 * checks:
 *
 * 1. Reject all pointers in the kernel's region of the virtual memory space. We check this by
 *    validating that all page tables have the user access bit set.
 * 2. Ensure the page(s) referenced are allocated. (They do not need to be resident).
 *
 * If ANY of these checks fail, the pointer is invalid. You should NEVER dereference an user
 * pointer before this function is called AND indicated validity!
 */
bool Syscall::validateUserPtr(const void *address, const size_t _length) {
    int err;
    uint64_t physAddr;
    vm::MapMode mode;

    // get the VM map of the calling task
    auto vm = sched::Thread::current()->task->vm;

    // round up length to a multiple of page size
    const auto pageSz = arch_page_size();
    uintptr_t length = _length;
    if(length % pageSz) {
        length += pageSz - (length % pageSz);
    }

    const uintptr_t base = reinterpret_cast<uintptr_t>(address) & ~(pageSz - 1);
#if LOG_PTR_VALIDATE
    log("buffer %p -> base %08x", address, base);
#endif

    // check each page referenced by the buffer
    for(size_t i = 0; i < (length / pageSz); i++) {
        const auto virtAddr = base + (i * pageSz);
        err = vm->get(virtAddr, physAddr, mode);

        if(err) {
            // 1 = no page, -1 = error codes
            log("invalid user ptr %p! (page %08x err %d)", address, virtAddr, err);
            return false;
        }

        // the page is mapped. ensure the user flag is set
        if(!TestFlags(mode & vm::MapMode::ACCESS_USER)) {
            log("invalid user ptr %p! (page %08x flags %d)", address, virtAddr, (int) mode);
            return false;
        }
    }

    // if we get here, the entire range was valid and user accessible
    return true;
}
