#ifndef KERNEL_VM_MAP_H
#define KERNEL_VM_MAP_H

#include "MapTree.h"
#include "MapEntry.h"

#include <stdint.h>

#include <bitflags.h>
#include <arch/rwlock.h>
#include <arch/PTEHandler.h>

#include <handle/Manager.h>
#include <runtime/Vector.h>
#include <runtime/Queue.h>

namespace vm {
class Mapper;

/// modifier flags for mappings, defining its protection level
ENUM_FLAGS(MapMode)
enum class MapMode {
    /// No flags set
    None                = 0,
    
    /// Allow reading from the page
    READ                = (1 << 0),
    /// Allow writing to the page
    WRITE               = (1 << 1),
    /// Allow executing from the page
    EXECUTE             = (1 << 2),

    /// User mode may access the page
    ACCESS_USER         = (1 << 8),

    /// The page should be mapped globally, e.g. it does not change between processes.
    GLOBAL              = (1 << 12),
    /// The page is not cached.
    CACHE_DISABLE       = (1 << 13),

    /// Read + execute for kernel text
    kKernelExec         = (READ | EXECUTE | GLOBAL),
    /// Read + write for kernel only
    kKernelRW           = (READ | WRITE),
    /// Read for kernel only
    kKernelRead         = (READ),

    /// Userspace text segments
    kUserExec           = (READ | EXECUTE | ACCESS_USER),
    /// Userspace read-only
    kUserRead           = (READ | ACCESS_USER),

    /// Device memory; bypass caches
    MMIO                = (CACHE_DISABLE),
};


/**
 * Maps represent the virtual memory environment for a particular process. They hold references to
 * all mappe sections, and handle generating the architecture-specific translation tables.
 */
class Map {
    friend class Mapper;

    public:
        /// Allocates a new VM map.
        static Map *alloc();
        /// Releases a previously allocated mapping struct.
        static void free(Map *);

    public:
        Map(const bool copyKernelMaps);
        ~Map();

        const bool isActive() const;
        void activate();

        int add(MapEntry *entry, sched::Task *task, const uintptr_t base = 0,
                const vm::MappingFlags flagMask = vm::MappingFlags::None,
                const uintptr_t searchStart = kVmSearchBase,
                const uintptr_t searchEnd = kVmMaxAddr);
        int remove(MapEntry *entry, sched::Task *);
        const bool contains(MapEntry *entry);

        int add(const uint64_t physAddr, const uintptr_t length, const uintptr_t vmAddr, 
                const MapMode mode);
        int remove(const uintptr_t vmAddr, const uintptr_t length);

        /// Gets the physical address to which this virtual address is mapped.
        int get(const uintptr_t virtAddr, uint64_t &phys) {
            MapMode temp;
            return this->get(virtAddr, phys, temp);
        }
        /// Gets the physical address to which this virtual address is mapped, and its flags.
        int get(const uintptr_t virtAddr, uint64_t &phys, MapMode &mode);

        /// Page fault handler
        bool handlePagefault(const uintptr_t virtAddr, const bool present, const bool write);

        /// Tests whether the given map entry can be expanded in this map without causing conflicts
        bool canResize(MapEntry *entry, const uintptr_t base, const size_t oldSize,
                const size_t newSize);

        /// Searches mappings to find one containing the given address.
        bool findRegion(const uintptr_t virtAddr, Handle &outHandle, uintptr_t &outOffset);

        /// Returns the number of installed mappings.
        const size_t numMappings() const {
            return this->entries.size();
        }

        /// Returns the global kernel map
        static Map *kern();
        /// Returns the currently activated map
        static Map *current() {
            return gCurrentMap;
        }

    private:
        static Map *gCurrentMap;

        static void initAllocator();

#if defined(__i386__)
        /// VM address at which we test for free VM space
        constexpr static const uintptr_t kVmSearchBase = 0x10000000;
        /// maximum address that can be mapped
        constexpr static const uintptr_t kVmMaxAddr = 0xBF800000;
#elif defined(__amd64__)
        constexpr static const uintptr_t kVmSearchBase = 0x400000000000;
        constexpr static const uintptr_t kVmMaxAddr =    0x7E0000000000;
#endif

    private:
        friend class arch::vm::PTEHandler;

        /// protecting modifications of the table
        DECLARE_RWLOCK(lock);

        // VM map entries
        MapTree entries;
        /// architecture-specific page table handling
        arch::vm::PTEHandler table;
};

}

#endif
