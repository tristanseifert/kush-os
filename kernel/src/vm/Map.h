#ifndef KERNEL_VM_MAP_H
#define KERNEL_VM_MAP_H

#include <stdint.h>

#include <arch/PTEHandler.h>

namespace vm {
class Mapper;

/**
 * Maps represent the virtual memory environment for a particular process. They hold references to
 * all mappe sections, and handle generating the architecture-specific translation tables.
 */
class Map {
    friend class Mapper;

    public:
        /// modifier flags for mappings, defining its protection level
        enum class MapMode: uint32_t {
            /// Allow reading from the page
            READ                = (1 << 0),
            /// Allow writing to the page
            WRITE               = (1 << 1),
            /// Allow executing from the page
            EXECUTE             = (1 << 2),

            /// User mode may access the page
            ACCESS_USER         = (1 << 8),

            /// Read + execute for kernel only
            kKernelExec         = (READ | EXECUTE),
            /// Read + write for kernel only
            kKernelRW           = (READ | WRITE),
        };

    public:
        Map() : Map(true) {};
        ~Map();

        void activate();

        int add(const uint64_t physAddr, const uintptr_t length, const uintptr_t vmAddr, 
                const MapMode mode);

    private:
        Map(const bool copyKernelMaps);

    private:
        /// architecture-specific page table handling
        arch::vm::PTEHandler table;
};

inline Map::MapMode operator &(Map::MapMode a, Map::MapMode b) {
    return static_cast<Map::MapMode>(static_cast<int>(a) & static_cast<int>(b));
};
}

#endif
