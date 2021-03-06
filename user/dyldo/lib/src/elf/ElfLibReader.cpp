#include "ElfLibReader.h"
#include "Linker.h"
#include "Library.h"

#include <cstdio>
#include <cstring>

#include <unistd.h>
#include <sys/elf.h>

using namespace dyldo;

/**
 * Initializes a shared library ELF reader with a pre-opened file descriptor.
 */
ElfLibReader::ElfLibReader(const uintptr_t _base, FILE *file, const char *path) :
    ElfReader(file, path), base(_base) {
    this->ensureLib();
}

/**
 * Initializes a shared library ELF reader.
 */
ElfLibReader::ElfLibReader(const uintptr_t _base, const char *path) : ElfReader(path), base(_base) {
    this->ensureLib();
}

/**
 * Release the list of imported symbols.
 */
ElfLibReader::~ElfLibReader() {

}

/**
 * Validates that the file is an executable.
 */
void ElfLibReader::ensureLib() {
    // read the header to extract some info
    Elf_Ehdr hdr;
    memset(&hdr, 0, sizeof hdr);
    this->read(sizeof hdr, &hdr, 0);

    // ensure it's an executable. also extract the offsets to program headers
    if(hdr.e_type != ET_DYN) {
        Linker::Abort("Invalid ELF type %08x for library", hdr.e_type);
    }
}



/**
 * Parses the program headers of the library, and loads all indicated segments. The virtual address
 * specified in the program header is added to our load address to yield the actual address at
 * which the page is loaded.
 */
void ElfLibReader::mapContents() {
    // read program headers
    auto phdrs = reinterpret_cast<Elf_Phdr *>(malloc(sizeof(Elf_Phdr) * this->phdrNum));
    if(!phdrs) {
        Linker::Abort("out of memory");
    }

    this->read(sizeof(Elf_Phdr) * this->phdrNum, phdrs, this->phdrOff);

    for(size_t i = 0; i < this->phdrNum; i++) {
        // ignore all non-LOAD segments
        const auto &phdr = phdrs[i];
        if(phdr.p_type != PT_LOAD) continue;

        // load the segment
        this->loadSegment(phdr, this->base);
    }

    // perform additional initialization
    this->init();
}

/**
 * Returns the total span of virtual memory allocated by the library.
 *
 * This is calculated by finding the highest end address of a segment, and adding one to it.
 *
 * @return Number of bytes of virtual memory required by the library, or 0 if none.
 */
const size_t ElfLibReader::getVmRequirements() const {
    size_t max = 0;
    for(const auto &segment : this->segments) {
        if(segment.vmEnd > max) {
            max = segment.vmEnd;
        }
    }

    // nothing loaded?
    if(!max) {
        return 0;
    }

    // otherwise, convert to size
    return (max - this->base) + 1;
}



/**
 * Shared initialization. Reads the program headers, dynamic section, and extracts dependencies
 * from the file. This takes place only after the library has been mapped.
 */
void ElfLibReader::init() {
    this->loadDynamicInfo();
    this->parseDynamicInfo();
}

/**
 * Reads program headers and extracts the dynamic section.
 */
