#include "Syscall.h"
#include "Handlers.h"

#include "vm/Map.h"
#include "sched/Task.h"
#include "sched/Thread.h"

#include <arch.h>

#include <new>
#include <log.h>

using namespace sys;

static uint8_t gSharedBuf[sizeof(Syscall)] __attribute__((aligned(64)));
Syscall *Syscall::gShared = nullptr;

/**
 * Global syscall table
 */
static int (* const gSyscalls[])(const Syscall::Args *, const uintptr_t) = {
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,

    // 0x10: yield remainder of CPU time
    [](auto, auto) {
        sched::Thread::yield();
        return 0;
    },
    // 0x11: sleeps the thread for the number of microseconds in arg0
    [](auto args, auto) {
        // validate args
        const uint64_t sleepNs = 1000ULL * args->args[0];
        if(!sleepNs) return 0;
        // call sleep handler
        sched::Thread::sleep(sleepNs);
        return 0;
    },

    // 0x30: Create task
    TaskCreate,
    // 0x31: Terminate task,
    TaskTerminate,
    // 0x32: Initialize task
    TaskInitialize,
    // 0x33: Set task name
    TaskSetName,
};
static const size_t gNumSyscalls = 32;

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
        return -1;
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
    log("buffer %p -> base %08x", address, base);

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
        if(!flags(mode & vm::MapMode::ACCESS_USER)) {
            log("invalid user ptr %p! (page %08x flags %d)", address, virtAddr, (int) mode);
            return false;
        }
    }

    // if we get here, the entire range was valid and user accessible
    return true;
}
