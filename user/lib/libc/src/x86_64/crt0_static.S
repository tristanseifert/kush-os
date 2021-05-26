#define ASM_FILE
#include "LaunchInfo.h"

.global _start
.type _start, function
.extern __libc_task_info

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
    add         $0x10, %esp

    // global destructors
    call        _fini

    // exit (with a status code of 0)
    xor         %rdi, %rdi
    xor         %rsi, %rsi
    call        TaskExit
