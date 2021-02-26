#define ASM_FILE
#include "LaunchInfo.h"

.global _start
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
    pop         %ebx
    xor         %ebp, %ebp

    mov         %ebx, __libc_task_info

    # set up the initializers and the C library
    call        __libc_init

    # run main function (get argc/argv from task info if non-null)
    mov         __libc_task_info, %eax
    test        %eax, %eax
    jz          2f

    pushl       LAUNCHINFO_OFF_ARGPTR(%eax)
    pushl       LAUNCHINFO_OFF_NARGS(%eax)
    call        main
    jmp         3f

2:
    pushl       $0
    pushl       $0
    call        main

3:
    add         $8, %esp

    # return to the dynamic linker that called us
    ret
