#ifndef KERNEL_SYS_SYSCALL_H
#define KERNEL_SYS_SYSCALL_H

#include <stddef.h>
#include <stdint.h>

#include "Errors.h"

extern "C" void kernel_init();

namespace sys {
/**
 * Handles kernel syscalls.
 */
class Syscall {
    friend void ::kernel_init();

    public:
        /// Validates whether the entire range of [base, base+length) is accessible
        static bool validateUserPtr(const void *address, const size_t length = 0x1000);

        static inline bool validateUserPtr(const uintptr_t address, const size_t length = 0x1000) {
            return validateUserPtr(reinterpret_cast<const void *>(address), length);
        }

        /// Copy data in from userspace
        static void copyIn(const void * __restrict__ from, const size_t fromBytes, 
                void * __restrict__ to, const size_t toBytes);
        /// Copy data out to userspace
        static void copyOut(const void * __restrict__ from, const size_t fromBytes,
                void * __restrict__ to, const size_t toBytes);

        /// An invalid system call stub
        static intptr_t UnimplementedSyscall();

    private:
        static void init();

    private:
        static Syscall *gShared;
};
};

#endif
