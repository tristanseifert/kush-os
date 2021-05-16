#ifndef KERNEL_VM_MAPENTRY_H
#define KERNEL_VM_MAPENTRY_H

#include <stddef.h>
#include <stdint.h>
#include <bitflags.h>
#include <runtime/SmartPointers.h>
#include <runtime/List.h>
#include <runtime/Vector.h>
#include <handle/Manager.h>

#include <arch/rwlock.h>

namespace sched {
struct Task;
}

namespace vm {
class Map;
class Mapper;

/**
 * Flags for a VM object mapping
 */
ENUM_FLAGS(MappingFlags)
enum class MappingFlags {
    /// No set flags
    None                                = 0,

    /// The mapping is readable
    Read                                = (1 << 0),
    /// The mapping can be written
    Write                               = (1 << 1),
    /// The mapping can be executed from
    Execute                             = (1 << 2),

    /// Memory mapped IO mode (caching disabled entirely)
    MMIO                                = (1 << 7),

    /// Mask including all permission bits
    PermissionsMask                     = (Read | Write | Execute),
};

/*
 * Represents an allocation of virtual memory.
 *
 * This range may be backed by physical memory, device memory, or nothing at all. Pages can be
 * faulted in on demand.
 *
 * VM entry objects are reference counted, and may be present in multiple maps simultaneously; this
 * enables shared memory. When the last reference to the entry is removed, it's deallocated, and
 * all physical memory it held is deallocated as well.
 */
class MapEntry {
    friend class Map;
    friend class Mapper;

    private:

    public:
        // you prob shouldn't really use these
        MapEntry(const size_t length, const MappingFlags flags);
        ~MapEntry();

    public:
        /// Returns the handle for the object
        const Handle getHandle() const {
            return this->handle;
        }
        /// Returns the length of the region
        const uintptr_t getLength() const {
            uintptr_t temp;
            __atomic_load(&this->length, &temp, __ATOMIC_RELAXED);
            return temp;
        }
        /// Returns the flags defining this VM object's mappings (by default)
        const MappingFlags getFlags() const {
            MappingFlags flags;
            __atomic_load(&this->flags, &flags, __ATOMIC_RELAXED);
            return flags;
        }

        /// whether we're backed by anonymous memory or not
        inline const bool backedByAnonymousMem() const {
            return this->isAnon;
        }

        /// Updates the flags of the map. Only the RWX and cacheability flags are updated.
        int updateFlags(const MappingFlags newFlags);
        /// Attempts to resize the VM object
        int resize(const size_t newSize);

        /// Allocates a VM object backed by a region of contiguous physical pages
        static rt::SharedPtr<MapEntry> makePhys(const uint64_t physAddr, const size_t length,
                const MappingFlags flags, const bool kernel = false);
        /// Allocates an anonymous VM object
        static rt::SharedPtr<MapEntry> makeAnon(const size_t length, const MappingFlags flags,
                const bool kernel = false);
        /// Releases a VM object
        static void free(MapEntry *entry);

    private:
        /// Info on a physical page we own
        struct AnonPageInfo {
            /// offset of this page from start of region
            uintptr_t pageOff = 0;
            /// physical address of page
            uint64_t physAddr = 0;

            /// task that originally allocated this page
            rt::WeakPtr<sched::Task> task;
        };

    private:
        /// Attempt to handle a page fault for the virtual address
        bool handlePagefault(Map *map, const uintptr_t base, const uintptr_t offset, 
                const bool present, const bool write);

    private:
        static void initAllocator();

        /// The entry was added to a mapping.
        void addedToMap(Map *, const rt::SharedPtr<sched::Task> &, const uintptr_t base,
                const MappingFlags flagsMask = MappingFlags::None);
        /// Maps all physical pages we've allocated (for anonymous mappings)
        void mapAnonPages(Map *, const uintptr_t, const MappingFlags);
        /// Directly maps the underlying physical page.
        void mapPhysMem(Map *, const uintptr_t, const MappingFlags);

        /// The entry was removed from a mapping.
        void removedFromMap(Map *, const rt::SharedPtr<sched::Task> &, const uintptr_t,
                const size_t);

        /// Faults in an anonymous memory page.
        void faultInPage(const uintptr_t base, const uintptr_t offset, Map *map);
        /// Frees a physical page and updates the caller's "pages owned" counter
        void freePage(const uintptr_t page);

    private:
        /// modification lock
        DECLARE_RWLOCK(lock);

        /// handle referencing this map entry
        Handle handle;

        /// length (in bytes)
        size_t length = 0;

        /// physical pages we own
        rt::Vector<AnonPageInfo> physOwned;
        /// all maps that we exist in
        rt::Vector<Map *> maps;

        /// flags for the mapping
        MappingFlags flags;

        /// when set, this is an anonymous mapping and is backed by anonymous phys mem
        bool isAnon = false;
        /// if not an anonymous map, the physical address base
        uint64_t physBase = 0;

        /// whether the map entry belongs to the kernel
        bool isKernel = false;
};
}

#endif
