#include "InitTask.h"
#include "elf.h"
#include "physmap.h"

#include <mem/PhysicalAllocator.h>
#include <sched/Scheduler.h>
#include <sched/Task.h>
#include <sched/Thread.h>
#include <vm/Map.h>
#include <vm/MapEntry.h>

#include <arch.h>
#include <log.h>
#include <string.h>

using namespace platform;

// output logs about setting up the root server environment
#define LOG_SETUP                       0

/// VM address at which the init bundle is mapped in the task
constexpr static const uintptr_t kInitBundleVmAddr = 0x90000000;

static void RootSrvEntry(const uintptr_t);
static void MapSrvElfTemp();
static void ValidateSrvElf();
static void RemoveSrvElfTempMap();
static uintptr_t MapSrvSegments();
static void MapInitBundle();
static void AllocSrvStack(const uintptr_t, const uintptr_t);

/// describes a module loaded by the bootloader
struct ModuleInfo {
    /// size of the command line buffer
    constexpr static const uintptr_t kCmdBufLen = 256;

    // phys base and length
    uintptr_t physBase = 0;
    uintptr_t length = 0;
    // command line
    char cmdline[kCmdBufLen] = {0};
};

static ModuleInfo gRootServerModule, gInitBundleModule;

/**
 * Processes a received multiboot module.
 */
void platform::InitHandleModule(const uint32_t type, const uint32_t physBase,
        const uint32_t physEnd, const char *cmdline) {
    // handle the type
    switch(type) {
        case 'root':
            gRootServerModule.physBase = physBase;
            gRootServerModule.length = (physEnd - physBase);
            strncpy(gRootServerModule.cmdline, cmdline, ModuleInfo::kCmdBufLen);
            break;

        case 'init':
            gInitBundleModule.physBase = physBase;
            gInitBundleModule.length = (physEnd - physBase);
            strncpy(gInitBundleModule.cmdline, cmdline, ModuleInfo::kCmdBufLen);
            break;

        default:
            log("unknown multiboot module: tag '%08x' (%d bytes at %08x)", type,
                    (physEnd - physBase), physBase);
            return;
    }

    // mark its physical memory as reserved
    physmap_module_reserve(physBase, physEnd);
}



/**
 * Initializes the root server task.
 */
sched::Task *platform_init_rootsrv() {
    // create the task
    auto task = sched::Task::alloc();
    REQUIRE(task, "failed to allocate rootsrv task");

    task->setName("rootsrv");

    // create the main thread
    auto main = sched::Thread::kernelThread(task, &RootSrvEntry);
    main->kernelMode = false;

    // schedule it
    sched::Scheduler::get()->scheduleRunnable(task);

    // done
    return task;
}



/**
 * Main entry point for the root server
 * 
 * We'll start off by mapping the entire ELF file into a temporary area of memory; then read the
 * headers and map the program segments to the correct spaces in memory. We allocate the required
 * memory for the bss and an userspace stack.
 *
 * After the ELF's sections are mapped, we unmap it from the temporary area; then map the init
 * bundle.
 *
 * When complete, we'll set up for an userspace return to the entry point of the ELF.
 *
 * The temporary mappings are at fixed locations; be sure that the actual ELF load addresses do
 * not overlap with these ranges.
 */
static void RootSrvEntry(const uintptr_t) {
    // this is usally handled by the syscall
    auto thread = sched::Thread::current();
    arch::TaskWillStart(thread->task);

    // validate the loaded file
    MapSrvElfTemp();
    ValidateSrvElf();

    // prepare the fixed mappings
    const auto entry = MapSrvSegments();
    REQUIRE(entry, "failed to locate root server entry point");

    // set up a 64K stack
    constexpr static const uintptr_t kStackTop    = 0x003F0000;
    constexpr static const uintptr_t kStackBottom = 0x00400000;

    AllocSrvStack(kStackTop, (kStackBottom - kStackTop));

    // remove the temporary mapping we added for the ELF file; then map the init bundle
    RemoveSrvElfTempMap();
    MapInitBundle();

    // XXX: offset from stack is to allow us to pop off the task info ptr (which is null)
    reinterpret_cast<uintptr_t *>(kStackBottom)[-1] = 0;

    // we've finished setup; jump to the server code
    sched::Thread::current()->returnToUser(entry, kStackBottom - sizeof(uintptr_t));
}

