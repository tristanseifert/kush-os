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

#include "Helpers.h"
#include "Arch/Gdt.h"
#include "Arch/Idt.h"
#include "Arch/Processor.h"
#include "Io/Console.h"
#include "Util/Backtrace.h"

using namespace Platform::Amd64Uefi;

static void InitPhysAllocator(struct stivale2_struct *info);
static void InitKernelVm();
static void PopulateKernelVm(struct stivale2_struct *info);

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

    InitKernelVm();
    PopulateKernelVm(loaderInfo);

    // create kernel VM object and map the kernel executable

    // initialize the kernel VM system and then switch to this map


    // jump to the kernel's entry point now
    Kernel::Start();
    // we should never get here…
    PANIC("Kernel entry point returned!");
}

/**
 * Initialize the physical memory allocator.
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
    REQUIRE(map, "Missing loader info struct %s (%016llx)", "phys mem map",
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
}

/**
 * Set up kernel VMM and allocate the kernel's virtual memory map.
 *
 * First, this initializes the kernel virtual memory manager.
 *
 * Then, it creates the first virtual memory map, in reserved storage space in the .data segment of
 * the kernel. It is then registered with the kernel VMM for later use.
 */
static void InitKernelVm() {
    // TODO: implement
}

/**
 * Populate the kernel virtual memory map
 *
 * Fill in the kernel's virtual memory map with the sections for the kernel executable, as well as
 * the physical map aperture which is used to access physical pages when building page tables.
 *
 * @param info Information structure provided by the bootloader
 */
static void PopulateKernelVm(struct stivale2_struct *info) {
    // TODO: implement
}
