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
    // copy arguments off stacks
    push        %ebp
    mov         %esp, %ebp

    mov          8(%ebp), %eax
    mov         12(%ebp), %ebx
    mov         16(%ebp), %ecx

    // reset the stack
    mov         %ebx, %esp
    push        %ecx

    // clear all other registers
    xor         %ebx, %ebx
    xor         %ecx, %ecx
    xor         %edx, %edx
    xor         %ebp, %ebp
    xor         %edi, %edi
    xor         %esi, %esi

    jmp         *%eax
