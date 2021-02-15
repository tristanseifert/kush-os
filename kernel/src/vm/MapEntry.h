#ifndef KERNEL_VM_MAPENTRY_H
#define KERNEL_VM_MAPENTRY_H

#include <stddef.h>
#include <stdint.h>
#include <bitflags.h>
#include <runtime/Vector.h>
#include <handle/Manager.h>

#include <arch/rwlock.h>

namespace vm {
class Map;
class Mapper;

/**
 * Flags for a VM object mapping
 */
ENUM_FLAGS(MappingFlags)
enum class MappingFlags {
    /// The mapping is readable
    Read                                = (1 << 0),
    /// The mapping can be written
    Write                               = (1 << 1),
    /// The mapping can be executed from
    Execute                             = (1 << 2),

    /// Memory mapped IO mode (caching disabled entirely)
    MMIO                                = (1 << 7),
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

    public:
        // you prob shouldn't really use these
        MapEntry(const uintptr_t base, const size_t length, const MappingFlags flags);
        ~MapEntry();

    public:
        /**
         * Increments the reference count.
         */
        auto retain() {
            __atomic_add_fetch(&this->refCount, 1, __ATOMIC_RELEASE);
            return this;
        }

        /**
         * Decrements the reference count. If we reach zero, the object is deleted.
         */
        MapEntry *release() {
            REQUIRE(this->refCount, "attempt to release %p with zero refcount", this);

            const auto count = __atomic_sub_fetch(&this->refCount, 1, __ATOMIC_RELEASE);
            if(!count) {
                free(this);
                return nullptr;
            }

            return this;
        }

        /**
         * Tests whether the given address is within the range.
         */
        bool contains(const uintptr_t address) const {
            return (address >= this->base) && (address < (this->base + this->length));
        }
        /**
         * Tests whether the given address range overlaps with this entry.
         */
        bool contains(const uintptr_t address, const size_t length) const {
            const auto x1 = this->base;
            const auto x2 = (this->base + this->length);
            const auto y1 = address;
            const auto y2 = (address + length);

            return (x1 >= y1 && x1 <= y2) ||
                   (x2 >= y1 && x2 <= y2) ||
                   (y1 >= x1 && y1 <= x2) ||
                   (y2 >= x1 && y2 <= x2);
        }
        /**
         * Tests whether the given mappings overlap.
         */
        inline bool intersects(const MapEntry *entry) {
            return this->contains(entry->base, entry->length);
        }

        /// Returns the handle for the object
        const Handle getHandle() const {
            return this->handle;
        }

        /// Attempts to resize the VM object
        int resize(const size_t newSize);

        /// Allocates a VM object backed by a region of contiguous physical pages
        static MapEntry *makePhys(const uint64_t physAddr, const uintptr_t address,
                const size_t length, const MappingFlags flags);
        /// Allocates an anonymous VM object
        static MapEntry *makeAnon(const uintptr_t address, const size_t length,
                const MappingFlags flags);
        /// Releases a VM object
        static void free(MapEntry *entry);

    private:
        /// Info on a physical page we own
        struct AnonPageInfo {
            /// offset of this page from start of region
            uintptr_t pageOff;
            /// physical address of page
            uint64_t physAddr;
        };

    private:
        /// Attempt to handle a page fault for the virtual address
        bool handlePagefault(const uintptr_t address, const bool present, const bool write);

    private:
        static void initAllocator();

        /// The entry was added to a mapping.
        void addedToMap(Map *);
        /// Maps all physical pages we've allocated (for anonymous mappings)
        void mapAnonPages(Map *);
        /// Directly maps the underlying physical page.
        void mapPhysMem(Map *);

        /// The entry was removed from a mapping.
        void removedFromMap(Map *);

        /// Faults in an anonymous memory page.
        void faultInPage(const uintptr_t address, Map *map);

    private:
        /// modification lock
        DECLARE_RWLOCK(lock);

        /// handle referencing this map entry
        Handle handle;

        /// number of references held to the entry
        uintptr_t refCount = 0;

        /// base virtual address
        uintptr_t base = 0;
        /// length (in bytes)
        size_t length = 0;

        /// physical pages we own
        rt::Vector<AnonPageInfo> physOwned;

        /// flags for the mapping
        MappingFlags flags;

        /// when set, this is an anonymous mapping and is backed by anonymous phys mem
        bool isAnon = false;
        /// if not an anonymous map, the physical address base
        uint64_t physBase = 0;
};
}

#endif
