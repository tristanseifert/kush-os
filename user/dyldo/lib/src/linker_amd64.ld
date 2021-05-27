/**
 * Linker script for the dynamic linker.
 */
ENTRY(__dyldo_start)
OUTPUT_FORMAT(elf64-x86-64)

SECTIONS {
    /* default load address */
    . = 0x100000;
    __elf_base = .;

    /* skip past ELF headers */
    __elf_headers_start = .;
    . += SIZEOF_HEADERS;
    __elf_headers_end = .;

    /* code (immutable) section */
    .text ALIGN(4K) : {
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
    .tdata : {
        __tls_data_start = .;
        *(.tdata);
        *(.tdata*);
    }:data
    .tbss : {
        *(.tbss);
        *(.tbss*);
    }:tls

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
    tls PT_TLS ;
}
/*
 * Since we use the static C library, define some stub symbols so the static TLS support will
 * link without complaining, even though we won't be using it.
 */
__tls_size = 0;
__tls_data_size = 0;
__tls_data_start = 0;
