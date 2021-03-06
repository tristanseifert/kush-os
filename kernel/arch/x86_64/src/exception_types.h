/*
 * Define the x86 exception vector numbers
 */
#ifndef ARCH_X86_EXCEPTION_TYPES_H
#define ARCH_X86_EXCEPTION_TYPES_H

/// Divide-by-zero
#define X86_EXC_DIVIDE                          0x00
/// Debugging feature
#define X86_EXC_DEBUG                           0x01
/// Non-maskable IRQ
#define X86_EXC_NMI                             0x02
/// Breakpoint
#define X86_EXC_BREAKPOINT                      0x03
/// Overflow
#define X86_EXC_OVERFLOW                        0x04
/// Bounds check exceeded
#define X86_EXC_BOUNDS                          0x05
/// Invalid opcode
#define X86_EXC_ILLEGAL_OPCODE                  0x06
/// Device unavailable (performing FPU instructions without FPU)
#define X86_EXC_DEVICE_UNAVAIL                  0x07
/// Double fault
#define X86_EXC_DOUBLE_FAULT                    0x08
/// Invalid task state segment
#define X86_EXC_INVALID_TSS                     0x0A
/// Segment not present
#define X86_EXC_SEGMENT_NP                      0x0B
/// Stack segment fault
#define X86_EXC_SS                              0x0C
/// General protection fault
#define X86_EXC_GPF                             0x0D
/// Page fault
#define X86_EXC_PAGING                          0x0E
/// x87 floating point exception
#define X86_EXC_FP                              0x10
/// Alignment check
#define X86_EXC_ALIGNMENT                       0x11
/// Machine check error
#define X86_EXC_MCE                             0x12
/// SIMD floating point error
#define X86_EXC_SIMD_FP                         0x13
/// Virtualization exception
#define X86_EXC_VIRT                            0x14

#endif
