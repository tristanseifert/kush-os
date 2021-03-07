ENTRY(_osentry)
OUTPUT_FORMAT(elf64-x86-64)

/**
 * Define the addresses of some stuff for the bootloader, as well as the per core init stack
 * sizes
 */
/*mmio = 0xfffffffff4000000;
fb = 0xfffffffff8000000;*/
mmio = 0xfffffffff8000000;
fb = 0xfffffffffc000000;
initstack = 4096;

/**
 * Give us just one segment to load, which is the "boot" segment containing all of the stuff
 */
PHDRS {
    boot PT_LOAD;
}

SECTIONS {
    /* base address for the code and all, minus two pages for bootboot info and env */
    . = 0xffffffffffe00000;

    bootboot    = .; . += 0x1000;
    environment = .; . += 0x1000;

    /* kernel code (immutable) section */
    .text : ALIGN(4K) {
        __kern_keep_start = .;
        __kern_code_start = .;

        KEEP(*(.entry))
        KEEP(*(.entry.rodata))

        *(.text .text.*)
        *(.rodata .rodata.*)
    } :boot
    __kern_code_end = .;

    /* kernel data (mutable) section */
    .data : ALIGN(4K) {
        __kern_data_start = .;
        *(*.data .data.*)
    } :boot
    __kern_data_size = SIZEOF(.data);

    /* kernel data (uninitialized, mutable) and stack */
    .bss (NOLOAD) : ALIGN(4K) {
        __kern_bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
    } :boot

    __kern_bss_size = SIZEOF(.bss);
    __kern_bss_size_dwords = SIZEOF(.bss)/4;
    __kern_size = __kern_bss_start - __kern_code_start;

    /* align to a page boundary */
    __kern_keep_end = (. + 4095) & ~ 4095;

    __kern_end = .;
}

