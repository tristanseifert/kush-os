#include "Elf32.h"
#include "../Task.h"
#include "StringHelpers.h"

#include <fmt/format.h>
#include <sys/elf.h>

#include <log.h>

#include <unistd.h>
#include <cstring>
#include <system_error>

using namespace task::loader;

/**
 * Initializes a 32-bit ELF loader.
 *
 * This performs some validation of the header of the ELF.
 *
 * @throws An exception is thrown if the ELF header is invalid.
 */
Elf32::Elf32(FILE *file) : Loader(file) {
    // get the header
    Elf32_Ehdr hdr;
    memset(&hdr, 0, sizeof(Elf32_Ehdr));

    this->read(sizeof(Elf32_Ehdr), &hdr, 0);

    // ensure the ELF is little endian, the correct version, and has an entry
    if(hdr.e_ident[EI_DATA] != ELFDATA2LSB) {
        throw LoaderError(fmt::format("Invalid ELF format: {:02x}", hdr.e_ident[EI_DATA]));
    }

    if(hdr.e_ident[EI_VERSION] != EV_CURRENT) {
        throw LoaderError(fmt::format("Invalid ELF version ({}): {:02x}", "ident", hdr.e_ident[EI_VERSION]));
    } else if(hdr.e_version != EV_CURRENT) {
        throw LoaderError(fmt::format("Invalid ELF version ({}): {:08x}", "header", hdr.e_version));
    }

    if(hdr.e_type == ET_EXEC) {
        // ensure that we've got an entry point and program headers
        if(!hdr.e_entry || !hdr.e_phoff) {
            throw LoaderError("Invalid ELF executable");
        }

        this->entryAddr = hdr.e_entry;
    } else {
        throw LoaderError(fmt::format("Invalid ELF type {:08x}", hdr.e_type));
    }

    // ensure CPU architecture
#if defined(__i386__)
    if(hdr.e_machine != EM_386) {
        throw LoaderError(fmt::format("Invalid ELF machine type {:08x}", hdr.e_type));
    }
#else
#error Update Elf32 to handle the current arch
#endif

    // ensure the program header and section header sizes make sense
    if(hdr.e_shentsize != sizeof(Elf32_Shdr)) {
        throw LoaderError(fmt::format("Invalid section header size {}", hdr.e_shentsize));
    }
    else if(hdr.e_phentsize != sizeof(Elf32_Phdr)) {
        throw LoaderError(fmt::format("Invalid program header size {}", hdr.e_phentsize));
    }

    this->phdrOff = hdr.e_phoff;
    this->numPhdr = hdr.e_phnum;
}



/**
 * Maps all sections defined by the program headers into the task.
 */
void Elf32::mapInto(Task *task) {
    // read program headers 
    std::vector<Elf32_Phdr> phdrs;
    phdrs.resize(this->numPhdr);

    this->read((this->numPhdr * sizeof(Elf32_Phdr)), phdrs.data(), this->phdrOff);

    // process each program header
    for(const auto &phdr : phdrs) {
        this->processProgHdr(task, phdr);
    }
}

/**
 * Processes a loaded program header.
 */
void Elf32::processProgHdr(Task *task, const Elf32_Phdr &phdr) {
    switch(phdr.p_type) {
        // load from file
        case PT_LOAD:
            this->phdrLoad(task, phdr);
            break;

        // define stack parameters
        case PT_GNU_STACK:
            this->phdrGnuStack(task, phdr);
            break;

        // dynamic link interpreter
        case PT_INTERP:
            this->phdrInterp(task, phdr);
            break;

        // dynamic and TLS info is handled by dynamic linker
        case PT_DYNAMIC:
        case PT_TLS:
            break;

        // unhandled program header type
        default:
            LOG("Unhandled phdr type %08x offset %08x vaddr %08x filesz %08x memsz %08x"
                " flags %08x align %08x", phdr.p_type, phdr.p_offset, phdr.p_vaddr, phdr.p_filesz,
                phdr.p_memsz, phdr.p_flags, phdr.p_align);
            break;
    }
}

/**
 * Loads a segment from the file.
 *
 * This will allocate an anonymous memory region, and copy from the file buffer. It will be mapped
 * in virtual address space at the location specified.
 */
