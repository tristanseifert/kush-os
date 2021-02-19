.global _start

/**
 * Program entry point: this will perform initialization of our temporary sections, then invoke
 * global constructors and run the program's main function. When that returns, we'll run all
 * destructors and request task termination.
 */
_start:
    # set up stack
    xor     %ebp, %ebp

    # clear BSS
    mov         $__bss_size_dwords-1, %ecx
    test        %ecx, %ecx
    js          1f

    mov         $__bss_start, %edi
    xor         %eax, %eax
    cld
    rep stosb

1:

    # set up the initializers
    call        _init

    # run main function (no argc or argv yet!)
    pushl       $0
    pushl       $0
    call        main
    add         $8, %esp

    # global destructors
    call        _fini

    # exit (with a status code of 0)
    pushl       $0
    pushl       $0
    call        TaskExit
