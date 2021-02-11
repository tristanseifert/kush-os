#ifndef KERNEL_SYS_SYSCALL_H
#define KERNEL_SYS_SYSCALL_H

#include <stdint.h>

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
        static int handle(const Args *args, const uintptr_t callNo) {
            return gShared->_handle(args, callNo);
        }

    private:
        static void init();
        int _handle(const Args *args, const uintptr_t code);

    private:
        static Syscall *gShared;
};
};

#endif
