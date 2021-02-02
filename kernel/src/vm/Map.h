#ifndef KERNEL_VM_MAP_H
#define KERNEL_VM_MAP_H

#include <stdint.h>

#include <bitflags.h>
#include <arch/PTEHandler.h>

namespace vm {
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

    /// Read + execute for kernel text
    kKernelExec         = (READ | EXECUTE | GLOBAL),
    /// Read + write for kernel only
    kKernelRW           = (READ | WRITE),
};


/**
 * Maps represent the virtual memory environment for a particular process. They hold references to
 * all mappe sections, and handle generating the architecture-specific translation tables.
 */
class Map {
    friend class Mapper;

    public:
        Map() : Map(true) {};
        ~Map();

        const bool isActive() const;
        void activate();

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

    private:
        Map(const bool copyKernelMaps);

    private:
        /// architecture-specific page table handling
        arch::vm::PTEHandler table;
};

}

#endif
