#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <dlfcn.h>
#include <link.h>

#include <sys/elf.h>

extern uintptr_t __elf_base, __elf_headers_start, __elf_headers_end;

/**
 * Iterates the list of shared objects, invoking the user-specified callback for each of them.
 *
 * This is a Linux-ism, but libunwind seems to require it...
 */
int dl_iterate_phdr(int (*callback) (struct dl_phdr_info *info, const size_t size, void *data),
        void *data) {
    /*
     * Read the ELF header, which we assume is at the very start of the ELF header region based
     * on the static binary linker script.
     *
     * From that, try to find the offset to the program headers.
     */
    const Elf_Ehdr *hdr = (Elf_Ehdr *) &__elf_headers_start;
    if(memcmp(hdr->e_ident, ELFMAG, SELFMAG)) {
        fprintf(stderr, "invalid ELF magic\n");
        abort();
    }

    const size_t hdrRegionLen = (size_t) &__elf_headers_end - (size_t) &__elf_headers_start;
    if(hdr->e_phoff + (hdr->e_phentsize * hdr->e_phnum) > hdrRegionLen) {
        fprintf(stderr, "program headers out of range (%lu %lu)\n", hdr->e_phoff, hdrRegionLen);
        abort();
    }

    Elf_Phdr *phdrs = (Elf_Phdr *) ((uintptr_t) &__elf_headers_start) + hdr->e_phoff;

    // prepare callback info
    struct dl_phdr_info info = {
        .dlpi_addr = (Elf_Addr) &__elf_base,
        .dlpi_name = "unknown",
        .dlpi_phdr = phdrs,
        .dlpi_phnum = hdr->e_phnum
    };

    return callback(&info, sizeof info, data);
}

