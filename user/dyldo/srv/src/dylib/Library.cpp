#include "Library.h"

#include "log.h"

#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

#include <unistd.h>
#include <sys/elf.h>

using namespace dylib;

/**
 * Opens the given file, and attempts to load a library from it.
 */
std::shared_ptr<Library> Library::loadFile(const std::string &path) {
    // try to open it
    auto file = fopen(path.c_str(), "rb");
    if(!file) {
        return nullptr;
    }

    // allocate a library and perform ELF validation
    auto lib = std::make_shared<Library>(file);

    if(!lib->validateHeader()) {
        L("Invalid ELF header for library '{}'", path);
        return nullptr;
    }

    // read out its information
    if(!lib->readSegments()) {
        L("Invalid ELF segments for library '{}'", path);
        return nullptr;
    }
    if(!lib->readDynInfo()) {
        L("Invalid ELF dynamic info for library '{}'", path);
        return nullptr;
    }

    // if we get here, it's a valid library and we have some info on it
    return lib;
}



/**
 * Creates a library that loads data from the given file.
 */
Library::Library(FILE *_file) : file(_file) {
    // don't really have anything to do
}

/**
 * Releases library information.
 */
Library::~Library() {
    // close file handle if not done already
    if(this->file) {
        fclose(this->file);
    }
}

/**
 * Try to read the ELF header.
 */
bool Library::validateHeader() {
    int err;

    // read the header
    std::vector<Elf32_Ehdr> buf;
    buf.resize(1);

    err = fread(buf.data(), sizeof(Elf32_Ehdr), 1, this->file);
    if(err != sizeof(Elf32_Ehdr)) return false;

    auto hdr = reinterpret_cast<const Elf32_Ehdr *>(buf.data());

    // ensure magic is correct, before we try and instantiate an ELF reader
    err = strncmp(reinterpret_cast<const char *>(hdr->e_ident), ELFMAG, SELFMAG);
    if(err) {
        L("Invalid ELF header: {:02x} {:02x} {:02x} {:02x}", hdr->e_ident[0], hdr->e_ident[1],
                hdr->e_ident[2], hdr->e_ident[3]);
        return false;
    }

    // use the class value to pick a reader (32 vs 64 bits)
    if(hdr->e_ident[EI_CLASS] != ELFCLASS32) {
        L("Invalid ELF class: {:02x}", hdr->e_ident[EI_CLASS]);
        return false;
    }
    // ensure the ELF is little endian, the correct version
    if(hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        L("Invalid ELF format: {:02x}", hdr->e_ident[EI_DATA]);
        return false;
    }

    if(hdr->e_ident[EI_VERSION] != EV_CURRENT) {
        L("Invalid ELF version ({}): {:02x}", "ident", hdr->e_ident[EI_VERSION]);
        return false;
    } else if(hdr->e_version != EV_CURRENT) {
        L("Invalid ELF version ({}): {:08x}", "header", hdr->e_version);
        return false;
    }

    // ensure it's a dynamic object for the correct CPU arch
    if(hdr->e_type != ET_DYN) {
        L("Invalid ELF type {:08x}", hdr->e_type);
        return false;
    }

    // ensure CPU architecture
#if defined(__i386__)
    if(hdr->e_machine != EM_386) {
        L("Invalid ELF machine type {:08x}", hdr->e_type);
        return false;
    }
#else
#error Update library loader to handle the current arch
#endif

    // ensure the program header and section header sizes make sense
    if(hdr->e_shentsize != sizeof(Elf32_Shdr)) {
        L("Invalid section header size {}", hdr->e_shentsize);
        return false;
    }
    else if(hdr->e_phentsize != sizeof(Elf32_Phdr)) {
        L("Invalid program header size {}", hdr->e_phentsize);
        return false;
    }
    this->phdrOff = hdr->e_phoff;
    this->phdrNum = hdr->e_phnum;

    if(!this->phdrNum) {
        L("No program headers in ELF");
        return false;
    }

    // the header is sane
    return true;
}

/**
 * Reads the program headers (segments) to determine how much VM space we need for the library.
 */
bool Library::readSegments() {
    int err;

    // read out program headers
    std::vector<Elf32_Phdr> buf;
    buf.resize(this->phdrNum);

    err = fseek(this->file, this->phdrOff, SEEK_SET);
    if(err) {
        L("Failed to seek to program headers (off {}): {} {}", this->phdrOff, err, errno);
        return false;
    }

    err = fread(buf.data(), sizeof(Elf32_Phdr), buf.size(), this->file);
    if(err != sizeof(Elf32_Phdr) * this->phdrNum) {
        L("Failed to read {} program headers (got {} bytes)", this->phdrNum, err);
        return false;
    }

    // process each segment
    for(size_t i = 0; i < this->phdrNum; i++) {
        if(!this->processSegment(buf[i])) {
            return false;
        }
    }

    // calculate page aligned bounds for the segment virtual memory regions
    const auto pageSz = sysconf(_SC_PAGESIZE);
    if(pageSz <= 0) return false;

    for(auto &segment : this->segments) {
        // page align start, round end address up
        const auto aBase = segment.base & ~(pageSz - 1);
        const auto aEnd = ((((segment.base + segment.length) + pageSz - 1) / pageSz) * pageSz) - 1;

        L("Segment {:08x} - {:08x}, aligned {:08x} - {:08x}", segment.base,
                segment.base + segment.length, aBase, aEnd);

        segment.vmStart = aBase;
        segment.vmEnd = aEnd;
    }

    return true;
}

/**
 * Processes a program header.
 *
 * We use this to find all loadable segments (to determine how much virtual memory to allocate) and
 * to discover the dynamic info region.
 */
