#ifndef KERNEL_PLATFORM_UEFI_ARCH_EXCEPTIONHANDLERS_H
#define KERNEL_PLATFORM_UEFI_ARCH_EXCEPTIONHANDLERS_H

#include "Processor.h"

/// Divide-by-zero
#define AMD64_EXC_DIVIDE                        0x00
/// Debugging feature
#define AMD64_EXC_DEBUG                         0x01
/// Non-maskable IRQ
#define AMD64_EXC_NMI                           0x02
/// Breakpoint
#define AMD64_EXC_BREAKPOINT                    0x03
/// Overflow
#define AMD64_EXC_OVERFLOW                      0x04
/// Bounds check exceeded
#define AMD64_EXC_BOUNDS                        0x05
/// Invalid opcode
#define AMD64_EXC_ILLEGAL_OPCODE                0x06
/// Device unavailable (performing FPU instructions without FPU)
#define AMD64_EXC_DEVICE_UNAVAIL                0x07
/// Double fault
#define AMD64_EXC_DOUBLE_FAULT                  0x08
/// Invalid task state segment
#define AMD64_EXC_INVALID_TSS                   0x0A
/// Segment not present
#define AMD64_EXC_SEGMENT_NP                    0x0B
/// Stack segment fault
#define AMD64_EXC_SS                            0x0C
/// General protection fault
#define AMD64_EXC_GPF                           0x0D
/// Page fault
#define AMD64_EXC_PAGING                        0x0E
/// x87 floating point exception
#define AMD64_EXC_FP                            0x10
/// Alignment check
#define AMD64_EXC_ALIGNMENT                     0x11
/// Machine check error
#define AMD64_EXC_MCE                           0x12
/// SIMD floating point error
#define AMD64_EXC_SIMD_FP                       0x13
/// Virtualization exception
#define AMD64_EXC_VIRT                          0x14

#ifndef ASM_FILE

namespace Platform::Amd64Uefi {
class Idt;

/**
 * Encapsulates default amd64 exception handlers
 *
 * This includes the intermediary dispatching logic, any exceptions that don't get dispatched, and
 * the assembly stubs.
 */
class ExceptionHandlers {
    public:
        static void Install(Idt &);

    private:
        static void Handle(Processor::Regs &state);
};
}

#endif

#endif