void ElfLibReader::loadDynamicInfo() {
    // read program headers
    auto phdrs = reinterpret_cast<Elf_Phdr *>(malloc(sizeof(Elf_Phdr) * this->phdrNum));
    if(!phdrs) {
        Linker::Abort("out of memory");
    }

    this->read(sizeof(Elf_Phdr) * this->phdrNum, phdrs, this->phdrOff);

    // try to find the dynamic header
    for(size_t i = 0; i < this->phdrNum; i++) {
        const auto &phdr = phdrs[i];

        switch(phdr.p_type) {
            // found the dynamic info so read it out
            case PT_DYNAMIC: {
                const auto num = (phdr.p_filesz / sizeof(Elf_Dyn));
                const auto base = reinterpret_cast<Elf_Dyn *>(phdr.p_vaddr + this->base);
                this->dynInfo = std::span<Elf_Dyn>(base, num);
                break;
            }

            // program headers in memory
            case PT_PHDR: {
                auto ptr = reinterpret_cast<Elf_Phdr *>(this->rebaseVmAddr(phdr.p_vaddr));
                this->phdrs = std::span<Elf_Phdr>(ptr, this->phdrNum);
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

/**
 * Extracts all symbols from the library, and registers them with the linker. We'll in turn point
 * at the specified runtime library structure.
 */
void ElfLibReader::exportSymbols(Library *lib) {
    for(const auto &sym : this->symtab) {
        // ignore symbols that aren't defined in this file
        if(sym.st_shndx == STN_UNDEF) continue;

        // get its name
        auto namePtr = this->readStrtab(sym.st_name);

        // XXX: skip _start exported from dynamic libs
        if(!strcmp(namePtr, "_start")) {
            continue;
        }

        auto name = lib->strings.add(namePtr);

        // register it
        Linker::the()->exportSymbol(name, sym, lib);
    }
}

/**
 * Extracts initializers and destructors from the binary.
 *
 * Initializers are taken from the value of the DT_INIT pointer, if present, then the contents of
 * the DT_INIT_ARRAY array.
 *
 * Destructors are read in the same way, first from the DT_FINI pointer, then the contents of the
 * DT_FINI_ARRAY array.
 *
 * It doesn't seem like the DT_INIT/DT_FINI constructs are exported by clang/lld, but they're
 * included for compatibility.
 */
void ElfLibReader::exportInitFiniFuncs(Library *lib) {
    uintptr_t initArrayAddr = 0, finiArrayAddr = 0;
    size_t initArrayLen = 0, finiArrayLen = 0;

    // find the old style INIT/FINI functions, and the addresses of the new arrays
    for(const auto &dyn : this->dynInfo) {
        switch(dyn.d_tag) {
            case DT_INIT: {
                const auto addr = this->rebaseVmAddr(dyn.d_un.d_ptr);
                auto func = reinterpret_cast<void(*)(void)>(addr);
                lib->initFuncs.push_back(func);
                break;
            }
            case DT_FINI: {
                const auto addr = this->rebaseVmAddr(dyn.d_un.d_ptr);
                auto func = reinterpret_cast<void(*)(void)>(addr);
                lib->finiFuncs.push_back(func);
                break;
            }

            case DT_INIT_ARRAY:
                initArrayAddr = this->rebaseVmAddr(dyn.d_un.d_ptr);
                break;
            case DT_INIT_ARRAYSZ:
                initArrayLen = dyn.d_un.d_val;
                break;
            case DT_FINI_ARRAY:
                finiArrayAddr = this->rebaseVmAddr(dyn.d_un.d_ptr);
                break;
            case DT_FINI_ARRAYSZ:
                finiArrayLen = dyn.d_un.d_val;
                break;

            default:
                continue;
        }
    }

    if(initArrayLen) {
        // read out the number of functions
        auto read = reinterpret_cast<const void *>(initArrayAddr);
        const auto numEntries = initArrayLen / sizeof(uintptr_t);

        for(size_t i = 0; i < numEntries; i++) {
            // read the value and add it to init func list
            uintptr_t addr = 0;
            memcpy(&addr, read, sizeof(uintptr_t));

            // valuse of 0 or -1 are ignored
            if(!addr || addr == -1) continue;

            // otherwise, store it
            addr = this->rebaseVmAddr(addr);

            auto func = reinterpret_cast<void(*)(void)>(addr);
            lib->initFuncs.push_back(func);

            // advance the pointer
            read = reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(read) + sizeof(uintptr_t));
        }
    }

    if(finiArrayLen) {
        // read out the number of functions
        auto read = reinterpret_cast<const void *>(finiArrayAddr);
        const auto numEntries = finiArrayLen / sizeof(uintptr_t);

        for(size_t i = 0; i < numEntries; i++) {
            // read the value and add it to destructor func list
            uintptr_t addr = 0;
            memcpy(&addr, read, sizeof(uintptr_t));
            addr = this->rebaseVmAddr(addr);

            auto func = reinterpret_cast<void(*)(void)>(addr);
            lib->finiFuncs.push_back(func);

            // advance the pointer
            read = reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(read) + sizeof(uintptr_t));
        }
    }
}

/**
 * Exports the library's thread-local storage requirements.
 */
void ElfLibReader::exportThreadLocals(Library *lib) {
    // read program headers
    auto phdrs = reinterpret_cast<Elf_Phdr *>(malloc(sizeof(Elf_Phdr) * this->phdrNum));
    if(!phdrs) {
        Linker::Abort("out of memory");
    }

    this->read(sizeof(Elf_Phdr) * this->phdrNum, phdrs, this->phdrOff);

    for(size_t i = 0; i < this->phdrNum; i++) {
        // ignore all non-TLS segments
        const auto &hdr = phdrs[i];
        if(hdr.p_type != PT_TLS) continue;

        // get the .tdata (initialized) TLS info
        std::span<std::byte> tdata;

        if(hdr.p_filesz) {
            tdata = std::span<std::byte>(reinterpret_cast<std::byte *>(hdr.p_vaddr), hdr.p_filesz);
        }

        // record this information, alongside the TOTAL size of the TLS
        const size_t tlsSize = hdr.p_memsz;
        Linker::the()->setLibTlsRequirements(tlsSize, tdata, lib);
    }

    // clean up
    free(phdrs);
}

