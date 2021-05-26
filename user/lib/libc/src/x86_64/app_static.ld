ENTRY(_start)
OUTPUT_FORMAT(elf64-x86-64)

SECTIONS {
    /* default load address */
    . = 0x08000000;
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

        /* exception handling */
        *(.gcc_except_table)
        *(.gcc_except_table*)
        *(.eh_frame)
    }:rodata

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
    }:data
    __data_size = SIZEOF(.data);

    /* data (uninitialized, mutable) */
    .bss ALIGN(4K) : {
        __bss_start = .;
        *(.bss)
        *(.bss*)
        *(COMMON)
    }:data
    __bss_size = SIZEOF(.bss);
    __bss_size_dwords = SIZEOF(.bss)/4;

    /* thread local */
    .tdata : {
        __tls_data_start = .;
        *(.tdata);
        *(.tdata*);
    }:data
    .tbss : {
        *(.tbss);
        *(.tbss*);
    }:tls

    __tls_size = SIZEOF(.tdata) + SIZEOF(.tbss);
    __tls_data_size = SIZEOF(.tdata);
    __tls_bss_size = SIZEOF(.tbss);
}

PHDRS {
    text PT_LOAD FILEHDR PHDRS ;
    headers PT_PHDR PHDRS ;
    rodata PT_LOAD ;
    data PT_LOAD ;
    tls PT_TLS ;
}
