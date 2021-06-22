#ifndef LIBDRIVER_DMA_DMABUFFER_H
#define LIBDRIVER_DMA_DMABUFFER_H

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace libdriver {
/**
 * Abstract base class for a buffer that can be decomposed into one or more extents of physical
 * memory, which can in turn be used as part of a DMA operation.
 */
class DmaBuffer {
    public:
        enum Errors: int {
            /// Failed to translate physical address
            PhysTranslationFailed               = -20000,
            /// Allocating physical memory failed
            AllocFailed                         = -20001,
            /// Mapping the memory region failed
            MapFailed                           = -20002,
            /// The sizes provided were invalid
            InvalidSize                         = -20003,
        };

        /**
         * Describes a single extent of the scatter/gather buffer, aka an individual contiguous
         * chunk of physical memory to be transfered.
         */
        struct Extent {
            /// Physical address of the extent
            uint64_t physAddr{0};
            /// Length of the extent, in bytes
            size_t length{0};

            constexpr uint64_t getPhysAddress() const {
                return this->physAddr;
            }
            constexpr size_t getSize() const {
                return this->length;
            }

            Extent(const uint64_t addr, const size_t size) : physAddr(addr), length(size) {}
        };

        virtual ~DmaBuffer() = default;

    public:
        /**
         * Returns a span that encompasses the entire buffer.
         */
        virtual explicit operator std::span<std::byte>() const {
            return std::span<std::byte>(static_cast<std::byte *>(this->data()), this->getSize());
        }

        /**
         * Returns a reference to all physical extents that make up this buffer.
         */
        virtual const std::vector<Extent> &getExtents() const = 0;

        /**
         * Returns the total size of the buffer.
         */
        virtual size_t getSize() const = 0;

        /**
         * Gets a reference to the underlying virtual memory of the buffer.
         */
        virtual void * _Nonnull data() const = 0;
};
}

#endif
