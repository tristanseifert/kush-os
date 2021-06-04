#include "ElfReader.h"
#include "Library.h"
#include "Linker.h"
#include "runtime/ThreadLocal.h"

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
    Elf_Ehdr hdr;
    memset(&hdr, 0, sizeof hdr);
    this->read(sizeof hdr, &hdr, 0);

    // ensure magic is correct, before we try and instantiate an ELF reader
    err = strncmp(reinterpret_cast<const char *>(hdr.e_ident), ELFMAG, SELFMAG);
    if(err) {
        Linker::Abort("Invalid ELF magic: %02x%02x%02x%02x", hdr.e_ident[0], hdr.e_ident[1],
                hdr.e_ident[2], hdr.e_ident[3]);
    }

    // validate ELF class based on current architecture
    switch(hdr.e_ident[EI_CLASS]) {
#if defined(__i386__)
        case ELFCLASS32: break;
#elif defined(__amd64__)
        case ELFCLASS64: break;
#endif

        // unsupported ELF class
        default:
            Linker::Abort("Unsupported ELF class: %u", hdr.e_ident[EI_CLASS]);
    }
    this->elfClass = hdr.e_ident[EI_CLASS];

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
    switch(hdr.e_machine) {
#if defined(__i386__)
        case EM_386: break;
#elif defined(__amd64__)
        case EM_X86_64: break;
#endif
        default:
            Linker::Abort("Invalid ELF machine type %08x", hdr.e_type);
    }
    this->elfMachine = hdr.e_machine;

    // read section header info
    if(hdr.e_shentsize != sizeof(Elf_Shdr)) {
        Linker::Abort("Invalid %s header size %u", "section", hdr.e_shentsize);
    }

    this->shdrOff = hdr.e_shoff;
    this->shdrNum = hdr.e_shnum;

    // read program header info
    if(hdr.e_phentsize != sizeof(Elf_Phdr)) {
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
    const auto shdrBytes = (sizeof(Elf_Shdr) * this->shdrNum);
    auto shdrs = reinterpret_cast<Elf_Shdr *>(malloc(shdrBytes));
    if(!shdrs) Linker::Abort("out of memory");

    this->read(shdrBytes, shdrs, this->shdrOff);

    for(size_t i = 0; i < this->shdrNum; i++) {
        const auto &hdr = shdrs[i];

        // extract fixed information
        switch(hdr.sh_type) {
            // dynamic symbol table
            case SHT_DYNSYM: {
                const auto nSyms = hdr.sh_size / symtabItemLen;
                this->symtab = std::span<Elf_Sym>(reinterpret_cast<Elf_Sym *>(symtabAddr), nSyms);
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
 * @param outRels If data relocations exist, set to a padded array encompassing all of them.
 * @return Whether there are relocations to process.
 */
bool ElfReader::getDynRels(PaddedArray<Elf_Rel> &outRels) {
    // get the extents of the region
    uintptr_t relAddr = 0;
    size_t relEntBytes = 0, relBytes = 0;
    bool isRela;

    // get the correct relocations type
    for(const auto &entry : this->dynInfo) {
        switch(entry.d_tag) {
#if defined(__i386__)
            case DT_REL:
                relAddr = this->rebaseVmAddr(entry.d_un.d_ptr);
                isRela = false;
                break;
            case DT_RELENT:
                relEntBytes = entry.d_un.d_val;
                break;
            case DT_RELSZ:
                relBytes = entry.d_un.d_val;
                break;
#elif defined(__amd64__)
            case DT_RELA:
                relAddr = this->rebaseVmAddr(entry.d_un.d_ptr);
                isRela = true;
                break;
            case DT_RELAENT:
                relEntBytes = entry.d_un.d_val;
                break;
            case DT_RELASZ:
                relBytes = entry.d_un.d_val;
                break;
#endif

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
    else if(!isRela && relEntBytes < sizeof(Elf_Rel)) {
        Linker::Abort("unsupported %s relent size %u (expected %lu)", "dynamic", relEntBytes,
                sizeof(Elf_Rel));
    }
    else if(isRela && relEntBytes < sizeof(Elf_Rela)) {
        Linker::Abort("unsupported %s relent size %u (expected %lu)", "dynamic", relEntBytes,
                sizeof(Elf_Rela));
    }

    // process each relocation
    const auto numRels = relBytes / relEntBytes;
    outRels = PaddedArray<Elf_Rel>(reinterpret_cast<Elf_Rel *>(relAddr), numRels, relEntBytes);

    return true;
}

/**
 * From the dynamic information, extract the location of the PLT relocations.
 *
 * @param outRels If relocations exist, set to the span encompassing all of them.
 * @return Whether there are PLT relocations to process.
 */
bool ElfReader::getPltRels(PaddedArray<Elf_Rel> &outRels) {
    // get the extents of the region
    uintptr_t relAddr = 0;
    size_t relEntBytes = 0, relBytes = 0;

    for(const auto &entry : this->dynInfo) {
        switch(entry.d_tag) {
            case DT_JMPREL:
                relAddr = this->rebaseVmAddr(entry.d_un.d_ptr);
                break;
            case DT_PLTREL:
                relEntBytes = (entry.d_un.d_val == DT_REL) ? sizeof(Elf_Rel) : sizeof(Elf_Rela);
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
    else if(relEntBytes < sizeof(Elf_Rel)) {
        Linker::Abort("unsupported %s relent size %u (expected %lu)", "PLT", relEntBytes,
                sizeof(Elf_Rel));
    }

    // process each relocation
    const auto numRels = relBytes / relEntBytes;
    outRels = PaddedArray<Elf_Rel>(reinterpret_cast<Elf_Rel *>(relAddr), numRels, relEntBytes);

    return true;
}

/**
 * Processes relocations in the object, invoking the correct architecture's code.
 *
 * @param base An offset to add to virtual addresses of symbols to turn them into absolute addresses.
 *
 * @note This will need to be updated any time additional architectures are to be supported.
 */
void ElfReader::patchRelocs(const PaddedArray<Elf_Rel> &rels, const uintptr_t base) {
    switch(this->elfMachine) {
#if defined(__i386__)
        case EM_386:
            return this->patchRelocsi386(rels, base);
#endif
#if defined(__amd64__)
        case EM_386:
            return this->patchRelocsi386(rels, base);
        case EM_X86_64: {
            // XXX: yuck... we really should do better than this
            auto rela = reinterpret_cast<const PaddedArray<Elf_Rela> &>(rels);
            return this->patchRelocsAmd64(rela, base);
        }
#endif
        default:
            Linker::Abort("don't know how to patch relocations for machine $%x", this->elfMachine);
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
void ElfReader::loadSegment(const Elf_Phdr &phdr, const uintptr_t base) {
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
        Linker::Trace("Segment off $%x length $%x va %p: %p - %p", seg.offset, seg.length,
                phdr.p_vaddr, seg.vmStart, seg.vmEnd);
    }

    // allocate the region
    const size_t regionLen = (seg.vmEnd - seg.vmStart) + 1;

    err = AllocVirtualAnonRegion(regionLen, VM_REGION_RW, &vmRegion);
    if(err) {
        Linker::Abort("failed to %s anon region: %d", "allocate", err);
    }
    seg.vmRegion = vmRegion;

    // map region
    err = MapVirtualRegion(vmRegion, seg.vmStart, regionLen, 0);
    if(err) {
        Linker::Abort("failed to %s anon region: %d", "map", err);
    }

    // copy into the page
    if(phdr.p_filesz) {
        auto copyTo = reinterpret_cast<void *>(seg.vmStart + (phdr.p_offset % pageSz));
        this->read(phdr.p_filesz, copyTo, phdr.p_offset);
    }
    // zero remaining area (XXX: technically, we can skip this as anon pages are faulted in zeroed)
    if(phdr.p_memsz > phdr.p_filesz) {
        auto zeroStart = reinterpret_cast<void *>(seg.vmStart + (phdr.p_offset % pageSz) + phdr.p_filesz);
        const auto numZeroBytes = phdr.p_memsz - phdr.p_filesz;

        if(gLogSegments) {
            Linker::Trace("Zeroing %lu bytes at %p", numZeroBytes, zeroStart);
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
            Linker::Info("W+X mapping at %p for %p", seg.vmStart, this);
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
void ElfReader::relocCopyFromShlib(const Elf_Rel &rel, const SymbolMap::Symbol *sym,
        const uintptr_t base) {
    auto dest = reinterpret_cast<void *>(rel.r_offset + base);
    auto from = reinterpret_cast<const void *>(sym->address);

    memcpy(dest, from, sym->length);
}

/**
 * Copies data out of a shared object. This implements the `R_X86_64_COPY` relocation type.
 *
 * @param base Offset to add to the offset field in the relocation to get an absolute address.
 */
void ElfReader::relocCopyFromShlib(const Elf_Rela &rel, const SymbolMap::Symbol *sym,
        const uintptr_t base) {
    auto dest = reinterpret_cast<void *>(rel.r_offset + base);
    auto from = reinterpret_cast<const void *>(sym->address);

    memcpy(dest, from, sym->length);
}
