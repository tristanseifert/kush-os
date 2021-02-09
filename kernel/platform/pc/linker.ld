ENTRY(_osentry)
OUTPUT_FORMAT(elf32-i386)

SECTIONS {
    . = 0x100000;
    /* multiboot header and boot code */
    .boot ALIGN(4K) : {
        __kern_keep_start = .;
        KEEP(*(.multiboot))
        KEEP(*(.entry))
        KEEP(*(.entry.rodata))
    }

    /* we're loaded to 0x100000, but then we map ourselves at 0xC0100000. */
    . += 0xC0000000;
    /* kernel code (immutable) section */
    .text ALIGN(4K) : AT(ADDR(.text) - 0xC0000000) {
        __kern_code_start = .;

        *(.entry.high)
        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
    }
    __kern_code_end = .;

    /* kernel data (mutable) section */
    .data ALIGN(4K) : AT(ADDR(.data) - 0xC0000000) {
        __kern_data_start = .;
        *(*.data)
        *(*.data*)
    }
    __kern_data_size = SIZEOF(.data);

    /* kernel data (uninitialized, mutable) and stack */
    .bss ALIGN(4K) : AT(ADDR(.bss) - 0xC0000000) {
        __kern_bss_start = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
    }

    __kern_bss_size = SIZEOF(.bss);
    __kern_bss_size_dwords = SIZEOF(.bss)/4;
    __kern_size = __kern_bss_start - __kern_code_start;

    /* align to a page boundary */
    __kern_keep_end = (. + 4095) & ~ 4095;

    __kern_end = .;
}

