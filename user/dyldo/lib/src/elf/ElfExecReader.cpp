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
}

/**
 * Initializes an executable ELF reader. This ensures the binary being loaded is a dynamic
 * executable, and that we can otherwise load it.
 */
ElfExecReader::ElfExecReader(const char *path) : ElfReader(path) {
    this->ensureExec();
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
    Elf_Ehdr hdr;
    memset(&hdr, 0, sizeof hdr);
    this->read(sizeof hdr, &hdr, 0);

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
                const auto base = reinterpret_cast<Elf_Dyn *>(phdr.p_vaddr);
                this->dynInfo = std::span<Elf_Dyn>(base, num);
                break;
            }

            // define the executable's thread-local region
            case PT_TLS:
                this->loadTlsTemplate(phdr);
                break;

            // program headers in memory
            case PT_PHDR: {
                auto ptr = reinterpret_cast<Elf_Phdr *>(phdr.p_vaddr);
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
 * Initializes the thread-local information. This consists of the "template" of values to yeet into
 * the task's TLS region, as well as its size.
 *
 * It's assumed that all of the template data (i.e. what's not in the .tbss) is mapped into the
 * address space of the process.
 */
void ElfExecReader::loadTlsTemplate(const Elf_Phdr &hdr) {
    // get the .tdata (initialized) TLS info
    std::span<std::byte> tdata;

    if(hdr.p_filesz) {
        tdata = std::span<std::byte>(reinterpret_cast<std::byte *>(hdr.p_vaddr), hdr.p_filesz);
    }

    // record this information, alongside the TOTAL size of the TLS
    const size_t tlsSize = hdr.p_memsz;
    Linker::the()->setExecTlsRequirements(tlsSize, tdata);
}

/**
 * Extracts initializers and destructors from the binary.
 */
void ElfExecReader::exportInitFiniFuncs() {
    uintptr_t initArrayAddr = 0, finiArrayAddr = 0;
    size_t initArrayLen = 0, finiArrayLen = 0;

    auto linker = Linker::the();

    // find the old style INIT/FINI functions, and the addresses of the new arrays
    for(const auto &dyn : this->dynInfo) {
        switch(dyn.d_tag) {
            case DT_INIT: {
                const auto addr = this->rebaseVmAddr(dyn.d_un.d_ptr);
                auto func = reinterpret_cast<void(*)(void)>(addr);
                linker->execInitFuncs.push_back(func);
                break;
            }
            case DT_FINI: {
                const auto addr = this->rebaseVmAddr(dyn.d_un.d_ptr);
                auto func = reinterpret_cast<void(*)(void)>(addr);
                linker->execFiniFuncs.push_back(func);
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
            linker->execInitFuncs.push_back(func);

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
            linker->execFiniFuncs.push_back(func);

            // advance the pointer
            read = reinterpret_cast<const void *>(reinterpret_cast<uintptr_t>(read) + sizeof(uintptr_t));
        }
    }
}

