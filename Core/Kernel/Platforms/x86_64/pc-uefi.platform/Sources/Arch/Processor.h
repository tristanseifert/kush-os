#ifndef KERNEL_PLATFORM_UEFI_ARCH_PROCESSOR
#define KERNEL_PLATFORM_UEFI_ARCH_PROCESSOR

#ifndef ASM_FILE
#include <stddef.h>
#include <stdint.h>

#include <Intrinsics.h>

namespace Platform::Amd64Uefi {
/**
 * @brief Amd64 processor stuff
 */
class Processor {
    public:
        /**
         * @brief Address for model-specific registers
         *
         * There are many more MSRs than are defined here; these are just the set of MSRs that we
         * use internally to make things go.
         */
        enum Msr: uint32_t {
            /// Extended feature enable register
            EFER                        = 0xC0000080,

            /// Ring 0 and 3 segment bases
            STAR                        = 0xC0000081,
            /// Program counter to load for 64-bit SYSCALL entry
            LSTAR                       = 0xC0000082,
            /// Program counter to load for compatibility mode SYSCALL entry
            CSTAR                       = 0xC0000083,
            /// Low 32 bits indicates which bits to mask off in RFLAGS
            FMASK                       = 0xC0000084,

            /// Base of %fs segment
            FSBase                      = 0xC0000100,
            /// Base of %gs segment
            GSBase                      = 0xC0000101,
            /// Kernel %gs base (for use with swapgs)
            KernelGSBase                = 0xC0000102,
        };

        /**
         * @brief All processor registers pushed on to the stack in an exception handler
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
            static int Backtrace(const Regs &state, char *out, const size_t outSize);

            /// Return the program counter value.
            constexpr inline auto getPc() const {
                return this->rip;
            }
            /// Return a reference to the program counter value.
            constexpr inline uint64_t &getPc() {
                return this->rip;
            }
        } KUSH_PACKED;

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

        static void VerifyFeatures();
        static void InitFeatures();

        /**
         * Read a model-specific register.
         */
        static inline void ReadMsr(const Msr msr, uint32_t &lo, uint32_t &hi) {
            asm volatile("rdmsr" : "=a"(lo), "=d"(hi) : "c"(msr));
        }

        /**
         * Write a model-specific register.
         */
        static inline void WriteMsr(const Msr msr, const uint32_t lo, const uint32_t hi) {
            asm volatile("wrmsr" : : "a"(lo), "d"(hi), "c"(msr));
        }
};
};

#ifndef DOXYGEN_SHOULD_SKIP_THIS
namespace Platform {
using Processor = Platform::Amd64Uefi::Processor;
using ProcessorState = Platform::Amd64Uefi::Processor::Regs;
}
#endif

#endif
#endif
