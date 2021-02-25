#include "Library.h"

#include "log.h"

#include <algorithm>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <vector>

#include <unistd.h>
#include <sys/elf.h>
#include <sys/syscalls.h>

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
    if(!lib->readSectionHeaders()) {
        L("Failed to read section headers for '{}'", path);
        return nullptr;
    }
    if(!lib->readDynInfo()) {
        L("Invalid ELF dynamic info for library '{}'", path);
        return nullptr;
    }
    if(!lib->readDynSyms()) {
        L("Failed to load dynsyms from '{}'!", path);
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
    int err;

    // release VM regions
    for(auto &segment : this->segments) {
        if(!segment.vmRegion) continue;

        err = UnmapVirtualRegion(segment.vmRegion);
        if(err) {
            L("Failed to unmap VM region {:x}: {:d}", segment.vmRegion, err);
        }
    }

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

    this->shdrOff = hdr->e_shoff;
    this->shdrNum = hdr->e_shnum;
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

        L("Segment {:08x} - {:08x}, aligned {:08x} - {:08x} (prot {:02x} copy {:x})", segment.base,
                segment.base + segment.length, aBase, aEnd, (uint8_t) segment.protection,
                segment.fileCopyBytes);

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

    if(phdr.p_flags & PF_R) {
        info.protection |= SegmentProtection::Read;
    }
    if(phdr.p_flags & PF_W) {
        info.protection |= SegmentProtection::Write;
    }
    if(phdr.p_flags & PF_X) {
        info.protection |= SegmentProtection::Execute;
    }

    // it's a load command, ostensibly we should read from file
    info.progbits = true;

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
 * Reads a string out of the temporary string table cache; if no cache is provided, we'll use the
 * slow read method.
 */
std::optional<std::string> Library::readStrtab(const uintptr_t i) {
    if(!this->strtabTemp.empty()) {
        std::span<char> span = this->strtabTemp;
        const auto sub = span.subspan(i);
        if(sub.empty()) return std::nullopt;
        if(sub[0] == '\0') return std::nullopt;

        const auto len = strnlen(sub.data(), sub.size());
        return std::string(sub.data(), len);
    } else {
        return this->readStrtabSlow(i);
    }
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

/**
 * Reads the dynamic symbol table for the library.
 */
bool Library::readDynSyms() {
    int err;

    // read the entirety of the string table
    std::vector<char> strtab;
    strtab.resize(this->strtabExtents.second);

    err = fseek(this->file, this->strtabExtents.first, SEEK_SET);
    if(err) return false;

    err = fread(strtab.data(), sizeof(char), this->strtabExtents.second, this->file);
    if(err != this->strtabExtents.second) {
        if(err < 0) {
            L("Failed to read strtab (off {}): {} {}", this->strtabExtents.first, err, errno);
        }
        return false;
    }

    this->strtabTemp = strtab;

    // read the dynamic symbol table
    const auto numSyms = this->dynsymLen / this->symtabEntSz;
    std::vector<Elf32_Sym> syms;
    syms.resize(numSyms);

    if(this->symtabEntSz != sizeof(Elf32_Sym)) {
        // not implemented; we'd have to copy each structure individually
        return false;
    } else {
        err = fseek(this->file, this->symtabOff, SEEK_SET);
        if(err) return false;

        err = fread(syms.data(), sizeof(Elf32_Sym), numSyms, this->file);
        if(err != (sizeof(Elf32_Sym) * numSyms)) {
            if(err < 0) {
                L("Failed to read dynsyms (off {}): {} {}", this->symtabOff, err, errno);
            }
            return false;
        }
    }

    // parse each of the symbols
    L("Read {} symbols", syms.size());

    return this->parseSymtab(strtab, syms);
}

/**
 * Parses the provided symbol table. The entire string table is loaded into memory in the given
 * buffer.
 */
bool Library::parseSymtab(const std::span<char> &strtab, const std::span<Elf32_Sym> &symtab) {
    this->syms.reserve(symtab.size());

    // getter for a string from the stringtab
    auto getStr = [&](const size_t i) -> std::optional<std::string> {
        const auto sub = strtab.subspan(i);
        if(sub.empty()) return std::nullopt;
        if(sub[0] == '\0') return std::nullopt;

        const auto len = strnlen(sub.data(), sub.size());
        return std::string(sub.data(), len);
    };

    // iterate over all symbols
    for(const auto &sym : symtab) {
        // build up a symbol struct
        Symbol info;
        info.data = std::make_pair(sym.st_value, sym.st_size);

        // resolve its name
        if(sym.st_name) {
            const auto name = getStr(sym.st_name);
            if(name) {
                info.name = *name;
            }
        }

        // object type
        switch(ELF32_ST_TYPE(sym.st_info)) {
            case STT_NOTYPE:
                info.flags |= SymbolFlags::TypeUnspecified;
                break;
            case STT_OBJECT:
                info.flags |= SymbolFlags::TypeData;
                break;
            case STT_FUNC:
                info.flags |= SymbolFlags::TypeFunction;
                break;

            default:
                L("Unknown object type: {}", ELF32_ST_TYPE(sym.st_info));
                return false;
        }

        // binding type
        switch(ELF32_ST_BIND(sym.st_info)) {
            case STB_LOCAL:
                info.flags |= SymbolFlags::BindLocal;
                break;
            case STB_GLOBAL:
                info.flags |= SymbolFlags::BindGlobal;
                break;
            case STB_WEAK:
                info.flags |= SymbolFlags::BindWeakGlobal;
                break;

            default:
                L("Unknown binding attribute: {}", ELF32_ST_BIND(sym.st_info));
                return false;
        }

        // process the section index: which one is it relative to?
        if(sym.st_shndx >= SHN_LORESERVE) {
            switch(sym.st_shndx) {
                // absolute value
                case SHN_ABS:
                    info.sectionIdx = UINT16_MAX;
                    break;
                // unhandled reserved section
                default:
                    L("Unknown reserved section {:04x}", sym.st_shndx);
                    return false;
            }
        } else {
            info.sectionIdx = sym.st_shndx;
        }

        // symbol considered resolved if we have a section
        if(info.sectionIdx) {
            info.flags |= SymbolFlags::ResolvedFlag;
        }

        // insert it
        this->syms.emplace_back(std::move(info));
        //this->syms.emplace_back(name, std::move(info));
    }

    return true;
}

/**
 * Tests if our symbol table contains a global symbol with the given name.
 *
 * TODO: This could be made _way_ faster by not doing a linear scan of this table every time...
 */
bool Library::exportsSymbol(const std::string &name) const {
/*    // we have an entry for this symbol
    if(this->syms.contains(name)) {
        const auto &sym = this->syms.at(name);

        // ensure it's a global or weak global symbol
        const auto type = first.flags & SymbolFlags::BindMask;
        switch(type) {
            case SymbolFlags::BindGlobal:
            case SymbolFlags::BindWeakGlobal:
                return true;
            default:
                return false;
        }
    }
    // not defined in this library
    else {
        return false;
    }*/
    return std::any_of(this->syms.begin(), this->syms.end(),
            [&](auto &first) -> bool {
        if(!first.sectionIdx) return false;

        const auto type = first.flags & SymbolFlags::BindMask;
        switch(type) {
            case SymbolFlags::BindGlobal:
            case SymbolFlags::BindWeakGlobal:
                break;
            default:
                return false;
        }

        return (first.name == name);
    });
}

/**
 * Gets the offset/length for a symbol.
 *
 * Precondition of this function is that the symbol must exist in the library.
 */
void Library::getSymbolInfo(const std::string &name, std::pair<uintptr_t, size_t> &outInfo) {
    auto it = std::find_if(this->syms.begin(), this->syms.end(), [&](const auto &sym) -> bool {
        if(!sym.sectionIdx) return false;

        const auto type = sym.flags & SymbolFlags::BindMask;
        switch(type) {
            case SymbolFlags::BindGlobal:
            case SymbolFlags::BindWeakGlobal:
                break;
            default:
                return false;
        }

        return (sym.name == name);
    });

    outInfo = (*it).data;
}

/**
 * Resolves all imported symbols.
 */
bool Library::resolveImports(const std::vector<std::pair<uintptr_t, std::shared_ptr<Library>>> &libs) {
    // iterate over all symbols
    for(auto &sym : this->syms) {
        // skip if already resolved, or if not a global symbol
        if(TestFlags(sym.flags & SymbolFlags::ResolvedFlag)) continue;
        else if((sym.flags & SymbolFlags::BindMask) != SymbolFlags::BindGlobal) continue;

        // try to resolve it in a library
        for(auto &[base, lib] : libs) {
            if(lib->exportsSymbol(sym.name)) {
                goto resolved;
            }
        }

        // is it one of the fixed built-in symbols?
        if(sym.name == "_GLOBAL_OFFSET_TABLE_") {
            goto resolved;
        }

        // failed to resolve symbol
        L("Failed to resolve symbol '{}'", sym.name);
        goto beach;

resolved:;
        sym.flags |= SymbolFlags::ResolvedFlag;
    }

beach:;
    // return whether there's any unresolved symbols remaining
    return std::all_of(this->syms.begin(), this->syms.end(), [](auto &sym) -> bool {
        // ignore non-global symbols
        if((sym.flags & SymbolFlags::BindMask) != SymbolFlags::BindGlobal) return true;
        // return true if symbol is resolved
        return TestFlags(sym.flags & SymbolFlags::ResolvedFlag);
    });
}



/**
 * Reads the section headers of the ELF file. Currently, we're only interested in the extents of
 * the dynamic symbol table.
 */
bool Library::readSectionHeaders() {
    int err;

    // read the section headers
    std::vector<Elf32_Shdr> shdrs;
    shdrs.resize(this->shdrNum);

    err = fseek(this->file, this->shdrOff, SEEK_SET);
    if(err) return false;

    err = fread(shdrs.data(), sizeof(Elf32_Shdr), this->shdrNum, this->file);
    if(err != (sizeof(Elf32_Shdr) * this->shdrNum)) return false;

    // iterate over each header
    for(const auto &shdr : shdrs) {
        // extract fixed information
        switch(shdr.sh_type) {
            // dynamic symbol table
            case SHT_DYNSYM:
                this->dynsymLen = shdr.sh_size;
                break;

            // ignore other types of sections
            default:
                break;
        }

        // if it's loaded, construct a section info structure
        if(shdr.sh_addr) {
            this->sections.emplace_back(Section(shdr));
        }
    }

    return true;
}

/**
 * Closes the underlying file, if it's open, and deallocates file read buffers.
 *
 * This should be called once we're sure that we will need no more data out of the library's file,
 * for example, to fill in sections.
 */
void Library::closeFile() {
    if(this->file) {
        fclose(this->file);
        this->file = nullptr;
    }
}



/**
 * Allocates memory regions for all program segments backed by persistent data from the file.
 *
 * @param vmBase If specified, used as the base address of all VM region allocations.
 */
bool Library::allocateProgbitsVm(const uintptr_t vmBase) {
    int err;
    uintptr_t handle;

    const auto pageSz = sysconf(_SC_PAGESIZE);
    if(pageSz <= 0) return false;

    // iterate over each segment...
    for(auto &segment : this->segments) {
        // skip if there's no loadable part
        if(!segment.fileCopyBytes) continue;
        const auto pageOff = segment.base & (pageSz - 1);

        // round up the file load region
        size_t length = ((segment.fileCopyBytes + pageOff + pageSz - 1) / pageSz) * pageSz;
        length = std::min(length, (segment.vmEnd - segment.vmStart) + 1);

        //L("Segment {:x} to {:x} (VM {:x} to {:x}): len {:x}", segment.base,
        //        segment.base + segment.length, segment.vmStart, segment.vmEnd, length);

        // allocate vm region
        const auto base = (vmBase ? (vmBase + segment.vmStart) : 0);
        err = AllocVirtualAnonRegion(base, length, VM_REGION_RW, &handle);
        if(err) {
            L("AllocVirtualAnonRegion(base = {:x}) failed: {}", base, err);
            return false;
        }

        segment.vmRegion = handle;

        // read into it file data
        if(segment.progbits) {
            // discover where this page is mapped
            uintptr_t base;
            err = VirtualRegionGetInfo(handle, &base, nullptr, nullptr);
            if(err) {
                L("VirtualRegionGetInfo() failed: {}", err);
                return false;
            }

            void *ptr = reinterpret_cast<void *>(base + (pageOff));
            // L("Copying to {} {:x}", ptr, base);

            // perform read
            err = fseek(this->file, segment.fileOff, SEEK_SET);
            if(err) {
                L("Failed to seek to segment offset ({}): {}", segment.fileOff, err);
                return false;
            }

            err = fread(ptr, sizeof(std::byte), segment.fileCopyBytes, this->file);
            if(err != segment.fileCopyBytes) {
                L("Failed to read PROGBITS data: {} (expected {} bytes)", err, segment.fileCopyBytes);
                return false;
            }
        }
    }

    // all segments were allocated
    return true;
}



/**
 * Performs relocations on the binary.
 *
 * We'll start off by finding the .dynamic section (by parsing the section table) and reading from
 * it the regular relocations (REL/RELSZ) and the PLT relocations (JMPREL/PLTRELSZ) if required.
 */
bool Library::relocate(const std::vector<std::pair<uintptr_t, std::shared_ptr<Library>>> &libs) {
    // find out our base address (from the array above)
    uintptr_t base = 0;
    for(const auto &[loadAddr, library] : libs) {
        if(library.get() == this) {
            base = loadAddr;
        }
    }
    if(!base) return false;

    // attempt to locate the dynamic section
    Elf32_Dyn *dynamic = nullptr;
    const auto numDynEntries = (this->dynLen / sizeof(Elf32_Dyn));

    for(const auto &section : this->sections) {
        if(section.type == Section::Type::DynamicInfo) {
            // get its VM offset
            dynamic = reinterpret_cast<Elf32_Dyn *>(base + section.addr);
        }
    }
    if(!dynamic) return false;
    if(dynamic[numDynEntries-1].d_tag != DT_NULL) return false;

    // extract from the dynamic section the address of the regular and PLT relocations
    uintptr_t relAddr = 0, relEnt = 0, pltRelAddr = 0, pltRelEnt = 0, gotOff = 0;

    for(size_t i = 0; i < numDynEntries; i++) {
        const auto &ent = dynamic[i];

        switch(ent.d_tag) {
            case DT_REL:
                relAddr = ent.d_un.d_ptr;
                break;
            case DT_RELSZ:
                relEnt = ent.d_un.d_val / sizeof(Elf32_Dyn);
                break;
            case DT_JMPREL:
                pltRelAddr = ent.d_un.d_ptr;
                break;
            case DT_PLTRELSZ:
                pltRelEnt = ent.d_un.d_val / sizeof(Elf32_Dyn);
                break;
            case DT_PLTGOT:
                gotOff = ent.d_un.d_ptr;
                break;
            // ensure PLT relocation type
            case DT_PLTREL:
                if(ent.d_un.d_val != DT_REL) {
                    L("Unsupported DT_PTREL value: {}", ent.d_un.d_val);
                    return false;
                }
                break;
            default:
                continue;
        }
    }

    L("Relocations at {:x} ({}) PLT relocations at {:x} ({}) GOT @ {:x}", relAddr, relEnt,
            pltRelAddr, pltRelEnt, gotOff);

    // process the relocations
    if(relAddr) {
        auto relsPtr = reinterpret_cast<Elf32_Rel *>(base + relAddr);
        auto rels = std::span<Elf32_Rel>(relsPtr, relEnt);

        if(!this->processRelocs(rels, base, libs)) {
            return false;
        }
    }
    if(pltRelAddr) {
        auto relsPtr = reinterpret_cast<Elf32_Rel *>(base + pltRelAddr);
        auto rels = std::span<Elf32_Rel>(relsPtr, pltRelEnt);

        if(!this->processRelocs(rels, base, libs)) {
            return false;
        }
    }

    // discard caches
    this->strtabTemp.clear();
    this->strtabTemp.shrink_to_fit();

    return true;
}

/**
 * Performs the relocations specified.
 */
bool Library::processRelocs(const std::span<Elf32_Rel> &relocs, const uintptr_t base,
        const std::vector<std::pair<uintptr_t, std::shared_ptr<Library>>> &libs) {
    // process each relocation
    for(const auto &rel : relocs) {
        // get pointer to the word (XXX: validate this)
        auto fileOff = reinterpret_cast<void *>(base + rel.r_offset);

        // handle the relocation type
        switch(ELF32_R_TYPE(rel.r_info)) {
            // relative address: add to the word 
            case R_386_RELATIVE: {
                uint32_t read;
                memcpy(&read, fileOff, sizeof(read));
                read += base;
                memcpy(fileOff, &read, sizeof(read));
                break;
            }

            // set GOT entry to data symbol
            case R_386_GLOB_DAT: {
                // try to resolve symbol
                const auto sym = ELF32_R_SYM(rel.r_info);
                uintptr_t addr = 0;
                bool resolved = (sym == STN_UNDEF);

                if(!resolved) {
                    // look up the symbol name
                    const auto &symInfo = this->syms.at(sym);
                    // search all libraries for it
                    addr = resolveSymbolVmAddr(symInfo.name, libs);
                    resolved = (addr != 0);
                }

                if(!resolved) {
                    L("Failed to resolve symbol for relocation type {} (off ${:08x} info ${:08x})", 
                            ELF32_R_TYPE(rel.r_info), rel.r_offset, rel.r_info);
                    return false;
                }

                memcpy(fileOff, &addr, sizeof(uintptr_t));
                break;
            }

            // PLT jump table entry
            case R_386_JMP_SLOT: {
                // try to resolve symbol
                const auto sym = ELF32_R_SYM(rel.r_info);
                const auto &symInfo = this->syms.at(sym);
                const uintptr_t addr = resolveSymbolVmAddr(symInfo.name, libs);

                if(!addr) {
                    L("Failed to resolve symbol for relocation type {} (off ${:08x} info ${:08x})", 
                            ELF32_R_TYPE(rel.r_info), rel.r_offset, rel.r_info);
                    return false;
                }

                memcpy(fileOff, &addr, sizeof(uintptr_t));
                break;
            }

            // absolute address of a symbol
            case R_386_32: {
                // resolve symbol
                const auto sym = ELF32_R_SYM(rel.r_info);
                uintptr_t addr = 0;
                bool resolved = (sym == STN_UNDEF);

                if(!resolved) {
                    const auto &symInfo = this->syms.at(sym);
                    addr = resolveSymbolVmAddr(symInfo.name, libs);
                    resolved = (addr != 0);
                }

                if(!resolved) {
                    L("Failed to resolve symbol for relocation type {} (off ${:08x} info ${:08x})", 
                            ELF32_R_TYPE(rel.r_info), rel.r_offset, rel.r_info);
                    return false;
                }

                // fix up the symbol
                uint32_t read;
                memcpy(&read, fileOff, sizeof(read));
                read += addr;
                memcpy(fileOff, &read, sizeof(read));
                break;
            }

            default:
                L("Unknown relocation type {} (off ${:08x} info ${:08x})", ELF32_R_TYPE(rel.r_info),
                        rel.r_offset, rel.r_info);
                return false;
        }
    }

    return true;
}

/**
 * Searches the provided list of libraries for one that contains the given symbol.
 *
 * @return Symbol virtual address, or 0 if not found.
 */
uintptr_t Library::resolveSymbolVmAddr(const std::string &name,
        const std::vector<std::pair<uintptr_t, std::shared_ptr<Library>>> &libs) {
    // iterate all libraries to see if any hold the symbol
    for(const auto &[loadAddr, lib] : libs) {
        // does this one define the symbol?
        if(lib->exportsSymbol(name)) {
            // it does, so resolve it
            std::pair<uintptr_t, size_t> symInfo;
            lib->getSymbolInfo(name, symInfo);

            return symInfo.first + loadAddr;
        }
    }

    // failed to find the symbol anywhere
    return 0;
}

/**
 * Maps the shareable (e.g. those segments that are read-only) into the task.
 */
void Library::mapShareable(const uintptr_t base, const uintptr_t taskHandle) {
    int err;

    for(const auto &segment : this->segments) {
        // skip if section is writable
        if(TestFlags(segment.protection & SegmentProtection::Write)) {
            continue;
        }

        // figure out protection to use
        uintptr_t flags = VM_REGION_READ;

        if(TestFlags(segment.protection & SegmentProtection::Write)) {
            L("Shareable page: {} (in {})", segment.vmStart, this->soname);
            std::terminate();
            // flags |= VM_REGION_WRITE;
        }
        if(TestFlags(segment.protection & SegmentProtection::Execute)) {
            flags |= VM_REGION_EXEC;
        }

        // map the page
        const auto vmBase = base + segment.vmStart;

        err = MapVirtualRegionAtTo(segment.vmRegion, taskHandle, vmBase);
        if(err) {
            L("Failed to map '{}' segment (base {:x}): {:d}", this->soname, segment.base, err);
            std::terminate();
        }

        L("Mapped shareable page of {}: base {:08x} vm {:08x} - {:08x}", this->soname,
                segment.base, vmBase, vmBase + segment.length);
    }
}

/*
LIBSYSTEM_EXPORT int MapVirtualRegionToFlags(const uintptr_t regionHandle, const uintptr_t taskHandle, const uintptr_t flags);

LIBSYSTEM_EXPORT int MapVirtualRegionAtTo(const uintptr_t regionHandle, const uintptr_t taskHandle,
        const uintptr_t baseAddr);

LIBSYSTEM_EXPORT int VirtualRegionSetFlags(const uintptr_t regionHandle, const uintptr_t newFlags);

LIBSYSTEM_EXPORT int AllocVirtualAnonRegion(const uintptr_t virtualAddr, const uintptr_t size,
const uintptr_t inFlags, uintptr_t *outHandle);

LIBSYSTEM_EXPORT int VirtualRegionGetInfo(const uintptr_t regionHandle, uintptr_t *baseAddr,
uintptr_t *length, uintptr_t *flags);
*/

/**
 * Creates copies of writeable segments and yeets them into the task.
 */
void Library::mapData(const uintptr_t vmBase, const uintptr_t taskHandle) {
    int err;
    uintptr_t base, regionSz, handle;

    // get the page size
    const auto pageSz = sysconf(_SC_PAGESIZE);
    if(pageSz <= 0) {
        L("Failed to retrieve page size {}", errno);
        std::terminate();
    }

    // iterate over all segments
    for(const auto &segment : this->segments) {
        // skip if section is not writable
        if(!TestFlags(segment.protection & SegmentProtection::Write)) {
            continue;
        }

        // get the base address of the source data
        err = VirtualRegionGetInfo(segment.vmRegion, &base, &regionSz, nullptr);
        if(err) {
            L("VirtualRegionGetInfo failed: {}", err);
            std::terminate();
        }

        auto srcBase = reinterpret_cast<void *>(base + (segment.base % pageSz));

        // allocate the data page and copy into it
        regionSz = ((segment.length + pageSz + (segment.base % pageSz) - 1) / pageSz) * pageSz;

        err = AllocVirtualAnonRegion(0, regionSz, VM_REGION_RW, &handle);
        if(err) {
            L("AllocVirtualAnonRegion failed: {}", err);
            std::terminate();
        }

        err = VirtualRegionGetInfo(handle, &base, nullptr, nullptr);
        if(err) {
            L("VirtualRegionGetInfo failed: {}", err);
            std::terminate();
        }

        /*
         * XXX: hack to work around page faults not working right when writing to map entries that
         * are at a non-native base in a task. we fault in all pages now so we don't have any
         * issues.
         */
        // memset(reinterpret_cast<void *>(base), 0, regionSz-1);

        auto destBase = reinterpret_cast<void *>(base + (segment.base % pageSz));

        L("Copying {} to {} (len {} {} {})", srcBase, destBase, segment.fileCopyBytes,
                segment.length, regionSz);
        memcpy(destBase, srcBase, segment.fileCopyBytes);

        // update the protection flags
        uintptr_t flags = VM_REGION_WRITE;

        if(TestFlags(segment.protection & SegmentProtection::Read)) {
            flags |= VM_REGION_READ;
        }
        if(TestFlags(segment.protection & SegmentProtection::Execute)) {
            flags |= VM_REGION_EXEC;
        }

        err = VirtualRegionSetFlags(handle, flags);
        if(err) {
            L("VirtualRegionSetFlags failed: {}", err);
            std::terminate();
        }

        // map it into the destination task
        const auto destVmBase = vmBase + segment.vmStart;

        // L("Mapping to {:08x}", destVmBase);
        err = MapVirtualRegionAtTo(handle, taskHandle, destVmBase);
        if(err) {
            L("Failed to map '{}' segment (base {:x}): {:d}", this->soname, segment.base, err);
            std::terminate();
        }

        L("Mapped private page of {}: base {:08x} vm {:08x} - {:08x} len {:08x}", this->soname,
                segment.base, destVmBase, destVmBase + segment.length, regionSz);

        // unmap the region from our address space
        err = UnmapVirtualRegion(handle);
        if(err) {
            L("UnmapVirtualRegion failed: {}", err);
        }
    }

}
