#include "ElfExecReader.h"
#include "Linker.h"

#include <cstdio>
#include <sys/elf.h>

using namespace dyldo;

/**
 * Initializes an executable ELF reader with a pre-opened file descriptor.
 */
ElfExecReader::ElfExecReader(FILE * _Nonnull file) : ElfReader(file) {
    this->ensureExec();
    this->init();
}

/**
 * Initializes an executable ELF reader. This ensures the binary being loaded is a dynamic
 * executable, and that we can otherwise load it.
 */
ElfExecReader::ElfExecReader(const char *path) : ElfReader(path) {
    this->ensureExec();
    this->init();
}

/**
 * Shared initialization. Reads the program headers, dynamic section, and extracts dependencies
 * from the file.
 */
void ElfExecReader::init() {
    // get the offset to the dynamic header info
    this->loadDynamicInfo();
    this->parseDynamicInfo();
}

/**
 * Release the list of imported symbols.
 */
ElfExecReader::~ElfExecReader() {

}

/**
 * Validates that the file is an executable.
 */
void ElfExecReader::ensureExec() {
    // read the header to extract some info
    Elf32_Ehdr hdr;
    memset(&hdr, 0, sizeof(Elf32_Ehdr));
    this->read(sizeof(Elf32_Ehdr), &hdr, 0);

    // ensure it's an executable. also extract the offsets to program headers
    if(hdr.e_type != ET_EXEC) {
        Linker::Abort("Invalid ELF type %08x", hdr.e_type);
    }

    this->entry = hdr.e_entry;
}



/**
 * Reads program headers and extracts the dynamic section.
 */
void ElfExecReader::loadDynamicInfo() {
    // read program headers
    auto phdrs = reinterpret_cast<Elf32_Phdr *>(malloc(sizeof(Elf32_Phdr) * this->phdrNum));
    if(!phdrs) {
        Linker::Abort("out of memory");
    }

    this->read(sizeof(Elf32_Phdr) * this->phdrNum, phdrs, this->phdrOff);

    // try to find the dynamic header
    for(size_t i = 0; i < this->phdrNum; i++) {
        const auto &phdr = phdrs[i];

        switch(phdr.p_type) {
            // found the dynamic info so read it out
            case PT_DYNAMIC: {
                const auto num = (phdr.p_filesz / sizeof(Elf32_Dyn));
                const auto base = reinterpret_cast<Elf32_Dyn *>(phdr.p_vaddr);
                this->dynInfo = std::span<Elf32_Dyn>(base, num);
                break;
            }

            // ignore all other types
            default:
                continue;
        }
    }

    // clean up
    free(phdrs);

    if(this->dynInfo.empty()) {
        Linker::Abort("PT_DYNAMIC missing");
    }
}

