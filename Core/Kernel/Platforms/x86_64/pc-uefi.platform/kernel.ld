ENTRY(_osentry)
OUTPUT_FORMAT(elf64-x86-64)

/**
 * Define the different protected segments
 */
PHDRS {
    null    PT_NULL    FLAGS(0) ;                   /* Null segment */
    text    PT_LOAD    FLAGS((1 << 0) | (1 << 2)) ; /* Execute + Read */
    rodata  PT_LOAD    FLAGS((1 << 2)) ;            /* Read only */
    data    PT_LOAD    FLAGS((1 << 1) | (1 << 2)) ; /* Write + Read */
}

SECTIONS {
    /* base address for the kernel; it is slid within this 2GB region */
    . = 0xffffffff80000000;

    /* kernel code (immutable) section */
    .text : ALIGN(4K) {
        __kernel_text_start = .;
        *(.text .text.*)
    } :text
    __kernel_text_end = .;

    /* Place the stivale2 header into rodata */
    .stivale2hdr : ALIGN(4K) {
        KEEP(*(.stivale2hdr))
    } :rodata

    /* kernel read-only data (immutable) section */
    .rodata : {
        *(.rodata .rodata.*)
    } :rodata

    /* kernel data (mutable) section */
    .data : ALIGN(4K) {
        *(*.data .data.*)
    } :data

    /* kernel data (uninitialized, mutable) and stack */
    .bss (NOLOAD) : ALIGN(64) {
        *(COMMON)
        *(.bss .bss.*)
    } :data

    /* discard some stuff */
    /DISCARD/ : {
        /* exception handling */
        *(.eh_frame)
    }
}

