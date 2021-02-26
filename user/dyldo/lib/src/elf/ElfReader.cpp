#include "ElfReader.h"
#include "Linker.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <errno.h>
#include <unistd.h>
#include <sys/elf.h>

using namespace dyldo;

/// whether we output logging information about loaded segments
bool ElfReader::gLogSegments = false;

/**
 * Creates an ELF reader for an already opened file.
 */
ElfReader::ElfReader(FILE * _Nonnull fp) : file(fp) {
    this->ownsFile = false;

    this->getFilesize();
    this->validateHeader();
}


/**
 * Opens the file at the provided path. If unable, the program is terminated.
 */
ElfReader::ElfReader(const char *path) {
    // open the file
    FILE *fp = fopen(path, "rb");
    if(!fp) {
        fprintf(stderr, "Failed to open executable '%s': %d", path, errno);
        _Exit(-1);
    }

    this->file = fp;
    this->ownsFile = true;

    this->getFilesize();
    this->validateHeader();
}

/**
 * Destroys the ELF reader's data structures.
 */
ElfReader::~ElfReader() {
    if(this->ownsFile) {
        fclose(this->file);
    }
}

/**
 * Figures out the size of the file.
 */
void ElfReader::getFilesize() {
    int err = fseek(this->file, 0, SEEK_END);
    if(err) {
        Linker::Abort("%s failed: %d %d", "seek", err, errno);
    }
    err = ftell(this->file);
    if(err < 0) {
        Linker::Abort("%s failed: %d %d", "ftell", err, errno);
    }
    this->fileSize = err;

    err = fseek(this->file, 0, SEEK_SET);
    if(err) {
        Linker::Abort("%s failed: %d %d", "seek", err, errno);
    }
}

/**
 * Validates an ELF header.
 */
void ElfReader::validateHeader() {
    int err;

    // read out the header
    Elf32_Ehdr hdr;
    memset(&hdr, 0, sizeof(Elf32_Ehdr));
    this->read(sizeof(Elf32_Ehdr), &hdr, 0);

    // ensure magic is correct, before we try and instantiate an ELF reader
    err = strncmp(reinterpret_cast<const char *>(hdr.e_ident), ELFMAG, SELFMAG);
    if(err) {
        Linker::Abort("Invalid ELF magic: %02x%02x%02x%02x", hdr.e_ident[0], hdr.e_ident[1],
                hdr.e_ident[2], hdr.e_ident[3]);
    }

    // We currently only handle 32-bit ELF
    if(hdr.e_ident[EI_CLASS] != ELFCLASS32) {
        Linker::Abort("Unsupported ELF class: %u", hdr.e_ident[EI_CLASS]);
    }

    // ensure the ELF is little endian, the correct version
    if(hdr.e_ident[EI_DATA] != ELFDATA2LSB) {
        Linker::Abort("Invalid ELF format: %02x", hdr.e_ident[EI_DATA]);
    }

    if(hdr.e_ident[EI_VERSION] != EV_CURRENT) {
        Linker::Abort("Invalid ELF version (%s): %02x", "ident", hdr.e_ident[EI_VERSION]);
    } else if(hdr.e_version != EV_CURRENT) {
        Linker::Abort("Invalid ELF version (%s): %08x", "header", hdr.e_version);
    }

    // ensure CPU architecture
#if defined(__i386__)
    if(hdr.e_machine != EM_386) {
        Linker::Abort("Invalid ELF machine type %08x", hdr.e_type);
    }
#else
#error Update library loader to handle the current arch
#endif

    // read section header info
    if(hdr.e_shentsize != sizeof(Elf32_Shdr)) {
        Linker::Abort("Invalid %s header size %u", "section", hdr.e_shentsize);
    }

    this->shdrOff = hdr.e_shoff;
    this->shdrNum = hdr.e_shnum;

    // read program header info
    if(hdr.e_phentsize != sizeof(Elf32_Phdr)) {
        Linker::Abort("Invalid %s header size %u", "program", hdr.e_phentsize);
    }

    this->phdrOff = hdr.e_phoff;
    this->phdrNum = hdr.e_phnum;

    if(!this->phdrNum) {
        Linker::Abort("No program headers in ELF");
    }
}

/**
 * Reads the given number of bytes from the file at the specified offset.
 */
void ElfReader::read(const size_t nBytes, void * _Nonnull out, const uintptr_t offset) {
    int err;

    // seek
    err = fseek(this->file, offset, SEEK_SET);
    if(err) {
        Linker::Abort("%s failed: %d %d", "seek", err, errno);
    }

    // read
    err = fread(out, 1, nBytes, this->file);
    if(err == nBytes) {
        // success!
    } else if(err > 0) {
        Linker::Abort("Partial read: got %d, expected %u", err, nBytes);
    } else {
        Linker::Abort("%s failed: %d %d", "read", err, errno);
    }
}

