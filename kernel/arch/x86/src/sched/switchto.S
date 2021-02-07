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

    // restore registers and CPU state
    popa
    popf

    // return to caller
    ret



.globl x86_switchto_save
/**
 * Performs a context switch, saving the current thread state.
 *
 * Stack frame setup:
 *  -4: Return address
 *  -8: From thread
 * -12: To thread
 *
 * Note that we actually return to a little return stub; this pops off the base pointer to undo
 * the stack frame we set up to access the parameters passed to this function. Yet another fun
 * idiosyncrasies (emphasis on idiot) of x86 :D
 */
x86_switchto_save:
    // set up stack frame
    push        %ebp
    mov         %esp, %ebp

    // push the instruction pointer and flags
    push        $x86_switchto_swap_stub

    pushf
    cli

    // push GP registers and the segment descriptors
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

/**
 * Stub for returning to a task lol
 */
x86_switchto_swap_stub:
    pop         %ebp
    ret