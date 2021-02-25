.globl __libdyldo_init

__libdyldo_init:
    # set up stack
    xor         %ebp, %ebp

    # initialize C library
    call        __libc_init@plt

    # invoke dynamic linker entry
    call        dyldo_start@plt
