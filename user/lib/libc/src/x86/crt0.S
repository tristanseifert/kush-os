.global _start

/**
 * Program entry point: we have basically nothing to do here, since the dynamic linker will have
 * already set up the C library, cleared .bss section, and ran any initializers specified in
 * shared libraries and the executable itself.
 */
_start:
    # set up stack
    xor     %ebp, %ebp

    # set up the initializers and the C library
    # call        __libc_init

    # run main function (no argc or argv yet!)
    pushl       $0
    pushl       $0
    call        main
    add         $8, %esp

    # return to the dynamic linker that called us
    ret
