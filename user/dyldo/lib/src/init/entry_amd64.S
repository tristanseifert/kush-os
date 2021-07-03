.globl __dyldo_start

# secret libc api :D
.extern __libc_set_heap_start
.extern __dyldo_stack_start

/**
 * When we enter here, we will have a stack frame that looks a little bit like the following:
 *
 * /------------------------------\ Bottom - 8
 * |    Address of launch info    |
 * \------------------------------/ Bottom of stack
 *
 * We also store the stack pointer at this point, so that when we jump to the user code, we can
 * restore it. This assumes that the bottom of the stack is aligned on a 256-byte boundary, as we
 * simply round the address up.
 */
__dyldo_start:
    // set up stack
    xor         %rbp, %rbp
    pop         %r15 // preserved by callees

    // Clear bss
    mov         $__bss_size_dwords-1, %rcx
    test        %rcx, %rcx
    js          1f

    movabs      $__bss_start, %rdi
    xor         %rax, %rax
    cld
    rep stosb

1:
    movabs      $__dyldo_stack_start, %rax
    mov         %rsp, (%rax)

    /*
     * Initialize the C library.
     *
     * Additionally, relocate the standard heap to grow upwards, up to 64M, somewhere in the first
     * 4G of the dynamic linker region that we reserve for our own use rather than loading shared
     * libraries.
     *
     * The address range 0x680000000000 to 0x6FFFFFFFFFFF is reserved for the dynamic linker use.
     */
    mov         $0x680080000000, %rdi
    mov         $0x4000000, %rsi
    call        __libc_set_heap_start

    call        __libc_init

    // invoke dynamic linker entry
    mov         %r15, %rdi
    call        dyldo_start

    // we really shouldn't get here
    xor         %rdi, %rdi
    call        exit