bool Library::processSegment(const Elf32_Phdr &phdr) {
    switch(phdr.p_type) {
        // load command: allocate the VM region
        case PT_LOAD:
            return this->processSegmentLoad(phdr);

        // references the dynamic region
        case PT_DYNAMIC:
            this->dynOff = phdr.p_offset;
            this->dynLen = phdr.p_filesz;
            return true;

        // ignore this segment, it is an unhandled type
        default:
            return true;
    }
}

/**
 * Processes a load command segment.
 *
 * This notes down the virtual memory space, and what region of the file (if any) it's backed by.
 */
bool Library::processSegmentLoad(const Elf32_Phdr &phdr) {
    // create segment info
    Segment info;
    info.base = phdr.p_vaddr;
    info.length = phdr.p_memsz;
    info.fileOff = phdr.p_offset;
    info.fileCopyBytes = phdr.p_filesz;

    // ensure it doesn't overlap any existing segments
    for(const auto &segment : this->segments) {
        if(segment.overlaps(info)) {
            L("Overlap between segments! (this {:x}-{:x}, conflict with {:x}-{:x})", info.base,
                    info.base + info.length, segment.base, segment.base + segment.length);
            return false;
        }
    }

    // insert it
    this->segments.push_back(std::move(info));
    return true;
}

/**
 * Reads the dynamic info section to extract the names of dependent libraries, our library name,
 * and some other information useful later for dynamic linking.
 */
bool Library::readDynInfo() {
    int err;

    // read the dynamic linker info table
    if(!this->dynOff || !this->dynLen) {
        L("Invalid .dynamic offset {} length {}", this->dynOff, this->dynLen);
        return false;
    }

    const auto numEntries = this->dynLen / sizeof(Elf32_Dyn);
    std::vector<Elf32_Dyn> dyn;
    dyn.resize(numEntries);

    err = fseek(this->file, this->dynOff, SEEK_SET);
    if(err) {
        L("Failed to seek to .dynamuc (off {}): {} {}", this->dynOff, err, errno);
        return false;
    }

    err = fread(dyn.data(), sizeof(Elf32_Dyn), dyn.size(), this->file);
    if(err != sizeof(Elf32_Dyn) * numEntries) {
        L("Failed to read {} dynamic entries (got {} bytes)", numEntries, err);
        return false;
    }

    // convert the entries to a key -> value type dealio and extract mandatory values
    DynMap dynTable;
    dynTable.reserve(dyn.size());

    for(const auto &entry : dyn) {
        dynTable.emplace(entry.d_tag, entry.d_un.d_val);
    }

    if(!this->readDynMandatory(dynTable)) {
        return false;
    }

    // read out the soname and dependant libraries
    auto soname = dynTable.find(DT_SONAME);

    if(soname != dynTable.end()) {
        auto val = this->readStrtabSlow(soname->second);
        if(val) {
            this->soname = *val;
        }
    }

    auto deps = dynTable.equal_range(DT_NEEDED);
    for(auto it = deps.first; it != deps.second; ++it) {
        auto name = this->readStrtabSlow(it->second);
        if(!name) {
            return false;
        }

        this->depNames.push_back(std::move(*name));
    }

    return true;
}

/**
 * Reads the mandatory dynamic table entries. This consists of the string and symbol table offsets,
 * and checking for the NULL entry.
 */
bool Library::readDynMandatory(const DynMap &map) {
    // string table offset
    auto strtab = map.find(DT_STRTAB);
    auto strsz = map.find(DT_STRSZ);
    if(strtab == map.end() || strsz == map.end()) {
        return false;
    }
    this->strtabExtents = std::make_pair(strtab->second, strsz->second);

    // symbol table offset
    auto symtab = map.find(DT_SYMTAB);
    auto syment = map.find(DT_SYMENT);

    if(symtab == map.end() || syment == map.end()) {
        return false;
    }

    this->symtabOff = symtab->second;
    this->symtabEntSz = syment->second;

    // ensure there's a DT_NULL entry
    if(!map.contains(DT_NULL)) {
        return false;
    }

    return true;
}

/**
 * Reads from the string table of the binary. This reads into a small buffer up to `maxLen` bytes
 * from the file, limiting it to the maximum size of the string table if necessary. A string is
 * then returned.
 *
 * If the string contains a single null byte, we interpret this to mean "no string" and return an
 * empty string. Null values represent errors.
 *
 * @param maxLen Maximum length to reserve for the symbol read buffer. This means, a symbol longer
 * than this value will be truncated.
 */
std::optional<std::string> Library::readStrtabSlow(const uintptr_t off, const size_t maxLen) {
    int err;

    // set up a read buffer and figure out how much to read (as to not read past the end)
    if(!this->file || off >= this->strtabExtents.second) {
        return std::nullopt;
    }

    char buf[maxLen];
    memset(buf, 0, maxLen);

    const auto toRead = std::min(maxLen, this->strtabExtents.second - off);

    // seek to the offset and read
    err = fseek(this->file, this->strtabExtents.first + off, SEEK_SET);
    if(err) {
        L("Failed to seek to strtab (off {}): {} {}", off, err, errno);
        return std::nullopt;
    }

    err = fread(buf, sizeof(char), toRead, this->file);
    if(err <= 0) {
        L("Failed to read strtab: {} {}", err, errno);
        return std::nullopt;
    }

    // if first byte is a null character, return empty string
    if(buf[0] == '\0') {
        return "";
    }

    // return the string
    const auto end = strnlen(buf, err);
    return std::string(buf, end);
}
