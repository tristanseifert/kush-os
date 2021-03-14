#include "tar.h"
#include "elf.h"

#include <platform.h>
#include <log.h>

#include <mem/PhysicalAllocator.h>
#include <sched/Scheduler.h>
#include <sched/Task.h>
#include <sched/Thread.h>
#include <vm/Map.h>
#include <vm/MapEntry.h>

#include <bootboot.h>

extern "C" BOOTBOOT bootboot;

using namespace platform;

// output logs about setting up the root server environment
#define LOG_SETUP                       0

/// VM address at which the init bundle is mapped in the task
constexpr static const uintptr_t kInitBundleVmAddr = 0x690000000;
/// Name of root server binary in initrd
static const char *kRootSrvName = "rootsrv\0";
constexpr static const size_t kRootSrvNameLen = 8;

static void RootSrvEntry(const uintptr_t);
static void MapInitBundle();
static bool FindRootsrvFile(void* &outPtr, size_t &outLength);
static uintptr_t ValidateSrvElf(void *elfBase, const size_t elfSize);
static uintptr_t MapSrvSegments(const uintptr_t, void *elfBase, const size_t elfSize);
static void AllocSrvStack(const uintptr_t, const uintptr_t);

/**
 * Loads the root server binary from the ramdisk.
 */
sched::Task *platform_init_rootsrv() {
    // create the task
    auto task = sched::Task::alloc();
    REQUIRE(task, "failed to allocate rootsrv task");

    task->setName("rootsrv");

#if LOG_SETUP
    log("created rootsrv task: %p $%016llx'h", task, task->handle);
#endif

    // create the main thread
    auto main = sched::Thread::kernelThread(task, &RootSrvEntry);
    main->kernelMode = false;
    main->setName("Main");

#if LOG_SETUP
    log("rootsrv thread: $%p $%p'h", main, main->handle);
#endif

    // schedule it
    sched::Scheduler::get()->scheduleRunnable(task);

    // done
    return task;
}

/**
 * Main entry point for the root server
 *
 * Map the init bundle -- an USTAR file -- into the task's address space, and attempt to find in
 * it the ELF for the root server. Once we've located it, create mappings that contain the ELF's
 * .text and .data segments, and allocate a .bss, and stack.
 *
 * When complete, we'll set up for an userspace return to the entry point of the ELF.
 *
 * The temporary mappings are at fixed locations; be sure that the actual ELF load addresses do
 * not overlap with these ranges.
 */
static void RootSrvEntry(const uintptr_t) {
    void *elfBase = nullptr;
    size_t elfLength = 0;

    // this is usally handled by the syscall
    auto thread = sched::Thread::current();
    arch::TaskWillStart(thread->task);

    // map the init bundle; find the root server file
    MapInitBundle();

    if(!FindRootsrvFile(elfBase, elfLength)) {
        panic("failed to find rootsrv");
    }

    const auto diff = reinterpret_cast<uintptr_t>(elfBase) - kInitBundleVmAddr;
    const auto elfPhys = bootboot.initrd_ptr + diff;

#if LOG_SETUP
    log("rootsrv ELF at %p (phys %p off %p %p) len %u", elfBase, elfPhys, diff, bootboot.initrd_ptr, elfLength);
#endif

    // create segments for ELF
    const auto entry = ValidateSrvElf(elfBase, elfLength);
    MapSrvSegments(elfPhys, elfBase, elfLength);

#if LOG_SETUP
    log("rootsrv entry: %p (file at %p len %u)", entry, elfBase, elfLength);
#endif

    // set up a 128K stack
    constexpr static const uintptr_t kStackTop    = 0x7fff80000000;
    constexpr static const uintptr_t kStackBottom = 0x7fff80008000;

    AllocSrvStack(kStackTop, (kStackBottom - kStackTop));

    // XXX: offset from stack is to allow us to pop off the task info ptr (which is null)
    reinterpret_cast<uintptr_t *>(kStackBottom)[-1] = 0;
    // we've finished setup; jump to the server code
#if LOG_SETUP
    log("going to: %p (stack %p)", entry, kStackBottom);
#endif
    sched::Thread::current()->returnToUser(entry, kStackBottom - sizeof(uintptr_t));
}

/**
 * Validates the loaded ELF. This ensures:
 *
 * - The file is actually an ELF.
 * - It is a statically linked binary.
 */
