#include "Elf64.h"
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
 * Initializes a 64-bit ELF loader.
 *
 * This performs some validation of the header of the ELF.
 *
 * @throws An exception is thrown if the ELF header is invalid.
 */
Elf64::Elf64(FILE *file) : ElfCommon(file) {
    // get the header
    Elf64_Ehdr hdr;
    memset(&hdr, 0, sizeof hdr);

    this->read(sizeof hdr, &hdr, 0);

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
    throw LoaderError("64 bit ELF not supported on 32 bit x86");
#elif defined(__amd64__)
    if(hdr.e_machine != EM_X86_64) {
        throw LoaderError(fmt::format("Invalid ELF machine type {:08x}", hdr.e_type));
    }
#else
#error Update Elf64 to handle the current arch
#endif

    // ensure the program header and section header sizes make sense
    if(hdr.e_shentsize != sizeof(Elf64_Shdr)) {
        throw LoaderError(fmt::format("Invalid section header size {}", hdr.e_shentsize));
    }
    else if(hdr.e_phentsize != sizeof(Elf64_Phdr)) {
        throw LoaderError(fmt::format("Invalid program header size {}", hdr.e_phentsize));
    }

    this->phdrOff = hdr.e_phoff;
    this->phdrSize = hdr.e_phentsize;
    this->numPhdr = hdr.e_phnum;
}



/**
 * Maps all sections defined by the program headers into the task.
 */
void Elf64::mapInto(const std::shared_ptr<Task> &task) {
    // read program headers 
    std::vector<Elf64_Phdr> phdrs;
    phdrs.resize(this->numPhdr);

    this->read((this->numPhdr * sizeof(Elf64_Phdr)), phdrs.data(), this->phdrOff);

    // process each program header
    for(const auto &phdr : phdrs) {
        this->processProgHdr(task, phdr);
    }
}

/**
 * Processes a loaded program header.
 */
void Elf64::processProgHdr(const std::shared_ptr<Task> &task, const Elf64_Phdr &phdr) {
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

        // points back to location of program headers in executable image
        case PT_PHDR:
            break;

        // unhandled program header type
        default:
            LOG("Unhandled phdr type %lu offset %p vaddr $%p filesz %lu memsz %lu"
                " flags $%08x align %lu", phdr.p_type, phdr.p_offset, phdr.p_vaddr, phdr.p_filesz,
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
void Elf64::phdrLoad(const std::shared_ptr<Task> &task, const Elf64_Phdr &hdr) {
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

    // allocate an anonymous region (RW for now) and get its base address in our vm map
    err = AllocVirtualAnonRegion(allocSize, VM_REGION_RW, &vmHandle);
    if(err) {
        throw std::system_error(err, std::generic_category(), "AllocVirtualAnonRegion");
    }

    err = MapVirtualRegionRange(vmHandle, ElfCommon::kTempMappingRange, allocSize, 0, &regionBase);

    if(err) {
        throw std::system_error(err, std::generic_category(), "MapVirtualRegionRange");
    }

    // set up to write to it
    vmBase = reinterpret_cast<void *>(regionBase + inPageOff);

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
    err = MapVirtualRegionRemote(taskHandle, vmHandle, virtBase, allocSize, 0);
    if(err) {
        throw std::system_error(err, std::generic_category(), "MapVirtualRegionRemote");
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
void Elf64::phdrGnuStack(const std::shared_ptr<Task> &, const Elf64_Phdr &hdr) {
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
void Elf64::phdrInterp(const std::shared_ptr<Task> &task, const Elf64_Phdr &hdr) {
    std::string path;

    // read zero terminated string
    path.resize(hdr.p_filesz);
    this->read(hdr.p_filesz, path.data(), hdr.p_offset);

    // trim off the last byte
    this->dynLdPath = path.substr(0, path.length() - 1);
}

