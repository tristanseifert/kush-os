ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)

SECTIONS {
    /* default load address */
    . = 0x08000000;

    /* code (immutable) section */
    .text ALIGN(4K) : {
        __text_start = .;

        *(.text)
        *(.text*)
        *(.rodata)
        *(.rodata*)
    }
    __text_size = SIZEOF(.text);

    /* data (mutable) section */
    .data ALIGN(4K) : {
        __data_start = .;
        *(*.data)
        *(*.data*)
    }
    __data_size = SIZEOF(.data);

    /* data (uninitialized, mutable) */
    .bss ALIGN(4K) : {
        __bss_start = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
    }

    __bss_size = SIZEOF(.bss);
    __bss_size_dwords = SIZEOF(.bss)/4;
}