static uintptr_t ValidateSrvElf(void *elfBase, const size_t elfSize) {
    REQUIRE(elfSize > sizeof(Elf64_Ehdr), "ELF too small: %u", elfSize);

    int err;
    const auto hdr = reinterpret_cast<Elf64_Ehdr *>(elfBase);

    // check the magic value (and also the class, data and version)
    static const uint8_t kElfIdent[7] = {
        // ELF magic
        0x7F, 0x45, 0x4C, 0x46,
        // 64-bit class
        0x02,
        // data encoding: little endian
        0x01,
        // SysV ABI
        0x01
    };
    err = memcmp(kElfIdent, hdr->ident, 7);

    REQUIRE(!err, "invalid ELF ident");

    // ensure header version and some other flags
    REQUIRE(hdr->version == 1, "invalid ELF header version %d", hdr->version);
    REQUIRE(hdr->type == 2, "rootsrv invalid binary type: %d", hdr->type);
    REQUIRE(hdr->machine == 62, "rootsrv invalid machine type: %d", hdr->machine); // EM_X86_64

    // ensure the program and section headers are in bounds
    REQUIRE((hdr->secHdrOff + (hdr->numSecHdr * hdr->secHdrSize)) <= elfSize, "%s headers extend past end of file", "section");
    REQUIRE((hdr->progHdrOff + (hdr->numProgHdr * hdr->progHdrSize)) <= elfSize, "%s headers extend past end of file", "program");

    REQUIRE(hdr->progHdrSize >= sizeof(Elf64_Phdr), "invalid phdr size: %u", hdr->progHdrSize);

    // return entry point address
    return hdr->entryAddr;
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
static uintptr_t MapSrvSegments(const uintptr_t elfPhys, void *elfBase, const size_t elfSize) {
    int err;

    const auto hdr = reinterpret_cast<Elf64_Ehdr *>(elfBase);

    const auto pageSz = arch_page_size();
    auto vm = sched::Task::current()->vm;

    // get location of program headers
    const auto pHdr = reinterpret_cast<Elf64_Phdr *>(reinterpret_cast<uintptr_t>(elfBase) 
            + hdr->progHdrOff);

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
        void *fileOff = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(elfBase) 
                + p.fileOff);
        memcpy(reinterpret_cast<void *>(p.virtAddr), fileOff, p.fileBytes);

#if LOG_SETUP
        log("phdr %u: allocated %d pages, copied $%x from file off $%x (len $%x) vm %08x", i,
                numPages, p.fileBytes, p.fileOff, p.memBytes, p.virtAddr);
#endif
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
    auto vm = sched::Task::current()->vm;

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
    const size_t numPages = ((bootboot.initrd_size + pageSz - 1) / pageSz);

    // create an allocation
    auto task = sched::Task::current();
    auto vm = task->vm;
    auto entry = vm::MapEntry::makePhys(bootboot.initrd_ptr, kInitBundleVmAddr,
            numPages * pageSz, vm::MappingFlags::Read);

#if LOG_SETUP
    log("Mapped init bundle: phys %p len %u bytes to %p", bootboot.initrd_ptr,
            bootboot.initrd_size, kInitBundleVmAddr);
#endif

    err = vm->add(entry, task);
    REQUIRE(!err, "failed to map root server init bundle: %d", err);
}

/**
 * Helper method to convert an octal string to a binary number.
 */
static size_t Oct2Bin(const char *str, size_t size) {
    size_t n = 0;
    auto c = str;
    while (size-- > 0) {
        n *= 8;
        n += *c - '0';
        c++;
    }
    return n;
}

/**
 * Searches the init bundle (which is assumed to be a tar file) for the root server binary.
 *
 * @param outPtr Virtual memory address of root server binary, if found
 * @param outLength Length of the root server binary, if found
 * @return Whether the binary was located
 */
static bool FindRootsrvFile(void* &outPtr, size_t &outLength) {
    auto read = reinterpret_cast<uint8_t *>(kInitBundleVmAddr);
    const auto bundleEnd = kInitBundleVmAddr + bootboot.initrd_size;

    // iterate until we find the right header
    while((reinterpret_cast<uintptr_t>(read) + 512) < bundleEnd &&
            !memcmp(read + 257, TMAGIC, 5)) {
        // get the size of this header and entry
        auto hdr = reinterpret_cast<struct posix_header *>(read);
        const auto size = Oct2Bin(hdr->size, 11);

        // compare filename
        if(!memcmp(hdr->name, kRootSrvName, kRootSrvNameLen)) {
            outLength = size;
            outPtr = read + 512; // XXX: is this block size _always_ 512?
            return true;
        }

        // advance to next entry
        read += (((size + 511) / 512) + 1) * 512;
    }

    return false;
}
