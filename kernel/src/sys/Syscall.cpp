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
 * Initializes the syscall handler.
 */
void Syscall::init() {
    gShared = reinterpret_cast<Syscall *>(&gSharedBuf);
    new(gShared) Syscall();
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
    log("buffer %p -> base %p", address, base);
#endif

    // check each page referenced by the buffer
    for(size_t i = 0; i < (length / pageSz); i++) {
        const auto virtAddr = base + (i * pageSz);
        err = vm->get(virtAddr, physAddr, mode);

        if(err) {
            // 1 = no page, -1 = error codes
            log("invalid user ptr %p! (page %llx err %d)", address, virtAddr, err);
            return false;
        }

        // the page is mapped. ensure the user flag is set
        if(!TestFlags(mode & vm::MapMode::ACCESS_USER)) {
            log("invalid user ptr %p! (page %llx flags %d)", address, virtAddr, (int) mode);
            return false;
        }
    }

    // if we get here, the entire range was valid and user accessible
    return true;
}

/**
 * Copies data from userspace.
 *
 * TODO: implement actual validation and SMAP support
 */
void Syscall::copyIn(const void *from, const size_t fromBytes, void *to, const size_t toBytes) {
    REQUIRE((reinterpret_cast<uintptr_t>(from) + fromBytes) < vm::kKernelVmBound, "%s(%p, %d, %p, %d)",
            "copyin", from, fromBytes, to, toBytes);

    memcpy(to, from, (toBytes > fromBytes) ? fromBytes : toBytes);
}
/**
 * Copies data to userspace.
 *
 * TODO: implement actual validation and SMAP support
 */
void Syscall::copyOut(const void *from, const size_t fromBytes, void *to, const size_t toBytes) {
    REQUIRE((reinterpret_cast<uintptr_t>(to) + toBytes) < vm::kKernelVmBound, "%s(%p, %d, %p, %d)",
            "copyout", from, fromBytes, to, toBytes);

    memcpy(to, from, (toBytes > fromBytes) ? fromBytes : toBytes);
}


/**
 * Stub for an unimplemented syscall
 */
intptr_t Syscall::UnimplementedSyscall() {
    return Errors::InvalidSyscall;
}