/**
 * Maps the entire ELF file to the temporary base address.
 */
static void MapSrvElfTemp() {
    int err;

    // determine how much to map
    const uintptr_t kTempBinaryBase = 0xA0000000;

    const auto pageSz = arch_page_size();
    const size_t numPages = ((gRootServerModule.length + pageSz - 1) / pageSz);

    // map each page
    auto vm = sched::Task::current()->vm;

    for(size_t i = 0; i < numPages; i++) {
        const auto physAddr = gRootServerModule.physBase + (i * pageSz);
        const auto vmAddr = kTempBinaryBase + (i * pageSz);

        err = vm->add(physAddr, pageSz, vmAddr, vm::MapMode::kKernelRead);
        REQUIRE(!err, "failed to map page %u of root server binary: %d", i, err);
    }
}

/**
 * Validates the loaded ELF. This ensures:
 *
 * - The file is actually an ELF.
 * - It is a statically linked binary.
 */
static void ValidateSrvElf() {
    int err;
    const auto hdr = reinterpret_cast<Elf32_Ehdr *>(0xA0000000);

    // check the magic value (and also the class, data and version)
    static const uint8_t kElfIdent[7] = {
        0x7F, 0x45, 0x4C, 0x46, 0x01, 0x01, 0x01
    };
    err = memcmp(kElfIdent, hdr->ident, 7);

    REQUIRE(!err, "invalid ELF ident");

    // ensure header version and some other flags
    REQUIRE(hdr->version == 1, "invalid ELF header version %d", hdr->version);
    REQUIRE(hdr->type == 2, "rootsrv invalid binary type: %d", hdr->type);
    REQUIRE(hdr->machine == 3, "rootsrv invalid machine type: %d", hdr->type);

    // ensure the program and section headers are in bounds
    REQUIRE((hdr->secHdrOff + (hdr->numSecHdr * hdr->secHdrSize)) <= gRootServerModule.length, "%s headers extend past end of file", "section");
    REQUIRE((hdr->progHdrOff + (hdr->numProgHdr * hdr->progHdrSize)) <= gRootServerModule.length, "%s headers extend past end of file", "program");
}

/**
 * Removes the temporary mapping we made for the ELF.
 */
static void RemoveSrvElfTempMap() {
    int err;

    const uintptr_t kTempBinaryBase = 0xA0000000;
    const auto pageSz = arch_page_size();
    const size_t numPages = ((gRootServerModule.length + pageSz - 1) / pageSz);

    auto vm = sched::Task::current()->vm;

    err = vm->remove(kTempBinaryBase, numPages * pageSz);
    REQUIRE(!err, "failed to remove temporary rootsrv ELF mapping: %d", err);
}

/**
 * Reads the ELF program headers to determine which file backed sections need to be loaded.
 *
 * For this to work, all loadabale sections in the file _must_ be aligned to a page size bound; the
 * linker scripts the C library provides for static binaries should ensure this.
 *
 * This should take care of both the rwdata (.data) and zero-initialized (.bss) sections of the
 * file; they're combined into one program header entry. (These we cannot direct map; instead we
 * just copy the data from the initial mapping.)
 *
 * @return Entry point address
 */