void Elf32::phdrLoad(Task *task, const Elf32_Phdr &hdr) {
    int err;
    uintptr_t vmHandle, taskHandle, regionBase;
    void *vmBase;

    // TODO: use sysconf
    const auto pageSz = 0x1000;

    // virtual address must be page aligned
    const uintptr_t inPageOff = hdr.p_vaddr & (pageSz - 1);
    const uintptr_t virtBase = hdr.p_vaddr & ~(pageSz - 1);

    // round up to the nearest page and get the mapping flags
    size_t allocSize = hdr.p_memsz + inPageOff;
    if(allocSize & (pageSz - 1)) {
        allocSize = ((allocSize + pageSz - 1) / pageSz) * pageSz;
    }

    // LOG("Alloc size %08x (orig %08x)", allocSize, hdr.p_memsz);

    // allocate an anonymous region (RW for now) and get its base address in our vm map
    err = AllocVirtualAnonRegion(0, allocSize, VM_REGION_RW, &vmHandle);
    if(err) {
        throw std::system_error(err, std::generic_category(), "AllocVirtualAnonRegion");
    }

    err = VirtualRegionGetInfo(vmHandle, &regionBase, nullptr, nullptr);
    if(err) {
        throw std::system_error(err, std::generic_category(), "VirtualRegionGetInfo");
    }

    // set up to write to it
    vmBase = reinterpret_cast<void *>(regionBase + inPageOff);
    // LOG("Handle $%08x'h (base %p)", vmHandle, vmBase);

    // get the corresponding file region and copy it
    if(hdr.p_filesz) {
        this->read(hdr.p_filesz, vmBase, hdr.p_offset);
    }

    /*
     * Change the page's protection level.
     *
     * If the dynamic linker needs to fix up a read-only region, it will remap it as read/write
     * temporarily. This ensures static binaries will never have their .text segments left writable
     * or need to rely on a particular startup code to be secure.
     */
    uintptr_t vmFlags = 0;
    if(hdr.p_flags & PF_R) {
        vmFlags |= VM_REGION_READ;
    }
    if(hdr.p_flags & PF_W) {
        vmFlags |= VM_REGION_WRITE;
    }

    if(hdr.p_flags & PF_X) {
        if(vmFlags & VM_REGION_WRITE) {
            UnmapVirtualRegion(vmHandle);
            throw LoaderError("Refusing to add WX mapping");
        }

        vmFlags |= VM_REGION_EXEC;
    }

    err = VirtualRegionSetFlags(vmHandle, vmFlags);
    if(err) {
        throw std::system_error(err, std::generic_category(), "VirtualRegionSetFlags");
    }

    // place the mapping into the task
    taskHandle = task->getHandle();
    err = MapVirtualRegionAtTo(vmHandle, taskHandle, virtBase);
    if(err) {
        throw std::system_error(err, std::generic_category(), "MapVirtualRegionAtTo");
    }

    // then unmap it from our task
    err = UnmapVirtualRegion(vmHandle);
    if(err) {
        throw std::system_error(err, std::generic_category(), "UnmapVirtualRegion");
    }
}

/**
 * Handles stack parameters.
 *
 * This just asserts that the flag is only RW; we do not support executable stack segments.
 */
void Elf32::phdrGnuStack(Task *, const Elf32_Phdr &hdr) {
    const auto flags = hdr.p_flags & ~(PF_MASKOS | PF_MASKPROC);
    if(flags & PF_X) {
        throw LoaderError(fmt::format("Unsupported stack flags {:08x}", hdr.p_flags));
    }
}

/**
 * Reads the interpreter string from the binary. This should be the path of a statically linked
 * executable that's loaded alongside this binary (so, it must be linked in such a way as to not
 * interfere with it)
 *
 * Per the ELF specification, the string always has a NULL terminator byte. SInce we're going to
 * store this as a C++ string, we chop that off.
 */
void Elf32::phdrInterp(Task *task, const Elf32_Phdr &hdr) {
    std::string path;

    // read zero terminated string
    path.resize(hdr.p_filesz);
    this->read(hdr.p_filesz, path.data(), hdr.p_offset);

    // trim off the last byte
    this->dynLdPath = path.substr(0, path.length() - 1);
}

/**
 * Sets up the stack memory pages.
 */
void Elf32::setUpStack(Task *task, const uintptr_t infoStructAddr) {
    int err;
    uintptr_t vmHandle;
    uintptr_t base, len;

    // TODO: get from sysconf
    const auto pageSz = sysconf(_SC_PAGESIZE);
    if(pageSz <= 0) {
        throw std::system_error(errno, std::generic_category(), "Failed to determine page size");
    }

    // allocate the anonymous region
    err = AllocVirtualAnonRegion(0, kDefaultStackSz, VM_REGION_RW, &vmHandle);
    if(err) {
        throw std::system_error(err, std::generic_category(), "AllocVirtualAnonRegion");
    }

    //LOG("Stack region: $%08x'h", vmHandle);

    // fault in the last page of the region
    err = VirtualRegionGetInfo(vmHandle, &base, &len, nullptr);
    if(err) {
        throw std::system_error(err, std::generic_category(), "VirtualRegionGetInfo");
    }

    auto lastPage = reinterpret_cast<std::byte *>(base + len - pageSz);
    memset(lastPage, 0, pageSz);

    // build the stack frame
    this->stackBottom = (kDefaultStackAddr + kDefaultStackSz) - sizeof(uintptr_t);

    auto argPtr = reinterpret_cast<uintptr_t *>(base + len);
    argPtr[-1] = infoStructAddr;

    // place the mapping into the task
    err = MapVirtualRegionAtTo(vmHandle, task->getHandle(), kDefaultStackAddr);
    if(err) {
        throw std::system_error(err, std::generic_category(), "MapVirtualRegionAtTo");
    }

    // then unmap it from our task
    err = UnmapVirtualRegion(vmHandle);
    if(err) {
        throw std::system_error(err, std::generic_category(), "UnmapVirtualRegion");
    }
}
