#include "Elf32.h"
#include "../Task.h"

#include <fmt/format.h>
#include <sys/elf.h>

#include <log.h>

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
Elf32::Elf32(const std::span<std::byte> &bytes) : Loader(bytes) {
    // get the header
    auto hdrSpan = bytes.subspan(0, sizeof(Elf32_Ehdr));
    if(hdrSpan.size() < sizeof(Elf32_Ehdr)) {
        throw LoaderError("ELF header too small");
    }

    auto hdr = reinterpret_cast<const Elf32_Ehdr *>(hdrSpan.data());

    // ensure the ELF is little endian, the correct version, and has an entry
    if(hdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        throw LoaderError(fmt::format("Invalid ELF format: {:02x}", hdr->e_ident[EI_DATA]));
    }

    if(hdr->e_ident[EI_VERSION] != EV_CURRENT) {
        throw LoaderError(fmt::format("Invalid ELF version ({}): {:02x}", "ident", hdr->e_ident[EI_VERSION]));
    } else if(hdr->e_version != EV_CURRENT) {
        throw LoaderError(fmt::format("Invalid ELF version ({}): {:08x}", "header", hdr->e_version));
    }

    if(hdr->e_type == ET_EXEC) {
        // ensure that we've got an entry point and program headers
        if(!hdr->e_entry || !hdr->e_phoff) {
            throw LoaderError("Invalid ELF executable");
        }

        this->entryAddr = hdr->e_entry;
    } else {
        throw LoaderError(fmt::format("Invalid ELF type {:08x}", hdr->e_type));
    }

    // ensure CPU architecture
#if defined(__i386__)
    if(hdr->e_machine != EM_386) {
        throw LoaderError(fmt::format("Invalid ELF machine type {:08x}", hdr->e_type));
    }
#else
#error Update Elf32 to handle the current arch
#endif

    // ensure the program header and section header sizes make sense
    if(hdr->e_shentsize != sizeof(Elf32_Shdr)) {
        throw LoaderError(fmt::format("Invalid section header size {}", hdr->e_shentsize));
    }
    else if(hdr->e_phentsize != sizeof(Elf32_Phdr)) {
        throw LoaderError(fmt::format("Invalid program header size {}", hdr->e_phentsize));
    }
}



/**
 * Maps all sections defined by the program headers into the task.
 */
void Elf32::mapInto(std::shared_ptr<Task> &task) {
    // get the ELF header
    auto hdrSpan = this->file.subspan(0, sizeof(Elf32_Ehdr));
    auto hdr = reinterpret_cast<const Elf32_Ehdr *>(hdrSpan.data());

    // then, find the program headers
    const size_t phdrBytes = hdr->e_phnum * hdr->e_phentsize;
    auto phdrSpan = this->file.subspan(hdr->e_phoff, phdrBytes);

    if(phdrSpan.size() < phdrBytes) {
        throw LoaderError(fmt::format("Invalid program header offset {}", hdr->e_phoff));
    }

    auto phdrs = reinterpret_cast<const Elf32_Phdr *>(phdrSpan.data());

    // process each program header
    for(size_t i = 0; i < hdr->e_phnum; i++) {
        this->processProgHdr(task, phdrs[i]);
    }
}

/**
 * Processes a loaded program header.
 */
void Elf32::processProgHdr(std::shared_ptr<Task> &task, const Elf32_Phdr &phdr) {
    switch(phdr.p_type) {
        // load from file
        case PT_LOAD:
            this->phdrLoad(task, phdr);
            break;

        // define stack parameters
        case PT_GNU_STACK:
            this->phdrGnuStack(task, phdr);
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
void Elf32::phdrLoad(std::shared_ptr<Task> &task, const Elf32_Phdr &hdr) {
    int err;
    uintptr_t vmHandle, taskHandle, regionBase;
    void *vmBase;

    // TODO: use sysconf
    const auto pageSz = 0x1000;

    // virtual address must be page aligned
    if(hdr.p_vaddr & (pageSz - 1)) {
        throw LoaderError(fmt::format("Unaligned load address {:08x}", hdr.p_vaddr));
    }

    // round up to the nearest page and get the mapping flags
    size_t allocSize = hdr.p_memsz;
    if(allocSize & (pageSz - 1)) {
        allocSize = ((allocSize + pageSz - 1) / pageSz) * pageSz;
    }

    LOG("Alloc size %08x (orig %08x)", allocSize, hdr.p_memsz);

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
    vmBase = reinterpret_cast<void *>(regionBase);
    LOG("Handle $%08x'h (base %p)", vmHandle, vmBase);

    // get the corresponding file region and copy it
    if(hdr.p_filesz) {
        auto toCopy = this->file.subspan(hdr.p_offset, hdr.p_filesz);
        if(toCopy.size() != hdr.p_filesz) {
            throw LoaderError(fmt::format("Failed to get section data; requested {} got {}",
                        hdr.p_filesz, toCopy.size()));
        }

        memcpy(vmBase, toCopy.data(), toCopy.size());
    }

    // change the protection level to what the program header desires
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
    err = MapVirtualRegionAtTo(vmHandle, taskHandle, hdr.p_vaddr);
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
void Elf32::phdrGnuStack(std::shared_ptr<Task> &, const Elf32_Phdr &hdr) {
    const auto flags = hdr.p_flags & ~(PF_MASKOS | PF_MASKPROC);
    if(flags & PF_X) {
        throw LoaderError(fmt::format("Unsupported stack flags {:08x}", hdr.p_flags));
    }
}



/**
 * Sets up the stack memory pages.
 */
void Elf32::setUpStack(std::shared_ptr<Task> &task) {
    int err;
    uintptr_t vmHandle;
    uintptr_t base, len;

    // TODO: get from sysconf
    const size_t pageSz = 0x1000;

    // allocate the anonymous region
    err = AllocVirtualAnonRegion(0, kDefaultStackSz, VM_REGION_RW, &vmHandle);
    if(err) {
        throw std::system_error(err, std::generic_category(), "AllocVirtualAnonRegion");
    }

    LOG("Stack region: $%08x'h", vmHandle);

    // fault in the last page of the region
    err = VirtualRegionGetInfo(vmHandle, &base, &len, nullptr);
    if(err) {
        throw std::system_error(err, std::generic_category(), "VirtualRegionGetInfo");
    }

    auto lastPage = reinterpret_cast<std::byte *>(base + len - pageSz);
    memset(lastPage, 0, pageSz);

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
