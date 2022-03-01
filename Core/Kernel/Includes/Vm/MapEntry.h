#ifndef KERNEL_VM_MAPENTRY_H
#define KERNEL_VM_MAPENTRY_H

#include <stddef.h>
#include <stdint.h>

#include <Runtime/RefCountable.h>
#include <Vm/Manager.h>

namespace Kernel::Vm {
class Map;

/**
 * @brief Base VM object
 *
 * A map entry is responsible for a single contiguous region of virtual address space in a virtual
 * memory map. Entries may be shared between one or more maps.
 *
 * This is the base class for all VM objects, which implements some basic behavior, including the
 * reference counting. Concrete subclasses override various points of this implementation, to
 * extend its features. This base interface provides for applying the same protections to the pages
 * regardless of which map it is contained in.
 */
class MapEntry: public Runtime::RefCountable<MapEntry> {
    friend class Map;

    public:
        MapEntry(const size_t length, const Mode mode);

        /**
         * @brief Get the length of the map entry, in bytes.
         */
        virtual const size_t getLength() const {
            return this->length;
        }

        /**
         * @brief Get the current base access mode of the map entry.
         */
        virtual const Mode getAccessMode() const {
            return this->accessMode;
        }

        /**
         * @brief Get the access mode for pages in this map entry in the given map.
         *
         * @param map VM map to query this entry's protection status in
         */
        virtual const Mode getAccessMode(Map &map) {
            return this->accessMode;
        }

        /**
         * @brief Check whether the VM object is orphaned, e.g. not associated with any map.
         */
        virtual const bool isOrphaned() const {
            // TODO: implement
            return true;
        }

        /**
         * @brief Handle a page fault caused by a page that falls inside this map entry.
         *
         * The handler can decide to fault in a page (possibly blocking the thread until some
         * external event happens) or abort the access.
         *
         * @param map Address space that the fault occurred in
         * @param virtualAddr The virtual address that caused the fault
         * @param mode Type of access that caused the fault
         *
         * @return 0 to resume execution, any other code to propagate the page fault
         */
        virtual int handleFault(Map &map, const uintptr_t virtualAddr, const Mode mode) {
            return -1;
        }

    protected:
        /**
         * @brief Callback invoked when the map entry is added to a map.
         *
         * @param base Virtual base address
         * @param map VM map object
         * @param pt Page table backing the VM map
         */
        virtual void addedTo(const uintptr_t base, Map &map, Platform::PageTable &pt) = 0;

        ~MapEntry() = default;

    protected:
        /**
         * @brief Number of bytes occupied by this map entry in virtual address space, in bytes.
         *
         * This should always be a multiple of the platform page size.
         */
        size_t length;

        /**
         * @brief Access mode for the map entry.
         */
        Mode accessMode;
};
}

#endif

