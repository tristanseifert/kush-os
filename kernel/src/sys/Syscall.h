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
        /**
         * Contains arguments for a syscall. Each syscall can receive up to four arguments, which
         * are passed according to platform-specific convention.
         *
         * The platform syscall handler will marshal those arguments into this structure.
         */
        struct Args {
            uintptr_t args[4];
        };

    public:
        /// Invokes a syscall handler.
        static intptr_t handle(const Args *args, const uintptr_t callNo) {
            return gShared->_handle(args, callNo);
        }

        /// Validates whether the entire range of [base, base+length) is accessible
        static bool validateUserPtr(const void *address, const size_t length = 0x1000);

    private:
        static void init();
        intptr_t _handle(const Args *args, const uintptr_t code);

    private:
        static Syscall *gShared;
};
};

#endif
