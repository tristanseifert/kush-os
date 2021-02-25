#include "ElfReader.h"
#include "Linker.h"

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <errno.h>
#include <unistd.h>
#include <sys/elf.h>

using namespace dyldo;

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

    // read dependencies
    this->readDeps();
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

    Linker::Trace("Segment off %08x length %08x va %08x: %08x - %08x", seg.offset, seg.length, phdr.p_vaddr, seg.vmStart, seg.vmEnd);

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

        Linker::Trace("Zeroing %u bytes at %p", numZeroBytes, zeroStart);

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
