ENTRY(user_syscall_stub)
OUTPUT_FORMAT(elf32-i386)

SECTIONS {
    /* fixed VM load address */
    . += 0xBF5F0000;
    .text ALIGN(4K) : AT(ADDR(.text) - 0xBF5F0000) {
        *(.text.first)
        *(.text)
    }
}

