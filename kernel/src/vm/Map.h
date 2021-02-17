#ifndef KERNEL_VM_MAP_H
#define KERNEL_VM_MAP_H

#include <stdint.h>

#include <bitflags.h>
#include <arch/rwlock.h>
#include <arch/PTEHandler.h>

#include <runtime/Vector.h>

namespace vm {
class MapEntry;
class Mapper;

/// modifier flags for mappings, defining its protection level
ENUM_FLAGS(MapMode)
enum class MapMode {
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

        int add(MapEntry *entry, const uintptr_t base = 0);
        int remove(MapEntry *entry);
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

        /// Returns the global kernel map
        static Map *kern();
        /// Returns the currently activated map
        static Map *current() {
            return gCurrentMap;
        }

    private:
        static Map *gCurrentMap;

        static void initAllocator();

        /// VM address at which we test for free VM space
        constexpr static const uintptr_t kVmSearchBase = 0x10000000;
        /// maximum address that can be mapped
        constexpr static const uintptr_t kVmMaxAddr = 0xBF800000;

    private:
        /// protecting modifications of the table
        DECLARE_RWLOCK(lock);

        /// all VM map entries
        rt::Vector<MapEntry *> entries;
        /// architecture-specific page table handling
        arch::vm::PTEHandler table;
};

}

#endif
