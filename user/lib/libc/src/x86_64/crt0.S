#define ASM_FILE
#include "LaunchInfo.h"

.global _start
.type _start, function

.extern __libc_task_info
.extern __stack_chk_guard

/**
 * Program entry point
 *
 * We don't have a whole lot to do, since the loader has already ran all initializers and cleared
 * the .bss section. The stack guard needs to be set up, and we can then go initialize the C
 * library.
 *
 * This function is invoked by the linker like with two arguments: first, the info structure in
 * %rdi; and the stack guard value in %rsi.
 */
_start:
    // set up stack
    xor         %rbp, %rbp
    mov         %rdi, __libc_task_info

    // fix up stack guard (so the dynamic linker and us use the same value)
    lea         __stack_chk_guard(%rip), %rax
    mov         %rsi, (%rax)
    xor         %rsi, %rsi
    xor         %rax, %rax

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
    // call TaskExit with the return value of main (in %rax)
    xor         %rdi, %rdi
    mov         %rax, %rsi
    call        TaskExit

    // should never get here!
    ud2