/**
 * Reads a string out of the string table.
 *
 * Generally, you should copy the key if you need it to stick around during program runtime.
 */
const char *ElfReader::readStrtab(const size_t i) {
    // ensure strtab is loaded
    if(this->strtab.empty()) return nullptr;

    // get subrange
    auto sub = this->strtab.subspan(i);
    if(sub.empty()) {
        return nullptr;
    }
    if(sub[0] == '\0') {
        return nullptr;
    }

    return sub.data();
}

/**
 * Parses the .dynamic section.
 *
 * Subclasses should invoke this after they set the `dynInfo` variable.
 */
void ElfReader::parseDynamicInfo() {
    // extract the string table and symbol table offset
    uintptr_t strtabAddr = 0, symtabAddr = 0;
    size_t strtabLen = 0, symtabItemLen = 0;

    for(const auto &entry : this->dynInfo) {
        switch(entry.d_tag) {
            case DT_STRTAB:
                strtabAddr = this->rebaseVmAddr(entry.d_un.d_ptr);
                break;
            case DT_STRSZ:
                strtabLen = entry.d_un.d_val;
                break;

            case DT_SYMTAB:
                symtabAddr = this->rebaseVmAddr(entry.d_un.d_ptr);
                break;
            case DT_SYMENT:
                symtabItemLen = entry.d_un.d_val;
                break;

            default:
                break;
        }
    }

    if(!strtabAddr || !strtabLen) {
        Linker::Abort("missing strtab");
    }
    this->strtab = std::span<char>(reinterpret_cast<char *>(strtabAddr), strtabLen);

    // parse section headers for the other stufs
    const auto shdrBytes = (sizeof(Elf32_Shdr) * this->shdrNum);
    auto shdrs = reinterpret_cast<Elf32_Shdr *>(malloc(shdrBytes));
    if(!shdrs) Linker::Abort("out of memory");

    this->read(shdrBytes, shdrs, this->shdrOff);

    for(size_t i = 0; i < this->shdrNum; i++) {
        const auto &hdr = shdrs[i];

        // extract fixed information
        switch(hdr.sh_type) {
            // dynamic symbol table
            case SHT_DYNSYM: {
                const auto nSyms = hdr.sh_size / symtabItemLen;
                this->symtab = std::span<Elf32_Sym>(reinterpret_cast<Elf32_Sym *>(symtabAddr), nSyms);
                break;
            }

            // ignore other types of sections
            default:
                break;
        }
    }

    // read dependencies
    this->readDeps();

    // clean up
    free(shdrs);
}

/**
 * From the dynamic information, extract the location of the relocation table and outputs it.
 *
 * @param outRels If data relocations exist, set to the span encompassing all of them.
 * @return Whether there are relocations to process.
 */
bool ElfReader::getDynRels(std::span<Elf32_Rel> &outRels) {
    // get the extents of the region
    uintptr_t relAddr = 0;
    size_t relEntBytes = 0, relBytes = 0;

    for(const auto &entry : this->dynInfo) {
        switch(entry.d_tag) {
            case DT_REL:
                relAddr = this->rebaseVmAddr(entry.d_un.d_ptr);
                break;
            case DT_RELENT:
                relEntBytes = entry.d_un.d_val;
                break;
            case DT_RELSZ:
                relBytes = entry.d_un.d_val;
                break;
            default:
                continue;
        }
    }

    if(!relAddr && !relEntBytes && !relBytes) {
        // no relocations to process
        return false;
    } else if(!relAddr || !relEntBytes || !relBytes) {
        // one of the relocation fields is missing
        Linker::Abort("failed to read %s relocs: REL %u ENT %u SZ %u", "data", relAddr,
                relEntBytes, relBytes);
    }
    else if(relEntBytes != sizeof(Elf32_Rel)) {
        Linker::Abort("unsupported relent size %u", relEntBytes);
    }

    // process each relocation
    const auto numRels = relBytes / relEntBytes;
    outRels = std::span<Elf32_Rel>(reinterpret_cast<Elf32_Rel *>(relAddr), numRels);

    return true;
}

/**
 * From the dynamic information, extract the location of the PLT relocations.
 *
 * @param outRels If relocations exist, set to the span encompassing all of them.
 * @return Whether there are PLT relocations to process.
 */
