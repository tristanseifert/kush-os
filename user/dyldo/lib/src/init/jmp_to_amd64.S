/**
 * Trampoline to jump to the program's entry point.
 *
 * This resets the registers, sets the stack back to the base, and pushes some arguments onto the
 * stack:
 *
 * 1. LaunchInfo struct ptr
 *
 * We're called like "void dyldo_jmp_to(uintptr_t pc, uintptr_t sp, uintptr_t arg)"
 */
.globl __dyldo_jmp_to
__dyldo_jmp_to:
    // reset the stack
    mov         %rsi, %rsp
    mov         %rdi, %rax
    mov         %rdx, %rdi

    /*
     * Clear all registers, EXCEPT for RAX (contains destination address) and RDI (contains the
     * argument to the entry point)
     */
    xor         %rbx, %rbx
    xor         %rcx, %rcx
    xor         %rdx, %rdx
    xor         %rbp, %rbp
    xor         %rsi, %rsi
    xor         %r8, %r8
    xor         %r9, %r9
    xor         %r10, %r10
    xor         %r11, %r11
    xor         %r12, %r12
    xor         %r13, %r13
    xor         %r14, %r14
    xor         %r15, %r15

    // jump to entry point
    jmp         *%rax
