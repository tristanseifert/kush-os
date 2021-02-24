ENTRY(_start)
OUTPUT_FORMAT(elf32-i386)

SECTIONS {
    /* default load address */
    . = 0x08000000;

    /* code (immutable) section */
    .text ALIGN(4K) : {
        *(.text)
        *(.text*)
    }

    /* read only data */
    .rodata ALIGN(4K) : {
        *(.rodata)
        *(.rodata*)

        /* exception handling */
        *(.gcc_except_table)
        *(.gcc_except_table*)
        *(.eh_frame)
    }

    /* data (mutable) section */
    .data ALIGN(4K) : {
        /* Include the list of initialization functions sorted. */
        _init_array_start = .;
        KEEP (*(SORT(EXCLUDE_FILE(crti.o crtn.o) .init_array.*)))
        KEEP (*(EXCLUDE_FILE(crti.o crtn.o) .init_array))
        _init_array_end = .;

        /* Include the list of termination functions sorted. */
        _fini_array_start = .;
        KEEP (*(SORT(EXCLUDE_FILE(crti.o crtn.o) .fini_array.*)))
        KEEP (*(EXCLUDE_FILE(crti.o crtn.o) .fini_array))
        _fini_array_end = .;

        __data_start = .;
        *(*.data)
        *(*.data*)

        /* relocations; is this ok? */
        (*.got)
        *(*.got*)
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

