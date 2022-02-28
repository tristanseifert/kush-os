#ifndef KERNEL_PLATFORM_UEFI_ARCH_EXCEPTIONHANDLERS_H
#define KERNEL_PLATFORM_UEFI_ARCH_EXCEPTIONHANDLERS_H

#include "Processor.h"

/// @{ @name Exception error types

/// Divide-by-zero
#define AMD64_EXC_DIVIDE                        (0x00|0x1000)
/// Debugging feature
#define AMD64_EXC_DEBUG                         (0x01|0x1000)
/// Non-maskable IRQ
#define AMD64_EXC_NMI                           (0x02|0x1000)
/// Breakpoint
#define AMD64_EXC_BREAKPOINT                    (0x03|0x1000)
/// Overflow
#define AMD64_EXC_OVERFLOW                      (0x04|0x1000)
/// Bounds check exceeded
#define AMD64_EXC_BOUNDS                        (0x05|0x1000)
/// Invalid opcode
#define AMD64_EXC_ILLEGAL_OPCODE                (0x06|0x1000)
/// Device unavailable (performing FPU instructions without FPU)
#define AMD64_EXC_DEVICE_UNAVAIL                (0x07|0x1000)
/// Double fault
#define AMD64_EXC_DOUBLE_FAULT                  (0x08|0x1000)
/// Invalid task state segment
#define AMD64_EXC_INVALID_TSS                   (0x0A|0x1000)
/// Segment not present
#define AMD64_EXC_SEGMENT_NP                    (0x0B|0x1000)
/// Stack segment fault
#define AMD64_EXC_SS                            (0x0C|0x1000)
/// General protection fault
#define AMD64_EXC_GPF                           (0x0D|0x1000)
/// Page fault
#define AMD64_EXC_PAGING                        (0x0E|0x1000)
/// x87 floating point exception
#define AMD64_EXC_FP                            (0x10|0x1000)
/// Alignment check
#define AMD64_EXC_ALIGNMENT                     (0x11|0x1000)
/// Machine check error
#define AMD64_EXC_MCE                           (0x12|0x1000)
/// SIMD floating point error
#define AMD64_EXC_SIMD_FP                       (0x13|0x1000)
/// Virtualization exception
#define AMD64_EXC_VIRT                          (0x14|0x1000)

/// @}

#ifndef ASM_FILE

namespace Platform::Amd64Uefi {
class Idt;

/**
 * @brief Platform specific exception dispatcher (and default handlers)
 *
 * This includes the intermediary dispatching logic, any exceptions that don't get dispatched, and
 * the assembly stubs.
 */
class ExceptionHandlers {
    public:
        static void Install(Idt &);

        static const char *GetExceptionName(const uint32_t vector, const bool allowNull = false);

    private:
        static void Handle(Processor::Regs &state);
};
}

#endif

#endif