bool ElfReader::getPltRels(std::span<Elf32_Rel> &outRels) {
    // get the extents of the region
    uintptr_t relAddr = 0;
    size_t relEntBytes = 0, relBytes = 0;

    for(const auto &entry : this->dynInfo) {
        switch(entry.d_tag) {
            case DT_JMPREL:
                relAddr = this->rebaseVmAddr(entry.d_un.d_ptr);
                break;
            case DT_PLTREL:
                relEntBytes = (entry.d_un.d_val == DT_REL) ? sizeof(Elf32_Rel) : sizeof(Elf32_Rela);
                break;
            case DT_PLTRELSZ:
                relBytes = entry.d_un.d_val;
                break;
            default:
                continue;
        }
    }

    if(!relAddr && !relEntBytes && !relBytes) {
        // no relocations to process
        return false;
    } else if(!relAddr || !relEntBytes || !relBytes) {
        // one of the relocation fields is missing
        Linker::Abort("failed to read %s relocs: REL %u ENT %u SZ %u", "PLT", relAddr, relEntBytes,
                relBytes);
    }
    else if(relEntBytes != sizeof(Elf32_Rel)) {
        Linker::Abort("unsupported relent size %u", relEntBytes);
    }

    // process each relocation
    const auto numRels = relBytes / relEntBytes;
    outRels = std::span<Elf32_Rel>(reinterpret_cast<Elf32_Rel *>(relAddr), numRels);

    return true;
}

/**
 * Processes relocations in the object.
 *
 * @param base An offset to add to virtual addresses of symbols to turn them into absolute addresses.
 */
void ElfReader::patchRelocs(const std::span<Elf32_Rel> &rels, const uintptr_t base) {
    const SymbolMap::Symbol *symbol;

    // process each relocation
    for(const auto &rel : rels) {
        const auto type = ELF32_R_TYPE(rel.r_info);

        // resolve symbol if needed
        switch(type) {
            // all these require symbol resolution
            case R_386_COPY:
            case R_386_GLOB_DAT:
            case R_386_JMP_SLOT:
            case R_386_32:
            {
                // translate the symbol index into a name
                const auto symIdx = ELF32_R_SYM(rel.r_info);
                const auto sym = this->symtab[symIdx];
                const auto name = this->readStrtab(sym.st_name);
                if(!name) {
                    Linker::Abort("failed to resolve name for symbol %u", symIdx);
                }

                // resolve to symbol
                symbol = Linker::the()->resolveSymbol(name);
                if(!symbol) {
                    Linker::Abort("failed to resolve symbol '%s'", name);
                }
                break;
            }

            // no symbol
            default:
                symbol = nullptr;
                break;
        }

        // invoke the processing routine
        switch(type) {
            /**
             * Reads a dword at the specified offset, and add to it our load address, then write
             * it back.
             *
             * This only makes sense in shared libraries.
             */
            case R_386_RELATIVE: {
                uintptr_t value = 0;
                auto from = reinterpret_cast<void *>(base + rel.r_offset);
                memcpy(&value, from, sizeof(uintptr_t));
                value += base;
                memcpy(from, &value, sizeof(uintptr_t));
                break;
            }

            /*
             * Copy data from the named symbol, located in a shared library, into our data segment
             * somewhere.
             *
             * After the copy is completed, we override the symbol with the address of the copied
             * data in our data segment. This way, when we perform relocations on shared objects
             * next, they reference this one copy of the symbols, rather than the read-only
             * 'template' of them in the library
             */
            case R_386_COPY: {
                this->relocCopyFromShlib(rel, symbol);
                Linker::the()->overrideSymbol(symbol, rel.r_offset);
                break;
            }

            /**
             * References global data that was previously copied into the app's data segment.
             *
             * This is the complement to the R_386_COPY relocation type.
             */
            case R_386_GLOB_DAT: {
                auto from = reinterpret_cast<void *>(base + rel.r_offset);
                memcpy(from, &symbol->address, sizeof(uintptr_t));
                break;
            }

            /**
             * Updates an entry in the PLT (jump slot) with the address of a symbol.
             */
            case R_386_JMP_SLOT: {
                auto from = reinterpret_cast<void *>(base + rel.r_offset);
                memcpy(from, &symbol->address, sizeof(uintptr_t));
                break;
            }

            /**
             * Write the absolute address of a resolved symbol into the offset specified.
             */
            case R_386_32: {
                uintptr_t value = 0;
                auto from = reinterpret_cast<void *>(base + rel.r_offset);
                memcpy(&value, from, sizeof(uintptr_t));
                value += symbol->address;
                memcpy(from, &value, sizeof(uintptr_t));
                break;
            }

            /// unknown relocation type
            default:
                Linker::Abort("unsupported relocation: type %u (off %08x info %08x)",
                        ELF32_R_TYPE(rel.r_info), rel.r_offset, rel.r_info);
        }
    }
}
/**
 * Parses all of the DT_NEEDED entries out of the provided dynamic table, and creates an entry for
 * the associated library.
 *
 * We expect that the string table is already cached at this point.
 */
