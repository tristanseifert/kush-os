#ifndef KERNEL_PLATFORM_UEFI_ARCH_PROCESSOR
#define KERNEL_PLATFORM_UEFI_ARCH_PROCESSOR

#ifndef ASM_FILE
#include <stddef.h>
#include <stdint.h>

namespace Platform::Amd64Uefi {
/**
 * Provide general processor management.
 */
class Processor {
    public:
        /**
         * Processor registers as pushed on to the stack in an exception handler
         */
        struct Regs {
            // General purpose registers
            uint64_t rax, rbx, rcx, rdx, rsi, rdi, rbp;
            uint64_t r8, r9, r10, r11, r12, r13, r14, r15;

            /**
             * Vector number
             *
             * Indicates the interrupt vector that caused this register state to be collected, or
             * -1 if there was no corresponding interrupt.
             */
            uint64_t irq;

            /**
             * Error code
             *
             * This is specified only for some exceptions; specifically, double fault, invalid TSS,
             * segment not present, stack segment fault, general protection fault, page fault,
             * alignment check, control protection exception, VMM communication exception, and
             * security exception.
             */
            uint64_t errorCode;
            /// Program counter
            uint64_t rip;
            /// Code segment
            uint64_t cs;
            /// CPU registers
            uint64_t rflags;
            /// Stack pointer
            uint64_t rsp;
            /// Stack segment
            uint64_t ss;

            static int Format(const Regs &state, char *out, const size_t outSize);
        } __attribute__((packed));

    public:
        /**
         * Disables interrupts and halts the processor.
         */
        [[noreturn]] static inline void HaltSelf() {
            for(;;) {
                asm volatile("cli; hlt" ::: "memory");
            }
        }

        [[noreturn]] static void HaltAll();
};
};

namespace Platform {
using Processor = Platform::Amd64Uefi::Processor;
using ProcessorState = Platform::Amd64Uefi::Processor::Regs;
}

#endif
#endif
