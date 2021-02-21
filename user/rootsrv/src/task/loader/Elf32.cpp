#include "Elf32.h"

#include <fmt/format.h>
#include <sys/elf.h>

using namespace task::loader;

/**
 * Initializes a 32-bit ELF loader.
 *
 * This performs some validation of the header of the ELF.
 *
 * @throws An exception is thrown if the ELF header is invalid.
 */
Elf32::Elf32(const std::span<std::byte> &bytes) : Loader(bytes) {
    // get the header
    auto hdrSpan = bytes.subspan(0, sizeof(Elf32_Ehdr));
    if(hdrSpan.size() < sizeof(Elf32_Ehdr)) {
        throw LoaderError("ELF header too small");
    }

    auto hdr = reinterpret_cast<const Elf32_Ehdr *>(hdrSpan.data());

    // ensure the ELF is little endian, the correct version, and has an entry
    if(hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        throw LoaderError(fmt::format("Invalid ELF format: {:02x}", hdr->e_ident[EI_DATA]));
    }

    if(hdr->e_ident[EI_VERSION] != EV_CURRENT) {
        throw LoaderError(fmt::format("Invalid ELF version ({}): {:02x}", "ident", hdr->e_ident[EI_VERSION]));
    } else if(hdr->e_version != EV_CURRENT) {
        throw LoaderError(fmt::format("Invalid ELF version ({}): {:02x}", "header", hdr->e_version));
    }

    if(!hdr->e_entry) {
        throw LoaderError("Invalid ELF entry");
    }
}