static uintptr_t MapSrvSegments() {
    int err;

    const auto hdr = reinterpret_cast<Elf32_Ehdr *>(0xA0000000);

    const auto pageSz = arch_page_size();
    auto vm = sched::Task::current()->vm;

    // get location of program headers
    const auto pHdr = reinterpret_cast<Elf32_Phdr *>(0xA0000000 + hdr->progHdrOff);

    // parse each of the program headers
    for(size_t i = 0; i < hdr->numProgHdr; i++) {
        // ignore all non-load program headers
        auto &p = pHdr[i];
        if(p.type != PT_LOAD) continue;

        // convert the program header flags into a VM protection mode
        auto flags = vm::MapMode::ACCESS_USER;

        if(p.flags & PF_EXECUTABLE) {
            flags |= vm::MapMode::EXECUTE;
        }
        if(p.flags & PF_READ) {
            flags |= vm::MapMode::READ;
        }
        if(p.flags & PF_WRITE) {
            REQUIRE((p.flags & PF_EXECUTABLE) == 0, "cannot map page as WX");
            flags |= vm::MapMode::WRITE;
        }

        // map directly if the memory and file size are the same
        if(p.fileBytes == p.memBytes) {
            REQUIRE(p.memBytes <= (gRootServerModule.length - p.fileOff),
                    "program header %u out of bounds", i);

            const auto physBase = gRootServerModule.physBase + p.fileOff;
            const size_t numPages = ((p.memBytes + pageSz - 1) / pageSz);

            // perform the mapping
#if LOG_SETUP
            log("phdr %u: direct map from file off $%x to vm $%08x (len $%x)", i, p.fileOff,
                    p.virtAddr, (numPages * pageSz));
#endif
            err = vm->add(physBase, pageSz * numPages, p.virtAddr, flags);
            REQUIRE(!err, "failed to map root server program segment %u: %d", i, err);
        }
        // otherwise, allocate the required memory and copy
        else {
            // allocate the required pages
            const size_t numPages = ((p.memBytes + pageSz - 1) / pageSz);

            for(size_t i = 0; i < numPages; i++) {
                // allocate the physical page
                const auto page = mem::PhysicalAllocator::alloc();
                REQUIRE(page, "failed to allocate physical page");

                // insert mapping and zero it
                const auto vmAddr = p.virtAddr + (i * pageSz);
                err = vm->add(page, pageSz, vmAddr, flags);
                REQUIRE(!err, "failed to map root server program segment %u: %d", i, err);

                memset(reinterpret_cast<void *>(vmAddr), 0, pageSz);
            }

            // copy the data from the file
            void *fileOff = reinterpret_cast<void *>(0xA0000000 + p.fileOff);
            memcpy(reinterpret_cast<void *>(p.virtAddr), fileOff, p.fileBytes);

#if LOG_SETUP
            log("phdr %u: allocated %d pages, copied $%x from file off $%x (len $%x) vm %08x", i,
                    numPages, p.fileBytes, p.fileOff, p.memBytes, p.virtAddr);
#endif
        }
    }

    // fish out the entry point address
    return hdr->entryAddr;
}

/**
 * Allocates a stack for the root server.
 *
 * @note Top address must be page aligned; length must be a page multiple.
 */
static void AllocSrvStack(const uintptr_t top, const uintptr_t length) {
    int err;

    const auto pageSz = arch_page_size();
    const auto numPages = length / pageSz;

    // map each page to some anon memory
    auto vm = sched::Thread::current()->task->vm;

    for(size_t i = 0; i < numPages; i++) {
        // allocate the physical page
        const auto page = mem::PhysicalAllocator::alloc();
        REQUIRE(page, "failed to allocate physical page");

        // insert mapping and zero it
        const auto vmAddr = top + (i * pageSz);
        err = vm->add(page, pageSz, vmAddr, vm::MapMode::ACCESS_USER | vm::MapMode::kKernelRW);

        memset(reinterpret_cast<void *>(vmAddr), 0, pageSz);
    }
}

/**
 * Adds a read-only mapping of the init bundle into the address space of the init task.
 */
static void MapInitBundle() {
    int err;

    // calculate the number of pages
    const auto pageSz = arch_page_size();
    const size_t numPages = ((gInitBundleModule.length + pageSz - 1) / pageSz);

    // create an allocation
    auto task = sched::Task::current();
    auto vm = task->vm;
    auto entry = vm::MapEntry::makePhys(gInitBundleModule.physBase, kInitBundleVmAddr,
            numPages * pageSz, vm::MappingFlags::Read);

    err = vm->add(entry, task);
    REQUIRE(!err, "failed to map root server init bundle: %d", err);
}
