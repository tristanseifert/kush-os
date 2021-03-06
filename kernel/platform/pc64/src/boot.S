.extern kernel_init
.extern kernel_main
.extern _ZN6crypto6Random4InitEv
.extern arch_init
.extern platform_init

.section .entry, "ax"
.global _osentry
/**
 * Entry point from the BOOTBOOT loader. At this point, we're executing in 64-bit mode, with
 * the floating point unit and SSE enabled. Interrupts are masked, and ALL cores in the system
 * will come through this code path (but with their own stacks).
 *
 * Additionally, the first 16G or so of physical memory are identity mapped starting at $0 in the
 * VM address space.
 */
_osentry:
    cli

    /**
     * Enable global pages and page-size extension. (XXX: should we test whether these are
     * supported before enabling them?)
     */
    mov         %cr4, %rax
    or          $((1 << 7) | (1 << 4)), %rax
    mov         %rax, %cr4

    /**
     * Determine the CPU number; this only supports up to 256 cores, we need to support x2APIC for
     * supporting more than this
     */
    mov         $1, %eax
    cpuid
    shr         $24, %ebx

    // jump if not the boostrap processor
    cmpw        %bx, bootboot + 0xC
    jne         ap_start

    // switch to the bootstrap stack
    lea         bsp_stack_bottom, %rsp

    // set up a NULL stack frame, so stack unwinding can stop here
    xor         %rbp, %rbp

    // clear BSS
    mov         $__kern_bss_start, %rdi
    xor         %rax, %rax
    mov         $__kern_bss_size_dwords-1, %rcx
    cld
    rep stosl

    // perform initializations
    call        arch_init
    call        platform_init
    call        _ZN6crypto6Random4InitEv
    call        kernel_init

    // jump into kernel
    call        kernel_main

    // hang forever if we return (shouldn't happen)
    cli

.Lhang:
    hlt
    jmp .Lhang

/**
 * Entry point for application processors (secondary CPU cores)
 *
 * For now, we do nothing with these after incrementing the "number of APs" counter; but, later,
 * these should spin until the rest of the kernel is initialized enough, then they should use
 * their CPU index as an offset into some grand old array to get per-processor structures and
 * enter the scheduler to start executing threads.
 *
 * It's likely each processor will have an APIC that needs configuring too.
 */
ap_start:
    // per-processor init

    // for now, just chill and do nothing
.Lyeet:
    cli
    hlt
    jmp         .Lyeet


/// bootstrap processor stack
.section .bss
bsp_stack_top:
.skip 0x2000
bsp_stack_bottom:
