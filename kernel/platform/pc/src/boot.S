// Declare constants for the multiboot header.
#define ALIGN    1<<0
#define MEMINFO  1<<1
#define FLAGS    ALIGN | MEMINFO
#define MAGIC    0x1BADB002
#define CHECKSUM -(MAGIC + FLAGS)

// pull in multiboot info
#define ASM_FILE                        1
#include "multiboot2.h"

// define the multiboot header
.section .multiboot
.align  8
multiboot_header:
    // magic value
    .long       MULTIBOOT2_HEADER_MAGIC
    // x86 ISA
    .long       MULTIBOOT_ARCHITECTURE_I386
    // total length of multiboot header, including tags
    .long       (multiboot_header_end - multiboot_header)
    // header checksum
    .long       -(MULTIBOOT2_HEADER_MAGIC + MULTIBOOT_ARCHITECTURE_I386 + (multiboot_header_end - multiboot_header)) & 0xFFFFFFFF

// modules must be page aligned
    .short      MULTIBOOT_HEADER_TAG_MODULE_ALIGN
    .short      0
    .long       8

// end of tags list
    .short      MULTIBOOT_HEADER_TAG_END
    .short      0
    .long       8
multiboot_header_end:

/*************************************************************************************************/
.section .entry, "ax"

.extern kernel_init
.extern kernel_main

.extern arch_init
.extern platform_init

.extern x86_multiboot_info
.extern x86_multiboot_magic

.globl _osentry
.type _osentry, @function
.set osentry, (_osentry - 0xC0000000)
.globl osentry

/**
 * Entry point for the bootloader
 */
_osentry:
    cli

    // Enter protected mode
    mov         %cr0, %ecx
    or          $0x00000001, %ecx
    mov         %ecx, %cr0

    // enable page size extension and PAE, and global pages
    mov         %cr4, %ecx
    or          $0x000000B0, %ecx
    mov         %ecx, %cr4

    // Load page directory
    mov         $boot_pae_pdpt, %ecx
    mov         %ecx, %cr3

    // Enable paging
    mov         %cr0, %ecx
    or          $0x80000000, %ecx
    mov         %ecx, %cr0

    // jump to the higher half kernel
    lea         jmp_highhalf, %ecx
    jmp         *%ecx

# All of this code is offset in virtual memory
.section .entry.high
jmp_highhalf:
    // Now, we're executing at 0xC0100000, so use the virtual address for stack.
    mov         $stack_top, %esp

    // Push multiboot info (EBX) and magic number (EAX)
    push        %ebx
    push        %eax

    // Write multiboot info pointer to memory
    mov         %eax, x86_multiboot_magic
    mov         %ebx, x86_multiboot_info

    // Check for SSE, and if it exists, enable it
    mov         $0x1, %eax
    cpuid
    test        $(1 << 25), %edx
    jz          .noSSE

    call        sse_init

.noSSE:
    // clear caches
    wbinvd

    // perform initializations
    call        arch_init
    call        platform_init
    call        kernel_init

    // jump into kernel
    call        kernel_main

    // hang forever if we return (shouldn't happen)
    cli

.Lhang:
    hlt
    jmp .Lhang

/*
* Enables the SSE features of the processor.
*/
sse_init:
    mov         %cr0, %eax

    // clear coprocessor emulation CR0.EM
    and         $0xFFFB, %ax

    // set coprocessor monitoring CR0.MP
    or          $0x2, %ax
    mov         %eax, %cr0
    mov         %cr4, %eax

    // set CR4.OSFXSR and CR4.OSXMMEXCPT at the same time
    or          $(3 << 9), %ax
    mov         %eax, %cr4
    ret

/*************************************************************************************************/
# Bootup page tables
.section .entry.rodata

/**
 * Page directory pointer table (PDPT); this contains 4 entries, each of which 8 bytes, pointing
 * to a page directory describing 1G of memory.
 *
 * Here, the first gig of RAM is identity mapped, followed by 2 gigs of unmapped space, and then
 * the first 16M of kernel space (at 0xC0000000) being mapped to address 0.
 */
.align 0x20
boot_pae_pdpt:
    // first 1 gig is identity mapped
    .long        boot_pae_pdt_identity + 0b1
    .long        0
    // two unmapped spaces
    .quad        0
    .quad        0
    // first 16M mapped to 0 for kernel
    .long        boot_pae_pdt_upper + 0b1
    .long        0

/**
 * PAE page directory that maps the first 1G of address space as-is.
 */
.align 0x1000
boot_pae_pdt_identity:
    .set        addr, 0
    .rept       512
    .long       addr | 0b010000011
    .long       0 // this is where the NX bit would be
    .set        addr, addr+0x200000
    .endr

/**
 * PAE page directory that maps the first 16M starting at 0xC0000000 to physical address 0.
 */
.align 0x1000
boot_pae_pdt_upper:
    .set        addr, 0
    .rept       8
    .long       addr | 0b010000011
    .long       0 // this is where the NX bit would be
    .set        addr, addr+0x200000
    .endr

    // remainder of the table is unmapped
    .rept       504
    .quad       0
    .endr


/*************************************************************************************************/
// 32K kernel stack to use during initialization
.section .bss
stack_bottom:
.skip 0x8000
stack_top:
