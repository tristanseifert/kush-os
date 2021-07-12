#include "ElfReader.h"
#include "Library.h"
#include "Linker.h"
#include "runtime/ThreadLocal.h"

#include <PacketTypes.h>

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include <errno.h>
#include <unistd.h>
#include <rpc/dispensary.h>
#include <rpc/RpcPacket.hpp>
#include <sys/elf.h>

using namespace dyldo;

/// Port used to receive replies from the RPC server
uintptr_t ElfReader::gRpcReplyPort{0};
/// Port of the dynamic link server's RPC endpoint
uintptr_t ElfReader::gRpcServerPort{0};
/// Dynamically allocated buffer for RPC responses
void *ElfReader::gRpcReceiveBuf;

/// whether we output logging information about loaded segments
bool ElfReader::gLogSegments = false;

/**
 * Creates an ELF reader for an already opened file.
 */
ElfReader::ElfReader(FILE * _Nonnull fp, const char *_path) : file(fp),
    path(strdup(_path)) {
    this->ownsFile = false;

    this->getFilesize();
    this->validateHeader();
}


/**
 * Opens the file at the provided path. If unable, the program is terminated.
 */
ElfReader::ElfReader(const char *_path) : path(strdup(_path)) {
    // open the file
    FILE *fp = fopen(_path, "rb");
    if(!fp) {
        fprintf(stderr, "Failed to open executable '%s': %d", _path, errno);
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

    free(this->path);
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
        Linker::Abort("(%s) Invalid ELF magic: %02x%02x%02x%02x", this->path ? this->path : "",
                hdr.e_ident[0], hdr.e_ident[1], hdr.e_ident[2], hdr.e_ident[3]);
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
        Linker::Abort("%s: missing strtab (addr $%x len %lu, %lu dynents at $%p)",
                this->path ? this->path : "?", strtabAddr, strtabLen, this->dynInfo.size(),
                this->dynInfo.data());
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
 * If the segment is marked as R^W (that is, read-only or execute-only) then we fire off a request
 * to the dynamic link server (if available) and request it provides the shared segment handle or
 * performs the initial load for us.
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

    // check if segment is shareable
    if(this->hasDyldosrv() && !(phdr.p_flags & PF_W)) {
        this->loadSegmentShared(phdr, base, seg);
    }
    // otherwise, manually map it
    else {
        // allocate the region
        const size_t regionLen = (seg.vmEnd - seg.vmStart) + 1;

        err = AllocVirtualAnonRegion(regionLen, VM_REGION_RW, &vmRegion);
        if(err) {
            Linker::Abort("failed to %s anon region (for %s): %d", "allocate", this->path, err);
        }
        seg.vmRegion = vmRegion;

        // map region
        err = MapVirtualRegion(vmRegion, seg.vmStart, regionLen, 0);
        if(err) {
            Linker::Abort("failed to %s anon region (for %s) at $%p ($%x bytes): %d", "map", this->path, seg.vmStart, regionLen, err);
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
    }

    // store info
    this->segments.push_back(std::move(seg));
}

/**
 * Establish the connection to the dynamic link server.
 */
bool ElfReader::hasDyldosrv() {
    int err;

    // if we've already allocated a reply port assume success
    if(gRpcReplyPort && gRpcServerPort) {
        return true;
    }

    // allocate the receive buffer
    if(!gRpcReceiveBuf) {
        err = posix_memalign(&gRpcReceiveBuf, 16, kMaxMsgLen);
        if(err) {
            Linker::Abort("%s failed: %d", "posix_memalign", err);
        }

        memset(gRpcReceiveBuf, 0, kMaxMsgLen);
    }

    // resolve the remote port
    err = LookupService(kDyldosrvPortName.data(), &gRpcServerPort);
    if(!err) {
        // server not available
        return false;
    } else if(err < 0) {
        Linker::Abort("%s failed: %d", "LookupService", err);
    }

    // allocate the reply port
    err = PortCreate(&gRpcReplyPort);
    if(err) {
        Linker::Abort("%s failed: %d", "PortCreate", err);
    }

    return true;
}

/**
 * Sends an RPC request to the dynamic link server, if possible, to map the segment.
 */
void ElfReader::loadSegmentShared(const Elf_Phdr &phdr, const uintptr_t base, Segment &seg) {
    int err;

    // allocate the buffer for the request
    void *msgBuf;
    const auto pathBytes = strlen(this->path) + 2;
    const auto msgBytes = sizeof(rpc::RpcPacket) + sizeof(DyldosrvMapSegmentRequest) + pathBytes;

    err = posix_memalign(&msgBuf, 16, msgBytes);
    if(err) {
        Linker::Abort("%s failed: %d", "posix_memalign", err);
    }

    memset(msgBuf, 0, msgBytes);

    auto outPacket = reinterpret_cast<rpc::RpcPacket *>(msgBuf);
    outPacket->replyPort = gRpcReplyPort;
    outPacket->type = static_cast<uint32_t>(DyldosrvMessageType::MapSegment);

    // build the message
    auto msg = reinterpret_cast<DyldosrvMapSegmentRequest *>(outPacket->payload);

    msg->objectVmBase = base;
    memcpy(&msg->phdr, &phdr, sizeof(phdr));
    strncpy(msg->path, this->path, pathBytes);

    // send it :)
    err = PortSend(gRpcServerPort, msgBuf, msgBytes);
    free(msgBuf);

    if(err) {
        Linker::Abort("%s failed: %d", "PortSend", err);
    }

    // wait to receive response
    auto replyMsg = reinterpret_cast<struct MessageHeader *>(gRpcReceiveBuf);
    err = PortReceive(gRpcReplyPort, replyMsg, kMaxMsgLen, UINTPTR_MAX);

    if(err < 0) {
        Linker::Abort("%s failed: %d", "PortReceive", err);
    }

    // validate it
    if(replyMsg->receivedBytes < sizeof(rpc::RpcPacket) + sizeof(DyldosrvMapSegmentReply)) {
        Linker::Abort("RPC reply too small (%lu bytes)", replyMsg->receivedBytes);
    }

    auto packet = reinterpret_cast<const rpc::RpcPacket *>(replyMsg->data);
    if(packet->type != static_cast<uint32_t>(DyldosrvMessageType::MapSegmentReply)) {
        Linker::Abort("Invalid RPC reply type %08x", packet->type);
    }

    // handle failures
    auto reply = reinterpret_cast<const DyldosrvMapSegmentReply *>(packet->payload);
    if(reply->status) {
        Linker::Abort("Failed to map shared region (off $%x len $%x) in %s: %d", phdr.p_offset,
                phdr.p_memsz, this->path, reply->status);
    }

    // and successes
    seg.vmRegion = reply->vmRegion;
    seg.shared = true;
}

/**
 * Apply the correct protection flags for all mapped segments.
 */
void ElfReader::applyProtection() {
    int err;

    for(const auto &seg : this->segments) {
        // shared mappings are properly protected already
        if(seg.shared) continue;

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
