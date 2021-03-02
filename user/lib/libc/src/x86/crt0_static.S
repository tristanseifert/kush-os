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
    # set up stack
    pop         %ebx
    xor         %ebp, %ebp

    # clear BSS
    mov         $__bss_size_dwords-1, %ecx
    test        %ecx, %ecx
    js          1f

    mov         $__bss_start, %edi
    xor         %eax, %eax
    cld
    rep stosb

1:
    mov         %ebx, __libc_task_info

    # set up the initializers and the C library
    call        __libc_init
    call        _init

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

    # global destructors
    call        _fini

    # exit (with a status code of 0)
    pushl       $0
    pushl       $0
    call        TaskExit
