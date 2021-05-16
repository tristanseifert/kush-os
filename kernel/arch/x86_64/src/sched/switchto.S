#define ASM_FILE
#include "ThreadState.h"
#include "gdt.h"

.global amd64_switchto
.global amd64_switchto_save
.global amd64_ring3_return
.global amd64_dpc_stub

/**
 * Performs a context switch to a thread; context is NOT saved.
 *
 * Until we actually get down to restoring the thread state, %eax is the pointer to the arch
 * ThreadState struct.
 */
amd64_switchto:
    // set up stack frame and get parameter
    push        %rbp
    mov         %rsp, %rbp

/**
 * Common context switch restoration function; %rdi should point to a ThreadState struct, with a
 * correctly formed stack frame that we can return to.
 */
amd64_switchto_common:
    // restore FPU state
    mov         TS_OFF_FPU_ENABLED(%rdi), %rax
    test        %rax, %rax
    jz          1f

    fxrstor     TS_OFF_FXSAVE(%rdi)

1:
    // switch back to the appropriate stack
    mov         TS_OFF_STACKTOP(%rdi), %rsp

amd64_switchto_common_stacked:
    // restore registers and CPU state
    pop         %r15
    pop         %r14
    pop         %r13
    pop         %r12
    pop         %r11
    pop         %r10
    pop         %r9
    pop         %r8
    pop         %rdi
    pop         %rsi
    pop         %rbp
    pop         %rbx
    pop         %rdx
    pop         %rcx
    pop         %rax

    popfq

    // return to caller
    ret



/**
 * Performs a context switch, saving the current thread state.
 *
 * %rdi: from task, %rsi: to task

 * Note that we actually return to a little return stub; this pops off the base pointer to undo
 * the stack frame we set up to access the parameters passed to this function. Yet another fun
 * idiosyncrasies (emphasis on idiot) of x86 :D
 */
amd64_switchto_save:
    // set up stack frame
    push        %rbp
    mov         %rsp, %rbp

    // push the instruction pointer and flags
    push        $amd64_switchto_swap_stub
    pushfq

    // push general-purpose registers
    push        %rax
    push        %rcx
    push        %rdx
    push        %rbx
    push        %rbp
    push        %rsi
    push        %rdi
    push        %r8
    push        %r9
    push        %r10
    push        %r11
    push        %r12
    push        %r13
    push        %r14
    push        %r15

    // store the stack pointer to restore to in the struct
    mov         %rsp, TS_OFF_STACKTOP(%rdi)

    // save FPU state, if required
    mov         TS_OFF_FPU_ENABLED(%rdi), %rax
    test        %rax, %rax
    jz          2f

    mov         TS_OFF_FXSAVE(%rdi), %rax
    fxsave      (%rax)

2:
    // the previous thread's state has been saved. switch to the next thread
    mov         %rsi, %rdi

    // restoring the thread state is the exact same as normal
    jmp         amd64_switchto_common

/**
 * Stub for returning to a task lol
 */
amd64_switchto_swap_stub:
    pop         %rbp
    sti
    ret

/**
 * Builds a stack frame suitable for use with IRET to return to ring 3. Additionally, all of the
 * general purpose registers are cleared.
 *
 * %rdi: Program counter
 * %rsi: Stack pointer
 * %rdx: Argument
 */
amd64_ring3_return:
    // to prevent interruptions
    cli

    // set up stack frame
    push        %rbp
    mov         %rsp, %rbp

    // the stack we're returning with
    pushq       $GDT_USER_DATA_SEG|0x3
    pushq       %rsi

    // flags: always enable interrupts
    xor         %rax, %rax
    or          $0x002, %rax // reserved bit, always 1
    or          $0x200, %rax
    push        %rax

    // program counter
    pushq       $GDT_USER_CODE_SEG|0x3
    pushq       %rdi

    // restore the argument
    mov         %rdx, %rdi

    // clear registers
    xor         %rax, %rax
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

    // and finally do the return
    iretq

/**
 * Invokes the DPC handler, then does a context switch to the thread state currently on the stack.
 */
amd64_dpc_stub:
    // set up stack frame
    push        %rbp
    mov         %rsp, %rbp

    // run the handlers
    call        amd64_dpc_handler

    // clean up stack and perform the context switch
    add         $8, %rsp
    jmp         amd64_switchto_common_stacked
