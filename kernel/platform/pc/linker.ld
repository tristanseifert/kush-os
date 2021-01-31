ENTRY(osentry)
OUTPUT_FORMAT(elf32-i386)

SECTIONS {
    /* we're loaded to 0x100000, but then we map ourselves at 0xC0100000. */
    . = 0xC0100000;

    /* kernel code (immutable) section */
    __kern_keep_start = .;
    __kern_code_start = .;
    .text ALIGN(4K) : AT(ADDR(.text) - 0xC0000000) {
        KEEP(*(.multiboot))
        KEEP(*(.entry))
        KEEP(*(.entry.rodata))

        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
    }
    __kern_code_end = .;

    /* kernel data (mutable) section */
    __kern_data_start = .;
    .data ALIGN(4K) : AT(ADDR(.data) - 0xC0000000) {
        *(*.data)
    }
    __kern_data_size = SIZEOF(.data);

    /* kernel data (uninitialized, mutable) and stack */
    __kern_bss_start = .;
    .bss ALIGN(4K) : AT(ADDR(.bss) - 0xC0000000) {
        *(.bss)
        *(COMMON)
    }

    __kern_bss_size = SIZEOF(.bss);
    __kern_size = __kern_bss_start - __kern_code_start;

    /* align to a page boundary */
    __kern_keep_end = (. + 4095) & ~ 4095;

    /* miscellaneous info (we can skip mapping this) */
    .eh_frame ALIGN(4K) (NOLOAD) : AT(ADDR(.eh_frame) - 0xC0000000) {
        *(.eh_frame*)
    }

    __kern_end = .;
}

