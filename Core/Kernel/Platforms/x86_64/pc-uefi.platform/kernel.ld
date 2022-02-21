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
        __kern_code_start = .;

        *(.text .text.*)
    } :text
    __kern_code_end = .;

    /* Place the stivale2 header into rodata */
    .stivale2hdr : ALIGN(4K) {
        __kernel_rodata_start = .;
        KEEP(*(.stivale2hdr))
    } :rodata

    /* kernel read-only data (immutable) section */
    .rodata : {
        *(.rodata .rodata.*)
    } :rodata
    __kernel_rodata_end = .;

    /* kernel data (mutable) section */
    .data : ALIGN(4K) {
        __kern_data_start = .;
        *(*.data .data.*)
    } :data
    __kern_data_size = SIZEOF(.data);

    /* kernel data (uninitialized, mutable) and stack */
    .bss (NOLOAD) : ALIGN(64) {
        __kern_bss_start = .;
        *(COMMON)
        *(.bss .bss.*)
    } :data
    __kern_bss_end = .;
    __kern_size = __kern_bss_start - __kern_code_start;

    /* align to a page boundary */
    __kern_keep_end = (. + 4095) & ~ 4095;
    __kern_end = .;
}

