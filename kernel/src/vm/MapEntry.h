#ifndef KERNEL_VM_MAPENTRY_H
#define KERNEL_VM_MAPENTRY_H

#include <stddef.h>
#include <stdint.h>
#include <bitflags.h>
#include <runtime/SmartPointers.h>
#include <runtime/RedBlackTree.h>
#include <runtime/List.h>
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
    /// Mapping is read/write
    RW                                  = (Read | Write),

    /// Memory mapped IO mode (caching disabled entirely)
    MMIO                                = (1 << 8),
    /// Write combining cache mode
    WriteCombine                        = (1 << 9),

    /// Whether the object is mapped copy-on-write in non-owner tasks
    CopyOnWrite                         = (1 << 16),

    /// Mask including all permission bits
    PermissionsMask                     = (Read | Write | Execute),
    /// Mask including all cacheability bits
    CacheabilityMask                    = (MMIO | WriteCombine),
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
        constexpr inline Handle getHandle() const {
            return this->handle;
        }
        /// Returns the length of the region
        inline uintptr_t getLength() const {
            uintptr_t temp;
            __atomic_load(&this->length, &temp, __ATOMIC_RELAXED);
            return temp;
        }
        /// Returns the flags defining this VM object's mappings (by default)
        inline MappingFlags getFlags() const {
            MappingFlags flags;
            __atomic_load(&this->flags, &flags, __ATOMIC_RELAXED);
            return flags;
        }

        /// whether we're backed by anonymous memory or not
        constexpr inline bool backedByAnonymousMem() const {
            return this->isAnon;
        }
        /// whether the object is copy on write or not
        inline bool isCoW() const {
            return TestFlags(this->getFlags() & MappingFlags::CopyOnWrite);
        }

        /// Updates the flags of the map. Only the RWX and cacheability flags are updated.
        [[nodiscard]] int updateFlags(const MappingFlags newFlags);
        /// Attempts to resize the VM object
        [[nodiscard]] int resize(const size_t newSize);

        /// Force all pages to be faulted in
        [[nodiscard]] int faultInAllPages();

        /**
         * Sets the owning task for the map.
         *
         * The owning task can modify the original pages (rather than the copy-on-write pages) and
         * can also resize the region.
         */
        void setOwner(const rt::SharedPtr<sched::Task> &newOwner) {
            RW_LOCK_WRITE_GUARD(this->lock);
            this->owner = newOwner;
        }
        constexpr inline rt::WeakPtr<sched::Task> &getOwner() {
            return this->owner;
        }

        /// Allocates a VM object backed by a region of contiguous physical pages
        static rt::SharedPtr<MapEntry> makePhys(const uint64_t physAddr, const size_t length,
                const MappingFlags flags, const bool kernel = false);
        /// Allocates an anonymous VM object
        static rt::SharedPtr<MapEntry> makeAnon(const size_t length, const MappingFlags flags,
                const bool kernel = false);
        /// Releases a VM object
        static void free(MapEntry *entry);

    private:
        /**
         * Tree node representing a single physical page backing some page of this mapping.
         */
        struct AnonInfoLeaf {
            /// offset of this page from start of region
            uintptr_t pageOff = 0;
            /// physical address of page
            uint64_t physAddr = 0;

            // tree stuff
            AnonInfoLeaf *left = nullptr;
            AnonInfoLeaf *right = nullptr;
            AnonInfoLeaf *parent = nullptr;

            rt::RBTNodeColor color = rt::RBTNodeColor::None;

            constexpr inline uintptr_t getKey() const {
                return this->pageOff;
            }
            constexpr inline rt::RBTNodeColor getColor() const {
                return this->color;
            }
            inline void setColor(const rt::RBTNodeColor newColor) {
                this->color = newColor;
            }

            AnonInfoLeaf() = default;
            AnonInfoLeaf(const uintptr_t offset, const uint64_t phys) : pageOff(offset),
                physAddr(phys) {}
        };

        /**
         * Information on a view that's added to a virtual memory map.
         */
        struct ViewInfo {
            rt::SharedPtr<sched::Task> task;
            uintptr_t base = 0;
            const MappingFlags flags = MappingFlags::None;

            ViewInfo() = default;
            ViewInfo(const rt::SharedPtr<sched::Task> &_task, const uintptr_t _base,
                    const MappingFlags _flags) : task(_task), base(_base), flags(_flags) {}
        };

    private:
        /// Attempt to handle a page fault for the virtual address
        bool handlePagefault(Map *map, const uintptr_t base, const uintptr_t offset, 
                const bool present, const bool write);

    private:
        static void initAllocator();

        /// Updates the flags of any pages that have already been mapped.
        void updateExistingMappingFlags();

        /// The entry was added to a mapping.
        void addedToMap(Map *, const rt::SharedPtr<sched::Task> &, const uintptr_t base,
                const MappingFlags flagsMask = MappingFlags::None);
        /// Maps all physical pages we've allocated (for anonymous mappings)
        void mapAnonPages(Map *, const uintptr_t, const MappingFlags, const bool);
        /// Directly maps the underlying physical page.
        void mapPhysMem(Map *, const uintptr_t, const MappingFlags, const bool);

        /// The entry was removed from a mapping.
        void removedFromMap(Map *, const rt::SharedPtr<sched::Task> &, const uintptr_t,
                const size_t);

        /// Faults in an anonymous memory page.
        void faultInPage(const uintptr_t base, const uintptr_t offset, Map *map);
        /// Frees a physical page and updates the caller's "pages owned" counter
        static void freePage(const AnonInfoLeaf &info);

    private:
        /// modification lock
        DECLARE_RWLOCK(lock);

        /// handle referencing this map entry
        Handle handle;
        /// allocated length (in bytes)
        size_t length = 0;

        /// flags for the mapping
        MappingFlags flags;

        /// whether the map entry belongs to the kernel
        bool isKernel = false;
        /// when set, this is an anonymous mapping and is backed by anonymous phys mem
        bool isAnon = false;
        /// if not an anonymous map, the physical address base
        uint64_t physBase = 0;

        /**
         * All physical memory pages owned by this map
         */
        rt::RedBlackTree<AnonInfoLeaf> pages;

        /**
         * Listing of all virtual memory maps that have a view into this entry.
         */
        rt::List<ViewInfo> mappedIn;

        /**
         * Task that originally created this mapping, and is considered as the "owner" of the
         * mapping.
         *
         * This is used primarily for copy-on-write mappings; the owning task will _not_ go through
         * the code path for creating copies of the original pages, but will rather modify the
         * underlying mapping.
         */
        rt::WeakPtr<sched::Task> owner;
};
}

#endif
