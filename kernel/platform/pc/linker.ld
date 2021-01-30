ENTRY(_osentry)
OUTPUT_FORMAT(elf32-i386)

SECTIONS {
    /* we're loaded to 0x100000, but then we map ourselves at 0xC0100000. */
    . = 0xC0100000;

    /* kernel code (immutable) section */
    __kern_code_start = .;
    .text ALIGN(4096) : AT(ADDR(.text) - 0xC0000000) {
        *(.multiboot)
        *(.entry)
        *(.entry.rodata)
        *(.text)
        *(.rodata)
        *(.rodata*)
    }
    __kern_code_end = .;

    /* kernel data (mutable) section */
    __kern_data_start = .;
    .data ALIGN(4096) : AT(ADDR(.data) - 0xC0000000) {
        *(*.data)
    }
    __kern_data_size = SIZEOF(.data);

    /* kernel data (uninitialized, mutable) and stack */
    __kern_bss_start = .;
    .bss ALIGN(4096) : AT(ADDR(.bss) - 0xC0000000) {
        *(.bss)
        *(COMMON)
    }

    __kern_bss_size = SIZEOF(.bss);
    __kern_size = __kern_bss_start - __kern_code_start;

    __kern_end = .;
}

