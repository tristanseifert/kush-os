#ifndef KERNEL_PLATFORM_UEFI_ARCH_PROCESSOR
#define KERNEL_PLATFORM_UEFI_ARCH_PROCESSOR

namespace Platform::Amd64Uefi {
/**
 * Provide general processor management.
 */
class Processor {
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
}

#endif
