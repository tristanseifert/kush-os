#define ASM_FILE
#include "LaunchInfo.h"

.global _start
.type _start, function

.extern __libc_task_info

/**
 * Program entry point
 *
 * We don't have a whole lot to do, since the loader has already ran all initializers and cleared
 * the .bss section. We'll simply go ahead and initialize the C library.
 *
 * When invoked, we expect a single argument on the stack: the task info structure.
 */
_start:
    # set up stack
    pop         %rbx
    xor         %rbp, %rbp

    mov         %rbx, __libc_task_info

    // set up the initializers and the C library
    call        __libc_init

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
    // return to the dynamic linker that called us
    ret
