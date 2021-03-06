ENTRY(user_syscall_stub)
OUTPUT_FORMAT(elf64-x86-64)

SECTIONS {
    /* fixed VM load address */
    . += 0x7FFFFFFFFF000000;
    .text ALIGN(4K) : AT(ADDR(.text) - 0x7FFFFFFFFF000000) {
        *(.text.first)
        *(.text)
    }
}