void ElfReader::readDeps() {
    for(const auto &dyn : this->dynInfo) {
        if(dyn.d_tag != DT_NEEDED) continue;

        // read the name string
        auto name = this->readStrtab(dyn.d_un.d_val);
        if(!name) {
            Linker::Abort("invalid DT_NEEDED symbol: %u", dyn.d_un.d_val);
        }

        // allocate a needs struct for it
        this->deps.emplace_back(name);
    }
}



/**
 * Loads the data referenced by the given file segment.
 *
 * TODO: Share text segments!
 *
 * @param base An offset to add to all virtual address offsets in the file. Should be 0 if the file
 * is an executable, otherwise the load address of the dynamic library.
 */
void ElfReader::loadSegment(const Elf32_Phdr &phdr, const uintptr_t base) {
    int err;
    uintptr_t vmRegion;

    const auto pageSz = sysconf(_SC_PAGESIZE);
    if(pageSz <= 0) {
        Linker::Abort("failed to determine page size");
    }

    // fill in base information
    Segment seg;
    seg.offset = phdr.p_offset;
    seg.length = phdr.p_memsz;

    if(phdr.p_flags & PF_R) {
        seg.protection |= SegmentProtection::Read;
    }
    if(phdr.p_flags & PF_W) {
        seg.protection |= SegmentProtection::Write;
    }
    if(phdr.p_flags & PF_X) {
        seg.protection |= SegmentProtection::Execute;
    }

    // align to virtual memory base
    const auto vmBase = phdr.p_vaddr + base;

    seg.vmStart = (vmBase / pageSz) * pageSz;
    seg.vmEnd = ((((vmBase + phdr.p_memsz) + pageSz - 1) / pageSz) * pageSz) - 1;

    if(gLogSegments) {
        Linker::Trace("Segment off %08x length %08x va %08x: %08x - %08x", seg.offset, seg.length,
                phdr.p_vaddr, seg.vmStart, seg.vmEnd);
    }

    // allocate the page
    err = AllocVirtualAnonRegion(seg.vmStart, (seg.vmEnd - seg.vmStart) + 1, VM_REGION_RW,
            &vmRegion);
    if(err) {
        Linker::Abort("failed to allocate anon region: %d", err);
    }
    seg.vmRegion = vmRegion;

    // copy into the page
    if(phdr.p_filesz) {
        auto copyTo = reinterpret_cast<void *>(seg.vmStart + (phdr.p_offset % pageSz));
        this->read(phdr.p_filesz, copyTo, phdr.p_offset);
    }
    // zero remaining area
    if(phdr.p_memsz > phdr.p_filesz) {
        auto zeroStart = reinterpret_cast<void *>(seg.vmStart + (phdr.p_offset % pageSz) + phdr.p_filesz);
        const auto numZeroBytes = phdr.p_memsz - phdr.p_filesz;

        if(gLogSegments) {
            Linker::Trace("Zeroing %u bytes at %p", numZeroBytes, zeroStart);
        }

        memset(zeroStart, 0, numZeroBytes);
    }

    // store info
    this->segments.push_back(std::move(seg));
}

/**
 * Apply the correct protection flags for all mapped segments.
 */
void ElfReader::applyProtection() {
    int err;

    for(const auto &seg : this->segments) {
        // always readable
        uintptr_t flags = VM_REGION_READ;

        if(TestFlags(seg.protection & SegmentProtection::Write)) {
            flags |= VM_REGION_WRITE;
        }
        if(TestFlags(seg.protection & SegmentProtection::Execute)) {
            flags |= VM_REGION_EXEC;
        }

        // warn if WX
        if((flags & VM_REGION_EXEC) && (flags & VM_REGION_WRITE)) {
            Linker::Info("W+X mapping at %08x for %p", seg.vmStart, this);
        }

        err = VirtualRegionSetFlags(seg.vmRegion, flags);
        if(err) {
            Linker::Abort("failed to update segment protection: %d", err);
        }
    }
}



/**
 * Copies data out of a shared object. This implements the `R_386_COPY` relocation type.
 *
 * @param base Offset to add to the offset field in the relocation to get an absolute address.
 */
void ElfReader::relocCopyFromShlib(const Elf32_Rel &rel, const SymbolMap::Symbol *sym,
        const uintptr_t base) {
    auto dest = reinterpret_cast<void *>(rel.r_offset + base);
    auto from = reinterpret_cast<const void *>(sym->address);

    memcpy(dest, from, sym->length);
}
