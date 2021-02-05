#define ASM_FILE
#include "ThreadState.h"

.globl x86_switchto
/**
 * Performs a context switch to a thread; context is NOT saved.
 *
 * Until we actually get down to restoring the thread state, %eax is the pointer to the arch
 * ThreadState struct.
 */
x86_switchto:
    // disable interrupts
    cli

    // set up stack frame and get parameter
    push        %ebp
    mov         %esp, %ebp
    mov         8(%ebp), %eax

/**
 * Common context switch restoration function; %eax should point to a ThreadState struct, with a
 * correctly formed stack frame that we can iret to.
 */
x86_switchto_common:
    // restore FPU state
    mov         TS_OFF_FPU_ENABLED(%eax), %ebx
    test        %ebx, %ebx
    jz          1f

    fxrstor     TS_OFF_FXSAVE(%eax)

1:
    // switch back to the appropriate stack
    mov         TS_OFF_STACKTOP(%eax), %esp

    // reload the original data segment fault descriptor
    pop         %gs
    pop         %fs
    pop         %es
    pop         %ds

    // restore registers, clear the pushed error code and ISR number
    popa
    add         $0x8, %esp

    // return; this will restore the interrupt flag too
    iret



.globl x86_switchto_save
/**
 * Performs a context switch, saving the current thread state.
 *
 * Stack frame setup:
 *  -4: Return address
 *  -8: From thread
 * -12: To thread
 */
x86_switchto_save:
    // set up stack frame and get a pointer to the from thread
    push        %ebp
    mov         %esp, %ebp

    /*
     * Build the IRET stack frame. Since we're returning to this function, which by definition must
     * always be in the kernel code segment, we can skip pushing the esp/ss pieces.
     *
     * If we're an user mode thread, there'll be the "real" interrupt frame somewhere further down
     * the frame to use with the proper user mode state.
     */
    pushf
    push        %cs
    push        4(%ebp)

    // push fake error codes, GP registers and the segment descriptors
    pushl       $0x0
    pushl       $0x0

    pusha

    push        %ds
    push        %es
    push        %fs
    push        %gs

    // store the stack pointer to restore to in the struct
    mov         8(%ebp), %eax
    mov         %esp, TS_OFF_STACKTOP(%eax)

    // save FPU state, if required
    mov         TS_OFF_FPU_ENABLED(%eax), %ebx
    test        %ebx, %ebx
    jz          2f

    mov         TS_OFF_FXSAVE(%eax), %ebx
    fxsave      (%ebx)

2:
    // the previous thread's state has been saved. switch to the next thread
    mov         12(%ebp), %eax

    // restoring the thread state is the exact same as normal
    jmp         x86_switchto_common
