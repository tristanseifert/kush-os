/**
 * Linker script for the dynamic linker.
 */
ENTRY(__dyldo_start)
OUTPUT_FORMAT(elf32-i386)

SECTIONS {
    /* default load address */
    . = 0xB8000000;

    /* code (immutable) section */
    .text ALIGN(4K) : {
        *(.text)
        *(.text*)
    }

    /* read only data */
    .rodata ALIGN(4K) : {
        *(.rodata)
        *(.rodata*)
    }

    /* data (mutable) section */
    .data ALIGN(4K) : {
        __data_start = .;
        *(*.data)
        *(*.data*)

        /* relocations; is this ok? */
        /*(*.got)
        *(*.got*)*/
    }
    __data_size = SIZEOF(.data);

    /* data (uninitialized, mutable) */
    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
    }

    __bss_size = SIZEOF(.bss);
    __bss_size_dwords = SIZEOF(.bss)/4;

    /* garbage we want to discard */
    /DISCARD/ : {
        *(.comment*)

        /* exception handling */
        *(.gcc_except_table)
        *(.gcc_except_table*)
        *(.eh_frame)
    }
}

