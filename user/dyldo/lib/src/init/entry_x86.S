.globl __dyldo_start

# secret libc api :D
.extern __libc_set_heap_start
.extern __dyldo_stack_start

/**
 * When we enter here, we will have a stack frame that looks a little bit like the following:
 *
 * /------------------------------\ Bottom - 4
 * |    Address of launch info    |
 * \------------------------------/ Bottom of stack
 *
 * We also store the stack pointer at this point, so that when we jump to the user code, we can
 * restore it. This assumes that the bottom of the stack is aligned on a 256-byte boundary, as we
 * simply round the address up.
 */
__dyldo_start:
    # set up stack
    xor         %ebp, %ebp

    # Clear bss
    mov         $__bss_size_dwords-1, %ecx
    test        %ecx, %ecx
    js          1f

    mov         $__bss_start, %edi
    xor         %eax, %eax
    cld
    rep stosb

1:
    mov         %esp, __dyldo_stack_start

    /*
     * Initialize the C library.
     *
     * Additionally, relocate the standard heap to grow upwards from 0xB9000000, with a maximum
     * size of 16M (end address 0xBA000000). This leaves some free space up at the top of the
     * memory map, but everything beyond 0xBF000000 is definitely off limits so we try to stay
     * clear of that.
     */
    push        $0x1000000
    push        $0xB9000000
    call        __libc_set_heap_start
    add         $8, %esp

    call        __libc_init

    # invoke dynamic linker entry
    call        dyldo_start
    add         $4, %esp

    # we really shouldn't get here
    call        exit
