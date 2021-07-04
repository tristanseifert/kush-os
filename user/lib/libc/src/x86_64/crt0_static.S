#define ASM_FILE
#include "LaunchInfo.h"

.global _start
.type _start, function
.extern __libc_task_info
.extern __stack_chk_guard
.extern GetEntropy

/// temporary buffer for entropy
.lcomm      EarlyEntropyBuf, 8

/**
 * Program entry point: this will perform initialization of our temporary sections, then invoke
 * global constructors and run the program's main function. When that returns, we'll run all
 * destructors and request task termination.
 *
 * It's expected that on the stack at this point is a single argument, a pointer to the task info
 * structure.
 */
_start:
    // set up stack
    pop         %rbx
    xor         %rbp, %rbp

    // clear BSS
    mov         $__bss_size_dwords-1, %rcx
    test        %rcx, %rcx
    js          1f

    mov         $__bss_start, %rdi
    xor         %rax, %rax
    cld
    rep stosl

1:
    mov         %rbx, __libc_task_info

    /*
     * Get entropy from the kernel into a temporary storage we've allocated in the .bss segment; if
     * the call fails, we exit immediately.
     *
     * Until this step is complete, we're using the default stack guard value that's defined in
     * libsystem. Therefore, we should minimize the number of function calls made (and call no user
     * provided code) until this step is done. (Besides, you can't change the stack guard while in
     * a function as the return will fail without fixing up the stack manually.)
     */
    lea         EarlyEntropyBuf(%rip), %rdi
    mov         $0x8, %rsi
    call        GetEntropy

    cmpq        $0x8, %rax
    je          4f

    // abort if we can't get the entropy for the stack guard
    ud2

4:
    lea         EarlyEntropyBuf(%rip), %rdi
    lea         __stack_chk_guard(%rip), %rsi

    // copy out of the early entropy buffer and into stack guard
    movq        (%rdi), %rax
    movq        %rax, (%rsi)
    // then clear the entropy buffer
    xor         %rax, %rax
    movq        %rax, (%rdi)

    // set up the initializers and the C library
    call        __libc_init
    call        _init

    // run main function (get argc/argv from task info if non-null)
    mov         __libc_task_info, %rax
    test        %rax, %rax
    jz          2f

    mov         LAUNCHINFO_OFF_ARGPTR(%rax), %rsi
    mov         LAUNCHINFO_OFF_NARGS(%rax), %rdi
    call        main
    jmp         3f

2:
    xor         %rdi, %rdi
    xor         %rsi, %rsi
    call        main

3:
    push        %rax

    // global destructors
    call        _fini

    // call TaskExit with the return value of main (in %rax)
    pop         %rax
    xor         %rdi, %rdi
    mov         %rax, %rsi
    call        TaskExit

    // should never get here!
    ud2
