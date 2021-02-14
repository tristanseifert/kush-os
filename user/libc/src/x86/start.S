.global _start

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

    # run main function (no argc or argv yet!)
    pushl   $0
    pushl   $0
    call    main
    add     $8, %esp

    # exit (with a status code of 0)
    pushl   $0
    pushl   $0
    call    TaskExit
