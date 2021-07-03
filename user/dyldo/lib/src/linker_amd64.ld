/**
 * Linker script for the dynamic linker.
 */
ENTRY(__dyldo_start)
OUTPUT_FORMAT(elf64-x86-64)

SECTIONS {
    /* default load address */
    . = 0x680000000000;
    __elf_base = .;

    /* skip past ELF headers */
    __elf_headers_start = .;
    . += SIZEOF_HEADERS;
    __elf_headers_end = .;

    /* code (immutable) section */
    .text : {
        *(.text)
        *(.text*)
    }:text

    /* read only data */
    .rodata ALIGN(4K) : {
        *(.rodata)
        *(.rodata*)
    }:rodata

    /* data (mutable) section */
    .data ALIGN(4K) : {
        __data_start = .;
        *(*.data)
        *(*.data*)

        /* relocations; is this ok? */
        /*(*.got)
        *(*.got*)*/
    }:data
    __data_size = SIZEOF(.data);

    /* data (uninitialized, mutable) */
    .bss : {
        __bss_start = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
    }:data

    __bss_size = SIZEOF(.bss);
    __bss_size_dwords = SIZEOF(.bss)/4;

    /*
     * Set up sections for thread local data, but we won't actually be using it as the task loader
     * will not bother with setting up the thread-locals for us.
     */

    /* garbage we want to discard */
    /DISCARD/ : {
        *(.comment*)

        /* exception handling */
        *(.gcc_except_table)
        *(.gcc_except_table*)
        *(.eh_frame)
    }
}

/**
 * Define the program headers; this ensures no thread-local data is stored
 */
PHDRS {
    text PT_LOAD FILEHDR PHDRS ;
    headers PT_PHDR PHDRS ;
    rodata PT_LOAD ;
    data PT_LOAD ;
}

