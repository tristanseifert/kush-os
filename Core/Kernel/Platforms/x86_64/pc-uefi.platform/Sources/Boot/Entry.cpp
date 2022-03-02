/**
 * Entry point, called from the bootloader.
 *
 * At this point, we have the following guarantees about the environment:
 *
 * - Stack is properly configured
 * - Virtual address mapped as requested in ELF program headers
 * - All segments are 64-bit disabled.
 * - GDT is loaded with bootloader-provided GDT.
 * - No IDT is specified.
 * - NX bit enabled, paging enabled, A20 gate opened
 * - All PIC and IOAPIC IRQs masked
 * - UEFI boot services exited
 */
#include <stdint.h>
#include <stddef.h>
#include <stivale2.h>

#include <Init.h>
#include <Memory/PhysicalAllocator.h>
#include <Logging/Console.h>
#include <Vm/Manager.h>
#include <Vm/Map.h>
#include <Vm/ContiguousPhysRegion.h>

#include <new>

#include "Helpers.h"
#include "Arch/Gdt.h"
#include "Arch/Idt.h"
#include "Arch/Processor.h"
#include "Memory/PhysicalMap.h"
#include "Io/Console.h"
#include "Util/Backtrace.h"
#include "Vm/KernelMemoryMap.h"

using namespace Platform::Amd64Uefi;

static void InitPhysAllocator(struct stivale2_struct *info);
static Kernel::Vm::Map *InitKernelVm();
static void PopulateKernelVm(struct stivale2_struct *info, Kernel::Vm::Map *map);
static void MapKernelSections(struct stivale2_struct *info, Kernel::Vm::Map *map);

/**
 * @brief Base address for framebuffer
 *
 * Virtual memory base address (in platform-specific space) for the framebuffer.
 */
static constexpr uintptr_t kFramebufferBase{0xffff'e800'0000'0000};

/**
 * Minimum size of physical memory regions to consider for allocation
 *
 * In some cases, the bootloader may provide a very fragmented memory map to the kernel, in which
 * many small chunks are carved out. Since each physical region comes with some fixed overhead, it
 * does not make sense to add these to the allocator and we just ignore that memory.
 *
 * That is to say, all usable memory regions smaller than this constant are wasted.
 */
constexpr static const size_t kMinPhysicalRegionSize{0x10000};

/**
 * First address for general purpose physical allocation
 *
 * Reserve all memory below this boundary, and do not add it to the general purpose allocator pool;
 * this is used so we can set aside the low 16M of system memory for legacy ISA DMA.
 */
constexpr static const uintptr_t kPhysAllocationBound{0x1000000};

/**
 * VM object corresponding to the kernel image.
 *
 * This is set when the kernel image is mapped into virtual address space, and can be used later to
 * map it into other address spaces or access it.
 */
static Kernel::Vm::MapEntry *gKernelImageVm{nullptr};

/**
 * Entry point from the bootloader.
 */
extern "C" void _osentry(struct stivale2_struct *loaderInfo) {
    // set up the console (bootloader terminal, serial, etc.) and kernel console
    Console::Init(loaderInfo);
    Kernel::Console::Init();

    Backtrace::Init(loaderInfo);

    // initialize processor data structures
    Processor::VerifyFeatures();
    Processor::InitFeatures();

    Gdt::Init();
    Idt::InitBsp();

    // initialize the physical allocator, then the initial kernel VM map
    InitPhysAllocator(loaderInfo);

    auto map = InitKernelVm();
    PopulateKernelVm(loaderInfo, map);

    // prepare a few internal components
    Console::PrepareForVm(loaderInfo, map);

    // then activate the map
    map->activate();
    Memory::PhysicalMap::FinishedEarlyBoot();
    Console::VmEnabled();

    if(gKernelImageVm) {
        auto ptr = reinterpret_cast<const void *>(KernelAddressLayout::KernelImageStart);
        Backtrace::ParseKernelElf(ptr, gKernelImageVm->getLength());
    }

    // jump to the kernel's entry point now
    Kernel::Start();
    // we should never get here…
    PANIC("Kernel entry point returned!");
}

/**
 * @brief Initialize the physical memory allocator.
 *
 * This initializes the kernel's physical allocator, with our base and extended page sizes. For
 * amd64, we only support 4K and 2M pages, so those are the two page sizes.
 *
 * Once the allocator is initialized, go through each of the memory regions provided by the
 * bootloader that are marked as usable. These are guaranteed to at least be 4K aligned which is
 * required by the physical allocator.
 *
 * @param info Information structure provided by the bootloader
 */
static void InitPhysAllocator(struct stivale2_struct *info) {
    // initialize kernel physical allocator
    static const size_t kExtraPageSizes[]{
        0x200000,
    };
    Kernel::PhysicalAllocator::Init(0x1000, kExtraPageSizes, 1);

    // locate physical memory map and validate it
    auto map = reinterpret_cast<const stivale2_struct_tag_memmap *>
        (Stivale2::GetTag(info, STIVALE2_STRUCT_TAG_MEMMAP_ID));
    REQUIRE(map, "Missing loader info struct %s (%016zx)", "phys mem map",
            STIVALE2_STRUCT_TAG_MEMMAP_ID);
    REQUIRE(map->entries, "Invalid loader info struct %s", "phys mem map");

    // add each usable region to the physical allocator
    for(size_t i = 0; i < map->entries; i++) {
        const auto &entry = map->memmap[i];

        if(entry.type != STIVALE2_MMAP_USABLE) continue;
        // ignore if it's too small
        else if(entry.length < kMinPhysicalRegionSize) continue;
        // if this entire region is below the cutoff, ignore it
        else if((entry.base + entry.length) < kPhysAllocationBound) continue;

        // adjust the base/length (if needed)
        uintptr_t base = entry.base;
        size_t length = entry.length;

        if(base < kPhysAllocationBound) {
            const auto diff = (kPhysAllocationBound - base);
            base += diff;
            length -= diff;
            // TODO: mark the set aside region for legacy use
        }

        // add it to the physical allocator
        Kernel::PhysicalAllocator::AddRegion(base, length);
    }

    Kernel::Logging::Console::Notice("Available memory: %zu K",
            Kernel::PhysicalAllocator::GetTotalPages() * 4);
}

/**
 * @brief Set up kernel VMM and allocate the kernel's virtual memory map.
 *
 * First, this initializes the kernel virtual memory manager.
 *
 * Then, it creates the first virtual memory map, in reserved storage space in the .data segment of
 * the kernel. It is then registered with the kernel VMM for later use.
 */
static Kernel::Vm::Map *InitKernelVm() {
    // set up VMM
    Kernel::Vm::Manager::Init();

    // create the kernel map
    static KUSH_ALIGNED(64) uint8_t gKernelMapBuf[sizeof(Kernel::Vm::Map)];
    auto ptr = reinterpret_cast<Kernel::Vm::Map *>(&gKernelMapBuf);

    new(ptr) Kernel::Vm::Map();

    return ptr;
}

/**
 * @brief Populate the kernel virtual memory map
 *
 * Fill in the kernel's virtual memory map with the sections for the kernel executable, as well as
 * the physical map aperture which is used to access physical pages when building page tables.
 *
 * @param info Information structure provided by the bootloader
 */
static void PopulateKernelVm(struct stivale2_struct *info, Kernel::Vm::Map *map) {
    int err;

    // map the kernel executable sections (.text, .rodata, .data/.bss) and then the full image
    MapKernelSections(info, map);

    auto file2 = reinterpret_cast<const struct stivale2_struct_tag_kernel_file_v2 *>(
            Stivale2::GetTag(info, STIVALE2_STRUCT_TAG_KERNEL_FILE_V2_ID));
    if(file2) {
        // get phys size and round up size
        const auto phys = file2->kernel_file & 0xffffffff;
        const auto pages = (file2->kernel_size + (PageTable::PageSize() - 1)) / PageTable::PageSize();
        const auto bytes = pages * PageTable::PageSize();

        REQUIRE(bytes <
                (KernelAddressLayout::KernelImageEnd - KernelAddressLayout::KernelImageStart),
                "Kernel image too large for reserved address region");

        // create and map the VM object
        static KUSH_ALIGNED(64) uint8_t gVmObjectBuf[sizeof(Kernel::Vm::ContiguousPhysRegion)];
        auto vm = reinterpret_cast<Kernel::Vm::ContiguousPhysRegion *>(gVmObjectBuf);
        new (vm) Kernel::Vm::ContiguousPhysRegion(phys, bytes, Kernel::Vm::Mode::KernelRead);

        err = map->add(KernelAddressLayout::KernelImageStart, vm);
        REQUIRE(!err, "failed to map %s: %d", "kernel image", err);

        gKernelImageVm = vm;
    } else {
        Backtrace::ParseKernelElf(nullptr, 0);
    }

    // map framebuffer (if specified by loader)
    static KUSH_ALIGNED(64) uint8_t gFbVmBuf[sizeof(Kernel::Vm::ContiguousPhysRegion)];
    Kernel::Vm::ContiguousPhysRegion *framebuffer{nullptr};

    auto mmap = reinterpret_cast<const stivale2_struct_tag_memmap *>
        (Stivale2::GetTag(info, STIVALE2_STRUCT_TAG_MEMMAP_ID));

    for(size_t i = 0; i < mmap->entries; i++) {
        const auto &entry = mmap->memmap[i];
        if(entry.type != STIVALE2_MMAP_FRAMEBUFFER) continue;

        // create the VM object
        framebuffer = reinterpret_cast<Kernel::Vm::ContiguousPhysRegion *>(gFbVmBuf);
        new(framebuffer) Kernel::Vm::ContiguousPhysRegion(entry.base, entry.length,
                Kernel::Vm::Mode::KernelRW);

        err = map->add(kFramebufferBase, framebuffer);
        REQUIRE(!err, "failed to map %s: %d", "framebuffer", err);

        Kernel::Console::Notice("Framebuffer: %016llx %zu bytes", entry.base, entry.length);
        break;
    }

    if(framebuffer) {
        Console::SetFramebuffer(info, framebuffer, reinterpret_cast<void *>(kFramebufferBase));
    }

    // last, remap the physical allocator structures
    Kernel::PhysicalAllocator::RemapTo(map);
}

/**
 * @brief Create VM objects for all of the kernel's segments.
 *
 * This will create VM objects for the virtual memory segments (based off the program headers, as
 * loaded by the bootloader) for the kernel. This roughly corresponds to the RX/R/RW regions that
 * hold .text, .rodata, and .data/.bss respectively.
 */
static void MapKernelSections(struct stivale2_struct *info, Kernel::Vm::Map *map) {
    int err;

    // get the physical and virtual base of the kernel image
    uint64_t kernelPhysBase{0}, kernelVirtBase{0xffffffff80000000};

    auto base = reinterpret_cast<const stivale2_struct_tag_kernel_base_address *>
        (Stivale2::GetTag(info, STIVALE2_STRUCT_TAG_KERNEL_BASE_ADDRESS_ID));
    if(base) {
        kernelPhysBase = base->physical_base_address;
        kernelVirtBase = base->virtual_base_address;
    }

    /*
     * Allocate a VM object for each of the PMRs set up by the bootloader. Each PMR corresponds to
     * a section of contiguous protection modes. Each of the three PHDRs specified in the linker
     * script will create its own section, with the .text section split into an executable and a
     * non-executable part.
     */
    auto pmrs = reinterpret_cast<const stivale2_struct_tag_pmrs *>
        (Stivale2::GetTag(info, STIVALE2_STRUCT_TAG_PMRS_ID));
    REQUIRE(map, "Missing loader info struct %s (%016zx)", "protected memory ranges",
            STIVALE2_STRUCT_TAG_PMRS_ID);

    constexpr static const size_t kMaxPmrs{4};
    static KUSH_ALIGNED(64) uint8_t gVmObjectAllocBuf[kMaxPmrs][sizeof(Kernel::Vm::ContiguousPhysRegion)];
    static size_t gVmObjectAllocNextFree{0};

    for(size_t i = 0; i < pmrs->entries; i++) {
        REQUIRE(i < kMaxPmrs, "exceeded max PMRs");
        const auto &pmr = pmrs->pmrs[i];

        // translate the physical address and mode
        const uint64_t phys = kernelPhysBase + (pmr.base - kernelVirtBase);
        Kernel::Vm::Mode mode{Kernel::Vm::Mode::None};

        if(pmr.permissions & STIVALE2_PMR_EXECUTABLE) {
            mode |= Kernel::Vm::Mode::KernelExec;
        }
        if(pmr.permissions & STIVALE2_PMR_READABLE) {
            mode |= Kernel::Vm::Mode::KernelRead;
        }
        if(pmr.permissions & STIVALE2_PMR_WRITABLE) {
            REQUIRE(!TestFlags(mode & Kernel::Vm::Mode::KernelExec),
                    "refusing to add PMR %zu (virt %016zx phys %016zx len %zx mode %02zx) as WX",
                    i, pmr.base, phys, pmr.length, pmr.permissions);
            mode |= Kernel::Vm::Mode::KernelWrite;
        }

        // create the VM object and add it
        const auto vmIdx = gVmObjectAllocNextFree++;
        auto vm = reinterpret_cast<Kernel::Vm::ContiguousPhysRegion *>(gVmObjectAllocBuf[vmIdx]);
        new (vm) Kernel::Vm::ContiguousPhysRegion(phys, pmr.length, mode);

        err = map->add(pmr.base, vm);
        REQUIRE(!err, "failed to map PMR %zu (virt %016zx phys %016zx len %zx mode %02zx): %d",
                i, pmr.base, phys, pmr.length, pmr.permissions, err);
    }
}
